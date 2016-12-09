/**
 * \file
 * \brief SDMA (System Direct Memory Access) RPC bindings.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <sdma/sdma_rpc.h>

errval_t sdma_rpc_send_and_receive(uintptr_t* args, void* send_handler,
        void* rcv_handler)
{
    struct sdma_rpc* rpc = (struct sdma_rpc*) args[0];
    // 1. Set send handler.
    CHECK("sdma_rpc_send_and_receive: lmp_chan_register_send",
            lmp_chan_register_send(&rpc->lc, rpc->ws,
                    MKCLOSURE(send_handler, args)));

    // 3. Set receive handler.
    CHECK("sdma_rpc_send_and_receive: lmp_chan_register_recv",
            lmp_chan_register_recv(&rpc->lc, rpc->ws,
                    MKCLOSURE(rcv_handler, args)));

    // 4. Block until channel is ready to send.
    CHECK("sdma_rpc_send_and_receive: event_dispatch send",
            event_dispatch(rpc->ws));

    // 5. Block until channel is ready to receive.
    CHECK("sdma_rpc_send_and_receive: event_dispatch receive",
            event_dispatch(rpc->ws));

    return SYS_ERR_OK;
}

errval_t sdma_rpc_init(struct sdma_rpc* rpc, struct waitset* ws)
{
    rpc->ws = ws;

    // 1. Get SDMA driver endpoint cap from Init.
    CHECK("sdma_rpc_init: aos_rpc_get_sdma_ep_cap",
            aos_rpc_get_sdma_ep_cap(get_init_rpc(), &cap_sdma_ep));

    // 2. Create local channel using SDMA driver as remote endpoint.
    CHECK("sdma_rpc_init: lmp_chan_accept",
            lmp_chan_accept(&rpc->lc, DEFAULT_LMP_BUF_WORDS, cap_sdma_ep));

    // 3. Marshal args.
    uintptr_t args = (uintptr_t) rpc;

    // 4. Allocate recv slot.
    CHECK("sdma_rpc_init: lmp_chan_alloc_recv_slot",
            lmp_chan_alloc_recv_slot(&rpc->lc));

    // 5. Send handshake request to SDMA driver and wait for ACK.
    CHECK("sdma_rpc_init: sdma_rpc_send_and_receive",
            sdma_rpc_send_and_receive(&args, sdma_rpc_handshake_send_handler,
                    sdma_rpc_handshake_recv_handler));

    // By now we've successfully established the underlying LMP channel for RPC.
    rpc->request_pending = false;

    return SYS_ERR_OK;
}

errval_t sdma_rpc_handshake_send_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;

    struct sdma_rpc *rpc = (struct sdma_rpc*) args[0];
    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                SDMA_RPC_HANDSHAKE);
        ++retries;
    } while (err_is_fail(err) && retries < 1);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Handshake retry limit exceeded");
    }

    return err;
}

errval_t sdma_rpc_handshake_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;

    struct sdma_rpc* rpc = (struct sdma_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;;
    struct capref cap;

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) sdma_rpc_handshake_recv_handler, args));
    }

    // We should have a response code.
    assert(msg.buf.msglen == 1);
    assert(SDMA_RPC_OK == msg.words[0]);

    rpc->lc.remote_cap = cap;

    // Return error from remote ops.
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "handshake receive handler");
    }
    return err;
}

errval_t sdma_rpc_memcpy(
		struct sdma_rpc* rpc,
		struct capref dst,
		struct capref src,
		size_t len)
{
	if (rpc->request_pending) {
		return SDMA_ERR_REQUEST_IN_PROGRESS;
	}

    uintptr_t args[4];
    args[0] = (uintptr_t) rpc;
    args[1] = (uintptr_t) SDMA_RPC_MEMCPY_SRC;
    args[2] = (uintptr_t) &src;
    args[3] = (uintptr_t) len;

    CHECK("sdma_rpc_memcpy: sdma_rpc_send_and_receive (src, len)",
            sdma_rpc_send_and_receive(args, sdma_rpc_memcpy_send_handler,
                    sdma_rpc_response_recv_handler));

    args[1] = (uintptr_t) SDMA_RPC_MEMCPY_DST;
    args[2] = (uintptr_t) &dst;

    CHECK("sdma_rpc_memcpy: sdma_rpc_send_and_receive (dst)",
            sdma_rpc_send_and_receive(args, sdma_rpc_memcpy_send_handler,
                    sdma_rpc_response_recv_handler));

    rpc->request_pending = true;

    // Set another receive handler, for when the remote memcpy has been
    // completed and the server acks back to inform us.
    CHECK("sdma_rpc_memcpy: lmp_chan_register_recv",
            lmp_chan_register_recv(&rpc->lc, rpc->ws,
                    MKCLOSURE((void*) sdma_rpc_response_recv_handler, &args)));

    return SYS_ERR_OK;
}

errval_t sdma_rpc_memcpy_send_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;

    struct sdma_rpc *rpc = (struct sdma_rpc*) args[0];
    size_t code = (size_t) args[1];
    struct capref* cap = (struct capref*) args[2];
    size_t len = (size_t) args[3];
    errval_t err;
    do {
        err = lmp_chan_send2(&rpc->lc, LMP_FLAG_SYNC, *cap, code, len);
    } while (err_is_fail(err));

    return err;
}

errval_t sdma_rpc_response_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;

    struct sdma_rpc* rpc = (struct sdma_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;;
    struct capref cap;

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) sdma_rpc_response_recv_handler, args));
    }

    // We should have a response code and an error.
    assert(msg.buf.msglen == 2);

    err = (errval_t) msg.words[1];
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "general-purpose response receive handler");
    }
    return err;
}

bool sdma_rpc_check_for_response(struct sdma_rpc* rpc)
{
	// This just peeks non-blockingly.
	errval_t err = check_for_event(rpc->ws);
	return err_is_ok(err);
}

errval_t sdma_rpc_wait_for_response(struct sdma_rpc* rpc)
{
    // Block until channel is ready to receive.
    CHECK("sdma_rpc_wait_for_response: event_dispatch receive",
            event_dispatch(rpc->ws));

    rpc->request_pending = false;

    return SYS_ERR_OK;
}
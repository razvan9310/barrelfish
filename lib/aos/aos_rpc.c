/**
 * \file
 * \brief Implementation of AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013 - 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <aos/aos_rpc.h>

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    return SYS_ERR_OK;
}

errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string)
{
    // TODO: implement functionality to send a string over the given channel
    // and wait for a response.
    return SYS_ERR_OK;
}

/**
 * \brief RAM cap request.
 */
errval_t aos_rpc_ram_send(void* arg)
{
    struct aos_rpc *rpc = (struct aos_rpc*) arg;
    CHECK("aos_rpc.c#aos_rpc_ram_send: lmp_chan_send0",
            lmp_chan_send2(&rpc->lc, LMP_FLAG_SYNC, *rpc->rcs.retcap,
                    AOS_RPC_MEMORY, rpc->rcs.req_bytes));
    return SYS_ERR_OK;
}

/**
 * \brief RAM cap response.
 */
errval_t aos_rpc_ram_recv(void* arg)
{
    struct aos_rpc* rpc = (struct aos_rpc*) arg;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, rpc->rcs.retcap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_ram_recv, arg));
    }

    // We should have received:
    // 1) RPC code
    // 2) RAM alloc error code
    // 3) actual returned size, if alloc succeeded.
    assert(msg.buf.msglen >= 2);

    // Will return error provided by server.
    err = (errval_t) msg.words[1];
    if (msg.words[0] == AOS_RPC_OK) {
        // Fill in true return size.
        *rpc->rcs.ret_bytes = msg.words[2];
    }

    // No need to reregister, we got our RAM.
    return err;
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t request_bytes,
                             struct capref *retcap, size_t *ret_bytes)
{
    // Fill in requested cap metadata.
    chan->rcs.retcap = retcap;
    chan->rcs.req_bytes = request_bytes;
    chan->rcs.ret_bytes = ret_bytes;

    // Allocate recv slot.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: lmp_chan_alloc_recv_slot",
            lmp_chan_alloc_recv_slot(&chan->lc));

    // Perform RPC. On success, this will make the provided capref pointer point
    // to the newly allocated memory region.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(chan, aos_rpc_ram_send,
                    aos_rpc_ram_recv));

    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc)
{
    // TODO implement functionality to request a character from
    // the serial driver.
    return SYS_ERR_OK;
}


errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c)
{
    // TODO implement functionality to send a character to the
    // serial port.
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid)
{
    // TODO (milestone 5): implement spawn new process rpc
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name)
{
    // TODO (milestone 5): implement name lookup for process given a process
    // id
    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count)
{
    // TODO (milestone 5): implement process id discovery
    return SYS_ERR_OK;
}

/**
 * \brief Initiate handshake by sending local cap to server.
 */
errval_t aos_rpc_handshake_send(void* arg)
{
    struct aos_rpc *rpc = (struct aos_rpc*) arg;
    CHECK("aos_rpc.c#aos_rpc_handshake_send: lmp_chan_send0",
            lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                    AOS_RPC_HANDSHAKE));
    return SYS_ERR_OK;
}

/**
 * \brief Finalize handshake by receiving ack from server.
 */
errval_t aos_rpc_handshake_recv(void* arg)
{
    struct aos_rpc* rpc = (struct aos_rpc*) arg;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;;
    struct capref cap;

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_handshake_recv, arg));
    }

    // This should be an ACK only.
    assert(msg.buf.msglen == 1);
    assert(msg.words[0] == AOS_RPC_OK);

    // No need to rereister here, as handshake is complete;
    return err;
}

/**
 * \brief General-purpose blocking RPC send-and-receive function.
 */
errval_t aos_rpc_send_and_receive(void** args, void* send_handler,
        void* rcv_handler)
{
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    // 1. Set send handler.
    debug_printf("aos_rpc.c: send_and_receive register_send\n");
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: lmp_chan_register_send",
            lmp_chan_register_send(&rpc->lc, rpc->ws,
                    MKCLOSURE(send_handler, args)));

    // 2. Set receive handler.
    debug_printf("aos_rpc.c: send_and_receive register_recv\n");
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: lmp_chan_register_recv",
            lmp_chan_register_recv(&rpc->lc, rpc->ws,
                    MKCLOSURE(rcv_handler, args)));

    // 3. Block until channel is ready to send.
    debug_printf("aos_rpc.c: send_and_receive dispatch send\n");
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: event_dispatch send",
            event_dispatch(rpc->ws));

    // 4. Block until channel is ready to receive.
    debug_printf("aos_rpc.c: send_and_receive dispatch recv\n");
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: event_dispatch receive",
            event_dispatch(rpc->ws));

    debug_printf("aos_rpc.c: send_and_receive register_recv OK\n");

    return SYS_ERR_OK;
}

errval_t aos_rpc_init(struct aos_rpc *rpc, struct waitset* ws)
{
    // 0. Assign waitset to use from now on.
    rpc->ws = ws;

    // 1. Create local channel using init as remote endpoint.
    CHECK("aos_rpc.c#aos_rpc_init: lmp_chan_accept",
            lmp_chan_accept(&rpc->lc, DEFAULT_LMP_BUF_WORDS, cap_initep));

    // 2. Send handshake request to init and wait for ACK.
    CHECK("aos_rpc.c#aos_rpc_init: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(rpc, aos_rpc_handshake_send,
                    aos_rpc_handshake_recv));

    // By now we've successfully established the underlying LMP channel for RPC.
    return SYS_ERR_OK;
}

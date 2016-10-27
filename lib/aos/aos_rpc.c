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

// defined at the end
uint32_t perf_measurement_get_counter(void);

/**
 * \brief Number handler.
 */
errval_t aos_rpc_send_number_send_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_rpc *rpc = (struct aos_rpc*) args[0];
    uintptr_t *number = (uintptr_t*) args[1];
    CHECK("aos_rpc.c#aos_rpc_send_number_handler: lmp_chan_send3",
            lmp_chan_send3(&rpc->lc, LMP_FLAG_SYNC, NULL_CAP,
                    AOS_RPC_NUMBER, 0, *number));

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_send_number_send: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

    return SYS_ERR_OK;
}

/**
 * \brief Number response.
 */
errval_t aos_rpc_send_number_recv_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_send_number_recv_handler, args));
    }

    assert(msg.buf.msglen == 1);

    assert(msg.words[0] == AOS_RPC_OK);
    // No need to reregister, we got our RAM.

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_send_number_recv: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

    return err;
}

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val)
{
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    uintptr_t* args = (uintptr_t*) malloc(2 * sizeof(uintptr_t));
    args[0] = (uintptr_t) ((struct aos_rpc*) malloc(sizeof(struct aos_rpc)));
    *((struct aos_rpc*) args[0]) = *chan;
    args[1] = (uintptr_t) ((uintptr_t*) malloc(sizeof(uintptr_t)));
    *((uintptr_t*) args[1]) = val;

    CHECK("aos_rpc.c#aos_rpc_send_number: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_send_number_send_handler,
                    aos_rpc_send_number_recv_handler));

    free((uintptr_t*) args[1]);
    free((struct aos_rpc*) args[0]);
    free(args);
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
errval_t aos_rpc_ram_send_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    // 1. aos_rpc
    struct aos_rpc *rpc = (struct aos_rpc*) args[0];
    // 2. request_bytes
    size_t* req_bytes = (size_t*) args[1];
    // 3. retcap
    struct capref* retcap = (struct capref*) args[2];
    // 4. ret_bytes is not used here.

    // 5. Perform send.
    CHECK("aos_rpc.c#aos_rpc_ram_send_handler: lmp_chan_send0",
            lmp_chan_send3(&rpc->lc, LMP_FLAG_SYNC, *retcap,
                    AOS_RPC_MEMORY, rpc->client_id, *req_bytes));

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_ram_send: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }
    return SYS_ERR_OK;
}

/**
 * \brief RAM cap response.
 */
errval_t aos_rpc_ram_recv_handler(void* void_args)
{
   // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    // 1. aos_rpc
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    // 2. retcap
    struct capref* retcap = (struct capref*) args[2];

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, retcap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_ram_recv_handler, args));
    }

    // We should have received:
    // 1) RPC code
    // 2) RAM alloc error code
    // 3) actual returned size, if alloc succeeded.
    assert(msg.buf.msglen >= 3);

    if (msg.words[0] == AOS_RPC_OK) {
        // Fill in returnd size.
        size_t* retsize = (size_t*) args[3];
        *retsize = msg.words[2];
    }

   // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_ram_recv: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

    // No need to reregister, we got our RAM.
    // Will return error provided by server.
    return (errval_t) msg.words[1];
}

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t request_bytes,
                             struct capref *retcap, size_t *ret_bytes)
{
    // Fill in args.
    // 1. aos_rpc
    // 2. request_bytes
    // 3. retcap
    // 4. ret_bytes
    uintptr_t* args = (uintptr_t*) malloc(4 * sizeof(uintptr_t));
    args[0] = (uintptr_t) ((struct aos_rpc*) malloc(sizeof(struct aos_rpc)));
    *((struct aos_rpc*) args[0]) = *chan;

    args[1] = (uintptr_t) ((size_t*) malloc(sizeof(size_t)));
    *((size_t*) args[1]) = request_bytes;

    // Unused -- will be filled in by RPC.
    args[2] = (uintptr_t) ((struct capref*) malloc(sizeof(struct capref)));
    args[3] = (uintptr_t) ((size_t*) malloc(sizeof(size_t)));

    // Allocate recv slot.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: lmp_chan_alloc_recv_slot",
            lmp_chan_alloc_recv_slot(&chan->lc));

    // Perform RPC. On success, this will make the provided capref pointer point
    // to the newly allocated memory region.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_ram_send_handler,
                    aos_rpc_ram_recv_handler));

    *retcap = *((struct capref*) args[2]);
    *ret_bytes = *((size_t*) args[3]);

    // Free args.
    free((size_t*) args[3]);
    free((struct capref*) args[2]);
    free((size_t*) args[1]);
    free((struct aos_rpc*) args[0]);
    free(args);

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

    uintptr_t* args = (uintptr_t*) malloc(2 * sizeof(uintptr_t));;
    args[0] = (uintptr_t) ((struct aos_rpc*) malloc(sizeof(struct aos_rpc)));
    *((struct aos_rpc*) args[0]) = *chan;

    args[1] = (uintptr_t) ((char*) malloc(sizeof(char)));
    *((char*) args[1]) = c;

    CHECK("aos_rpc.c#aos_rpc_serial_putchar: aos_rpc_send_and_receive",
           aos_rpc_send_and_receive(args, aos_rpc_putchar_send_handler,
                   aos_rpc_putchar_recv_handler));

    free((char*) args[1]);
    free((struct aos_rpc*) args[0]);
    free(args);

    return SYS_ERR_OK;
}

errval_t aos_rpc_putchar_send_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

   struct aos_rpc* rpc = (struct aos_rpc*) args[0];
   char* to_put = (char*) args[1];

   CHECK("aos_rpc.c#aos_rpc_putchar_send_handler: lmp_chan_send0",
           lmp_chan_send3(&rpc->lc, LMP_FLAG_SYNC, NULL_CAP,
                   AOS_RPC_PUTCHAR, 0, *to_put));

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_putchar_send: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

   return SYS_ERR_OK;
}

errval_t aos_rpc_putchar_recv_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_putchar_recv_handler, args));
    }

    // This should be an ACK only.
    assert(msg.buf.msglen == 1);
    assert(msg.words[0] == AOS_RPC_OK);

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_putchar_recv: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

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
errval_t aos_rpc_handshake_send_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_rpc *rpc = (struct aos_rpc*) args[0];
    CHECK("aos_rpc.c#aos_rpc_handshake_send_handler: lmp_chan_send0",
            lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                    AOS_RPC_HANDSHAKE));

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_handshake_send: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

    return SYS_ERR_OK;
}

/**
 * \brief Finalize handshake by receiving ack from server.
 */
errval_t aos_rpc_handshake_recv_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;;
    struct capref cap;

    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_handshake_recv_handler, args));
    }

    // We should have an ACK with a client ID.
    assert(msg.buf.msglen == 2);
    assert(msg.words[0] == AOS_RPC_OK);
    rpc->client_id = (uint32_t) msg.words[1];

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_handshake_recv: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

    // No need to rereister here, as handshake is complete;
    return err;
}

/**
 * \brief General-purpose blocking RPC send-and-receive function.
 */
errval_t aos_rpc_send_and_receive(uintptr_t* args, void* send_handler,
        void* rcv_handler)
{
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    // 1. Set send handler.
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: lmp_chan_register_send",
            lmp_chan_register_send(&rpc->lc, rpc->ws,
                    MKCLOSURE(send_handler, args)));

    // 2. Set receive handler.
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: lmp_chan_register_recv",
            lmp_chan_register_recv(&rpc->lc, rpc->ws,
                    MKCLOSURE(rcv_handler, args)));

    // 3. Block until channel is ready to send.
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: event_dispatch send",
            event_dispatch(rpc->ws));

    // 4. Block until channel is ready to receive.
    CHECK("aos_rpc.c#aos_rpc_send_and_receive: event_dispatch receive",
            event_dispatch(rpc->ws));

    return SYS_ERR_OK;
}

errval_t aos_rpc_init(struct aos_rpc *rpc, struct waitset* ws)
{
    // 0. Assign waitset to use from now on.
    rpc->ws = ws;

    // 1. Create local channel using init as remote endpoint.
    CHECK("aos_rpc.c#aos_rpc_init: lmp_chan_accept",
            lmp_chan_accept(&rpc->lc, DEFAULT_LMP_BUF_WORDS, cap_initep));

    // 2. Marshal args.
    uintptr_t* args = (uintptr_t*) malloc(sizeof(uintptr_t));
    args[0] = (uintptr_t) rpc;
    args[0] = (uintptr_t) ((struct aos_rpc*) malloc(sizeof(struct aos_rpc)));
    *((struct aos_rpc*) args[0]) = *rpc;

    // 2. Send handshake request to init and wait for ACK.
    CHECK("aos_rpc.c#aos_rpc_init: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_handshake_send_handler,
                    aos_rpc_handshake_recv_handler));

    // 3. Free args.
    free((struct aos_rpc*) args[0]);
    free(args);

    // By now we've successfully established the underlying LMP channel for RPC.
    return SYS_ERR_OK;
}

uint32_t perf_measurement_get_counter(void) {
    uint32_t counter = 0;
    __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(counter));
    return counter;
}

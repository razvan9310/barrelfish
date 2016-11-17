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
#include <string.h>



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
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    coreid_t* core = (coreid_t*) args[1];
    uintptr_t* number = (uintptr_t*) args[2];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send3(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_RPC_NUMBER, *core, *number);
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

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

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val, coreid_t core)
{
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    uintptr_t args[3];
    args[0] = (uintptr_t) chan;
    args[1] = (uintptr_t) &core;
    args[2] = (uintptr_t) &val;

    CHECK("aos_rpc.c#aos_rpc_send_number: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_send_number_send_handler,
                    aos_rpc_send_number_recv_handler));

    return SYS_ERR_OK;
}

/**
 * \brief String handler.
 */
errval_t aos_rpc_send_string_send_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_rpc*   rpc = (struct aos_rpc*) args[0];
    //coreid_t* core = (coreid_t*) args[1];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send9(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_RPC_STRING,
                *((uintptr_t*) args[1]), *((uintptr_t*) args[8]),
                *((uintptr_t*) args[2]), *((uintptr_t*) args[3]), 
                *((uintptr_t*) args[4]), *((uintptr_t*) args[5]),
                *((uintptr_t*) args[6]), *((uintptr_t*) args[7]));
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;
}

/**
 * \brief String response.
 */
errval_t aos_rpc_send_string_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_send_string_recv_handler, args));
    }

    assert(msg.buf.msglen == 1);

    assert(msg.words[0] == AOS_RPC_OK);
    // No need to reregister, we got our RAM.
    return err;
}

errval_t aos_rpc_send_string(struct aos_rpc* chan, const char* string,
        coreid_t core)
{
    // break string into pieces
    uintptr_t* args = (uintptr_t*) malloc(9 * sizeof(uintptr_t));
    args[0] = (uintptr_t) ((struct aos_rpc*) malloc(sizeof(struct aos_rpc)));
    *((struct aos_rpc*) args[0]) = *chan;

    args[1] = (uintptr_t) ((coreid_t*) malloc(sizeof(coreid_t)));
    *((coreid_t*) args[1]) = core;

    uint32_t len = strlen(string);
    uint32_t rem_len = len;
    char words[24];
    char buf[4];
    for(int i=0; i<=(len/24); i++) {

        args[8] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));
        *((uint32_t*) args[8]) = rem_len;
        if(rem_len > 24) {
            memcpy(words, string + i*24, 24);
            for(int j=0; j<6; j++) {
                memcpy(buf, words+(j*4), 4);
                args[j+2] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
                *((char*) args[j+2]) = *buf;
                *((char*) args[j+2] + 1) = *(buf + 1);
                *((char*) args[j+2] + 2) = *(buf + 2);
                *((char*) args[j+2] + 3) = *(buf + 3);
            }
            rem_len -= 24;
        }else {
            memcpy(words, string + i*24, rem_len);
            rem_len = 24;
            for(int k=len; k<rem_len; k++) words[k] = 0;
            
            for(int j=0; j<(rem_len/4); j++) {
                memcpy(buf, words+(j*4), 4);
                //debug_printf("%%%%%%%% %c %c %c %c",buf[j], buf[j+2], buf[j+2], buf[j+3]);
                args[j+2] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
                *((char*) args[j+2]) = *buf;
                *((char*) args[j+2] + 1) = *(buf + 1);
                *((char*) args[j+2] + 2) = *(buf + 2);
                *((char*) args[j+2] + 3) = *(buf + 3);
                //printf("%s\n", args[j+1]);
            }
        }
        CHECK("aos_rpc.c#aos_rpc_send_string: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_send_string_send_handler,
                    aos_rpc_send_string_recv_handler));
    }
    //TODO: Free args

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

    // 5. Perform send.
    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send2(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                    AOS_RPC_MEMORY, *req_bytes);
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

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
    uintptr_t args[4];
    args[0] = (uintptr_t) chan;
    args[1] = (uintptr_t) &request_bytes;
    args[2] = (uintptr_t) retcap;
    args[3] = (uintptr_t) ret_bytes;

    // Allocate recv slot.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: lmp_chan_alloc_recv_slot",
            lmp_chan_alloc_recv_slot(&chan->lc));

    // Perform RPC. On success, this will make the provided capref pointer point
    // to the newly allocated memory region.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_ram_send_handler,
                    aos_rpc_ram_recv_handler));

    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar_send_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    char* to_get = (char*) args[1];

    lmp_chan_send2(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                   AOS_RPC_GETCHAR, *to_get);

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_getchar_send: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

   return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar_recv_handler(void* void_args)
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
                MKCLOSURE((void*) aos_rpc_serial_getchar_recv_handler, args));
    }

    printf("&&&&& %c\n", msg.words[2]);
    // This should be an ACK only.
    assert(msg.buf.msglen == 1);
    assert(msg.words[0] == AOS_RPC_OK);

    // N. get new cycle counter value, show result
    uint32_t cycle_counter_end = perf_measurement_get_counter();
    if (cycle_counter_end > cycle_counter_begin) {  // otherwise it overflowed and doesn't make much sense
        debug_printf(" *** performance measurement: aos_rpc_getchar_recv: %u cycles\n", cycle_counter_end - cycle_counter_begin);
    }

    return SYS_ERR_OK;
}

errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc)
{
    // TODO implement functionality to request a character from
    // the serial driver.
    uintptr_t args[2];
    args[0] = (uintptr_t) (chan);
    args[1] = (uintptr_t) (retc);

    CHECK("aos_rpc.c#aos_rpc_serial_putchar: aos_rpc_send_and_receive",
           aos_rpc_send_and_receive(args, aos_rpc_serial_getchar_send_handler,
                   aos_rpc_serial_getchar_recv_handler));

    return SYS_ERR_OK;
}


errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c)
{
    uintptr_t args[2];
    args[0] = (uintptr_t) chan;
    args[1] = (uintptr_t) &c;

    CHECK("aos_rpc.c#aos_rpc_serial_putchar: aos_rpc_send_and_receive",
           aos_rpc_send_and_receive(args, aos_rpc_putchar_send_handler,
                   aos_rpc_putchar_recv_handler));

    return SYS_ERR_OK;
}

errval_t aos_rpc_putchar_send_handler(void* void_args)
{
    // 0. get cycle counter value
    uint32_t cycle_counter_begin = perf_measurement_get_counter();

    uintptr_t* args = (uintptr_t*) void_args;

    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    char* to_put = (char*) args[1];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send2(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                   AOS_RPC_PUTCHAR, *to_put);
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

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

/**
 * \brief Process spawn request.
 */
errval_t aos_rpc_process_spawn_send_handler(void* void_args) 
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_rpc*   rpc = (struct aos_rpc*) args[0];
    //coreid_t* core = (coreid_t*) args[1];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send9(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_RPC_SPAWN,
                *((uintptr_t*) args[1]), *((uintptr_t*) args[8]),
                *((uintptr_t*) args[2]), *((uintptr_t*) args[3]), 
                *((uintptr_t*) args[4]), *((uintptr_t*) args[5]),
                *((uintptr_t*) args[6]), *((uintptr_t*) args[7]));
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;
}

/**
 * \brief Process spawn response.
 */
errval_t aos_rpc_process_spawn_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    //for implicit cap minitng
    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_process_spawn_recv_handler, args));
    }

//    assert(msg.buf.msglen == 2);

    //assert(msg.words[0] == AOS_RPC_OK);
    *((domainid_t*) args[2]) = msg.words[2];

    return (errval_t) msg.words[1];
}

errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid)
{
    // break name into pieces
    uintptr_t* args = (uintptr_t*) malloc(9 * sizeof(uintptr_t));
    args[0] = (uintptr_t) ((struct aos_rpc*) malloc(sizeof(struct aos_rpc)));
    *((struct aos_rpc*) args[0]) = *chan;

    args[1] = (uintptr_t) ((coreid_t*) malloc(sizeof(coreid_t)));
    *((coreid_t*) args[1]) = core;

    uint32_t len = strlen(name);
    uint32_t rem_len = len;
    char words[24];
    char buf[4];
    for(int i=0; i<=(len/24); i++) {

        args[8] = (uintptr_t) ((uint32_t*) malloc(sizeof(uint32_t)));
        *((uint32_t*) args[8]) = rem_len;
        if(rem_len > 24) {
            memcpy(words, name + i*24, 24);
            for(int j=0; j<6; j++) {
                memcpy(buf, words+(j*4), 4);
                args[j+2] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
                *((char*) args[j+2]) = *buf;
                *((char*) args[j+2] + 1) = *(buf + 1);
                *((char*) args[j+2] + 2) = *(buf + 2);
                *((char*) args[j+2] + 3) = *(buf + 3);
            }
            rem_len -= 24;
        }else {
            memcpy(words, name + i*24, rem_len);
            rem_len = 24;
            for(int k=len; k<rem_len; k++) words[k] = 0;
            
            for(int j=0; j<(rem_len/4); j++) {
                memcpy(buf, words+(j*4), 4);
                //debug_printf("%%%%%%%% %c %c %c %c",buf[j], buf[j+2], buf[j+2], buf[j+3]);
                args[j+2] = (uintptr_t) ((char*) malloc(4*sizeof(char)));
                *((char*) args[j+2]) = *buf;
                *((char*) args[j+2] + 1) = *(buf + 1);
                *((char*) args[j+2] + 2) = *(buf + 2);
                *((char*) args[j+2] + 3) = *(buf + 3);
                //printf("%s\n", args[j+1]);
            }
        }
        CHECK("aos_rpc.c#aos_rpc_process_spawn: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_process_spawn_send_handler,
                    aos_rpc_process_spawn_recv_handler));
    }
    debug_printf("Send and receive suceeded (spawn process)\n");
    *newpid = *(domainid_t*) args[2];

    debug_printf("Pid for process is %u\n", *newpid);

    //TODO: Free args

    return SYS_ERR_OK;
}

//TODO: Finnish implementing
errval_t aos_rpc_process_get_name_send_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    coreid_t* core = (coreid_t*) args[1];
    domainid_t* pid = (domainid_t*) args[2];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send3(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_RPC_GET_PNAME, *core, *pid);
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;
}

//TODO: Finnish implementing
errval_t aos_rpc_process_get_name_recv_handler(void* void_args)
{
    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    struct aos_rpc* init_rpc;

    init_rpc = get_init_rpc();

    //for implicit cap minitng
    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_process_get_name_recv_handler, args));
    }

    debug_printf("Program name is %u characters long\n", msg.words[0]);

    init_rpc->buffer = (char *) malloc(msg.words[0] * sizeof(char));
    size_t rem_length = msg.words[0];
    init_rpc->offset = 0;


    if (rem_length > 32) {
        for (int i = 0; i < 8; i++) {
            init_rpc->buffer[4 * i + init_rpc->offset] = (char) (msg.words[i + 1]); 
            init_rpc->buffer[4 * i + init_rpc->offset + 1] = (char) (msg.words[i + 1] >> 8); 
            init_rpc->buffer[4 * i + init_rpc->offset + 2] = (char) (msg.words[i + 1] >> 16); 
            init_rpc->buffer[4 * i + init_rpc->offset + 3] = (char) (msg.words[i + 1] >> 24); 
        }
        init_rpc->offset += 32;
    } else {

        for (int i = 0; i <= rem_length/4; i++) {

            init_rpc->buffer[4 * i + init_rpc->offset] = (char) (msg.words[i + 1]); 
            init_rpc->buffer[4 * i + init_rpc->offset + 1] = (char) (msg.words[i + 1] >> 8); 
            init_rpc->buffer[4 * i + init_rpc->offset + 2] = (char) (msg.words[i + 1] >> 16); 
            init_rpc->buffer[4 * i + init_rpc->offset + 3] = (char) (msg.words[i + 1] >> 24); 
        }
    }

    debug_printf("Program name is %s\n", init_rpc->buffer);


    return (errval_t) msg.words[1];
}

//TODO: Finish implementing
errval_t aos_rpc_process_get_name(struct aos_rpc* chan, domainid_t pid,
        coreid_t core, char** name)
{
    // TODO (milestone 5): implement name lookup for process given a process
    // id
    uintptr_t* args = (uintptr_t*) malloc(3 * sizeof(uintptr_t));
    args[0] = (uintptr_t) chan;
    args[1] = (uintptr_t) &core;
    args[2] = (uintptr_t) &pid;
    // Unused -- will be filled in by RPC.
    //args[2] = (uintptr_t) ((size_t*) malloc(sizeof(size_t)));

    // Perform RPC. On success, this will make the provided capref pointer point
    // to the newly allocated memory region.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_process_get_name_send_handler,
                    aos_rpc_process_get_name_recv_handler));

    *name = (char*) malloc(strlen(get_init_rpc()->buffer) * sizeof(char));
    strncpy(*name, get_init_rpc()->buffer, strlen(get_init_rpc()->buffer)); //(char*) malloc(args[2] * sizeof(char));


    // Free args.
    free((size_t*) args[2]);
    free((domainid_t*) args[1]);
    free((struct aos_rpc*) args[0]);
    free(args);

    return SYS_ERR_OK;
}

errval_t aos_rpc_process_get_process_list_send_handler(void* void_args) 
{
    //TODO: IMPLEMENT
    uintptr_t* args = (uintptr_t*) void_args;
    
    // TODO: implement functionality to send a number ofer the channel
    // given channel and wait until the ack gets returned.
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];

    errval_t err;
    size_t retries = 0;
    do {
        err = lmp_chan_send4(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_RPC_GET_PLIST, *((uintptr_t*) args[1]),
                *((uintptr_t*) args[2]), *((uintptr_t*) args[2]));
        ++retries;
    } while (err_is_fail(err) && retries < 5);
    if (retries == 5) {
        return err;
    }

    return SYS_ERR_OK;
}
errval_t aos_rpc_process_get_process_list_recv_handler(void* void_args) 
{
    //TODO: IMPLEMENT
    uintptr_t* args = (uintptr_t*) void_args;
    
    struct aos_rpc* rpc = (struct aos_rpc*) args[0];
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    struct aos_rpc* init_rpc;

    init_rpc = get_init_rpc();

    //for implicit cap minitng
    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // Reregister.
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void*) aos_rpc_process_get_name_recv_handler, args));
    }

    debug_printf("Program name is %u characters long\n", msg.words[0]);

    size_t rem_length = msg.words[1];
    init_rpc->ps_list = (domainid_t *) malloc(rem_length * sizeof(domainid_t));
    init_rpc->ps_offset = 0;


    if (rem_length > 7) {
        for (int i = 0; i < 7; i++) {
            init_rpc->ps_list[i+init_rpc->ps_offset] = (domainid_t) (msg.words[i + 2]); 
        }
        init_rpc->ps_offset += 7;
    } else {
        for (int i = 0; i < rem_length; i++) {
            init_rpc->ps_list[i+init_rpc->ps_offset] = (domainid_t) (msg.words[i + 2]); 
        }
        init_rpc->ps_offset += rem_length;
    }

    return (errval_t) msg.words[1];
}

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
        coreid_t core, domainid_t **pids, size_t *pid_count)
{
    // TODO (milestone 5): implement process id discovery
    /*CHECK("aos_rpc.c#aos_rpc_process_spawn: aos_rpc_send_and_receive",
    aos_rpc_send_and_receive(args, aos_rpc_process_spawn_send_handler,
            aos_rpc_process_spawn_recv_handler);*/

    uintptr_t args[4];
    args[0] = (uintptr_t) (chan);
    args[1] = (uintptr_t) &core;
    args[2] = (uintptr_t) (pids);
    args[3] = (uintptr_t) (pid_count);

    // Perform RPC. On success, this will make the provided capref pointer point
    // to the newly allocated memory region.
    CHECK("aos_rpc.c#aos_rpc_get_ram_cap: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(args, aos_rpc_process_get_process_list_send_handler,
                    aos_rpc_process_get_process_list_recv_handler));


    *pids = (domainid_t*) malloc(get_init_rpc()->ps_offset * sizeof(domainid_t));
    memcpy(*pids, get_init_rpc()->ps_list, get_init_rpc()->ps_offset); //(char*) malloc(args[2] * sizeof(char));

    *pid_count = get_init_rpc()->ps_offset;

    return SYS_ERR_OK;
}

errval_t aos_rpc_get_device_cap(struct aos_rpc *rpc,
                                lpaddr_t paddr, size_t bytes,
                                struct capref *frame)
{
    return LIB_ERR_NOT_IMPLEMENTED;
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
    errval_t err;
    do {
        err = lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, rpc->lc.local_cap,
                AOS_RPC_HANDSHAKE);
    } while (err_is_fail(err));

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

    // We should have an ACK.
    assert(msg.buf.msglen == 1);
    assert(msg.words[0] == AOS_RPC_OK);

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
    debug_printf("aos_rpc_init: LOCAL CAP HAS SLOT %d\n", rpc->lc.local_cap.slot);

    // 2. Marshal args.
    uintptr_t args = (uintptr_t) rpc;

    // 2. Send handshake request to init and wait for ACK.
    CHECK("aos_rpc.c#aos_rpc_init: aos_rpc_send_and_receive",
            aos_rpc_send_and_receive(&args, aos_rpc_handshake_send_handler,
                    aos_rpc_handshake_recv_handler));

    // By now we've successfully established the underlying LMP channel for RPC.
    return SYS_ERR_OK;
}

uint32_t perf_measurement_get_counter(void) {
    uint32_t counter = 0;
    __asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(counter));
    return counter;
}
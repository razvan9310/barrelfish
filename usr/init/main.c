/**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/morecore.h>
#include <aos/paging.h>

#include <mm/mm.h>
#include "mem_alloc.h"
#include <spawn/spawn.h>

coreid_t my_core_id;
struct bootinfo *bi;

errval_t recv_handler(void* arg);
errval_t parent_send_handshake(void* arg);
errval_t parent_send_memory(void* arg);

errval_t recv_handler(void *arg)
{
    struct aos_rpc* rpc = (struct aos_rpc*) arg;
    struct lmp_recv_msg msg;
    // printf("msg buflen %d\n", msg.buf.buflen);
    struct capref cap;
    errval_t err = lmp_chan_recv(&rpc->lc, &msg, &cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        // reregister
        lmp_chan_register_recv(&rpc->lc, rpc->ws,
                MKCLOSURE((void *)recv_handler, arg));
    }

    debug_printf("main.c: got message of size %u\n", msg.buf.msglen);
    if (msg.buf.msglen > 0) {
        void* response;
        debug_printf("init.c: msg buflen %zu\n", msg.buf.msglen);
        debug_printf("init.c: msg->words[0] = %d\n", msg.words[0]);
        switch (msg.words[0]) {
            case AOS_RPC_HANDSHAKE:
                rpc->lc.remote_cap = cap;
                response = (void*) parent_send_handshake;
                break;
            case AOS_RPC_MEMORY:
                *rpc->rcs.retcap = cap;
                rpc->rcs.req_bytes = msg.words[1];
                response = (void*) parent_send_memory;
                break;
            default:
                return 1;  // TODO: More meaning plz
        }

        CHECK("lmp_chan_register_send parent",
                lmp_chan_register_send(&rpc->lc, rpc->ws,
                        MKCLOSURE(response, arg)));  
    }

    // Reregister.
    CHECK("Create Slot", lmp_chan_alloc_recv_slot(&rpc->lc));
    lmp_chan_register_recv(&rpc->lc, rpc->ws,
            MKCLOSURE((void *)recv_handler, arg));

    return err;
}

errval_t parent_send_handshake(void *arg)
{
    struct aos_rpc* rpc = (struct aos_rpc*) arg;
    CHECK("lmp_chan_send handshake",
            lmp_chan_send1(&rpc->lc, LMP_FLAG_SYNC, NULL_CAP, AOS_RPC_OK));
    return SYS_ERR_OK;
}

errval_t parent_send_memory(void* arg)
{
    struct aos_rpc* rpc = (struct aos_rpc*) arg;
    assert(rpc->rcs.retcap != NULL);
    assert(rpc->rcs.ret_bytes != NULL);

    errval_t err = frame_alloc(rpc->rcs.retcap, rpc->rcs.req_bytes,
            rpc->rcs.ret_bytes);
    size_t code = err_is_fail(err) ? AOS_RPC_FAILED : AOS_RPC_OK;
    CHECK("lmp_chan_send memory",
            lmp_chan_send3(&rpc->lc, LMP_FLAG_SYNC, *rpc->rcs.retcap,
                code, (uintptr_t) err, *rpc->rcs.ret_bytes));
    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    errval_t err;

    /* Set the core id in the disp_priv struct */
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID " invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
       printf(" %s", argv[i]);
    }
    printf("\n");

    /* First argument contains the bootinfo location, if it's not set */
    bi = (struct bootinfo*)strtol(argv[1], NULL, 10);
    if (!bi) {
        assert(my_core_id > 0);
    }

    /* Initialize paging. */
    // err = paging_init();
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "paging_init");
    // }

    /* Initialize the default slot allocator. */
    // err = slot_alloc_init();
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "slot_alloc_init");
    // }

    err = initialize_ram_alloc();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    CHECK("Retype selfep from dispatcher", cap_retype(cap_selfep, cap_dispatcher, 0, ObjType_EndPoint, 0, 1));

    struct aos_rpc* rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    rpc->ws = get_default_waitset();
    // TODO: Find a nicer way...
    struct capref dummy_cap;
    size_t dummy_size;
    rpc->rcs.retcap = &dummy_cap;
    rpc->rcs.ret_bytes = &dummy_size;

    CHECK("Create channel for parent", lmp_chan_accept(&rpc->lc, DEFAULT_LMP_BUF_WORDS, NULL_CAP));

    CHECK("Create Slot", lmp_chan_alloc_recv_slot(&rpc->lc));
    CHECK("COpy to initep", cap_copy(cap_initep, rpc->lc.local_cap));

    CHECK("lmp_chan_register_recv child",
            lmp_chan_register_recv(&rpc->lc, rpc->ws,
                    MKCLOSURE((void*) recv_handler, rpc)));

    // // ALLOCATE A LOT OF MEMORY TROLOLOLOLO.
    // struct capref frame;
    // size_t retsize;
    // err = frame_alloc(&frame, 900 * 1024 * 1024, &retsize);
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "PANIC FRAME ALLOC 64 MB");
    // } else {
    //     debug_printf("main.c: was given frame size %u\n", retsize);
    // }
    // void* buf;
    // err = paging_map_frame_attr(get_current_paging_state(),
    //     &buf, retsize, frame,
    //     VREGION_FLAGS_READ_WRITE, NULL, NULL);
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "PANIC MAPPING 64 MB FRAME");
    // }

    // debug_printf("main.c: testing memory @ %p\n", buf);
    // char* cbuf = (char*)buf;
    // *cbuf = 'J';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // cbuf += 225 * 1024 * 1024;
    // *cbuf = 'K';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // cbuf += 225 * 1024 * 1024;
    // *cbuf = 'L';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // cbuf += 225 * 1024 * 1024;
    // *cbuf = 'M';
    // sys_debug_flush_cache();
    // printf("%c\n", *cbuf);

    // spawn a few helloz
    spawn_load_by_name("hello", (struct spawninfo*) malloc(sizeof(struct spawninfo)));
    // spawn_load_by_name("byebye", (struct spawninfo*) malloc(sizeof(struct spawninfo)));
    //spawn_load_by_name("hello", (struct spawninfo*) malloc(sizeof(struct spawninfo)));
    //spawn_load_by_name("byebye", (struct spawninfo*) malloc(sizeof(struct spawninfo)));


    debug_printf("Message handler loop\n");
    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return EXIT_SUCCESS;
}

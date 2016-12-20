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
#include <aos/waitset.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <mm/mm.h>
#include <aos/inthandler.h>
#include <spawn/spawn.h>
#include <urpc/urpc.h>

#include "coreboot.h"
#include "mem_alloc.h"
#include "scheduler.h"
#include "rpc_server.h"

coreid_t my_core_id;
struct bootinfo *bi;

static char c;
void terminal_read_handler(void *params);
void terminal_read_handler(void *params)
{
    debug_printf("I am in terminal_read_handler!!");
    sys_getchar(&c);

    debug_printf("%c\n", c);
    debug_printf("We got an interrupt for a character\n");
}


// static void pesudo_task(void)
// {
//     debug_printf("Hello World\n");
// }

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

    genpaddr_t remaining_mem_base = 0;
    gensize_t remaining_mem_size = 0;
    if (my_core_id == 0) {
        err = initialize_ram_alloc(&remaining_mem_base, &remaining_mem_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err, "initialize_ram_alloc");
        }
    }

    void* urpc_buf;
    CHECK("mapping URPC frame into vspace",
            map_urpc_frame_to_vspace(&urpc_buf, my_core_id));

    write_to_urpc(urpc_buf, remaining_mem_base, remaining_mem_size, bi,
            my_core_id);
    CHECK("forging RAM cap & retrieving bi from URPC frame",
            read_from_urpc(urpc_buf, &bi, my_core_id));
    CHECK("start core 1", start_core(1, my_core_id, bi));

    if (my_core_id == 1) {
        err = initialize_ram_alloc(&remaining_mem_base, &remaining_mem_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err, "initialize_ram_alloc");
        }

        CHECK("reading modules from URPC",
                read_modules(urpc_buf, bi, my_core_id));
    }
    // Initialize URPC for subsequent inter-core communication attempts.
    urpc_init(urpc_buf, my_core_id);

    struct lmp_chan* lc = (struct lmp_chan*) malloc(sizeof(struct lmp_chan));
    CHECK("Create channel for parent", lmp_chan_accept(lc, DEFAULT_LMP_BUF_WORDS, NULL_CAP));

    CHECK("Create Slot", lmp_chan_alloc_recv_slot(lc));
    CHECK("Copy to initep", cap_copy(cap_initep, lc->local_cap));

    struct scheduler sc;
    scheduler_init(&sc, my_core_id, urpc_buf);
    if (my_core_id == 0) {
        CHECK("spawning nameserver",
                spawn_load_by_name("nameserver",
                        (struct spawninfo*) malloc(sizeof(struct spawninfo)),
                        my_core_id));
        
        // Busy wait on the waitset to make sure we've shaken hands with the
        // Nameserver.
        void* arg = malloc(ROUND_UP(sizeof(struct lmp_chan*), 4)
                + ROUND_UP(sizeof(struct client_state*), 4));
        void* arg_cpy = arg;
        *((struct lmp_chan**) arg) = lc;
        arg = (void*) ROUND_UP((uintptr_t) arg + sizeof(struct lmp_chan*), 4);
        *((struct client_state**) arg) = sc.local_clients;

        debug_printf("Waiting for Nameserver to hanshake...\n");
        CHECK("lmp_chan_register_recv",
                lmp_chan_register_recv(lc, get_default_waitset(),
                        MKCLOSURE((void*) local_recv_once_handler, arg_cpy)));
        for (size_t j = 0; j < 2; ++j) {
            event_dispatch(get_default_waitset());
            debug_printf("OK! :)\n");
        }
        CHECK("creating slot for subsequent LMP", lmp_chan_alloc_recv_slot(lc));

        CHECK("spawning sdma",
                spawn_load_by_name(
                        "sdma",
                        (struct spawninfo*) malloc(sizeof(struct spawninfo)),
                        my_core_id));
        size_t i = 0;
        while(i<1000000) {
            i++;
        }
        CHECK("spawning net",
                spawn_load_by_name("net",
                        (struct spawninfo*) malloc(sizeof(struct spawninfo)), my_core_id));

        add_process_ps_list("init");
        add_process_ps_list("nameserver");
        add_process_ps_list("sdma");
        add_process_ps_list("net");
    } else {
        CHECK("spawning bash",
                spawn_load_by_name("bash",
                        (struct spawninfo*) malloc(sizeof(struct spawninfo)), my_core_id));

        add_process_ps_list("init");
        add_process_ps_list("bash");
    }

    if (my_core_id == 1) { // let's give something to do to core 1 too :D
    }

    debug_printf("Message handler loop\n");
    scheduler_start(&sc, lc);
    // // Hang around
    // struct waitset *default_ws = get_default_waitset();
    // while (true) {
    //     err = event_dispatch(default_ws);
    //     if (err_is_fail(err)) {
    //         DEBUG_ERR(err, "in event_dispatch");
    //         abort();
    //     }
    // }

    return EXIT_SUCCESS;
}

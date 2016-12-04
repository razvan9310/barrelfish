/**
 * \file
 * \brief Second hello world application
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */


#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>

int main(int argc, char *argv[])
{
    debug_printf("Printing to terminal driver on core 0...\n");
    printf("Goodbye :( (this is printed by byebye)\n");

    // debug_printf("Memory operations w/ core 0...\n");
    // char* c = (char*) malloc(120 * 1024 * 1024);
    // *c = 'B';
    // sys_debug_flush_cache();
    // *(c + 40 * 1024 * 1024) = 'Y';
    // sys_debug_flush_cache();
    // *(c + 80 * 1024 * 1024) = 'E';
    // sys_debug_flush_cache();

    // debug_printf("If you see \"BYE\", memory ops with core 0 succeeded: %c%c%c\n",
    //         *c, *(c + 40 * 1024 * 1024), *(c + 80 * 1024 * 1024));

    // debug_printf("RPC: Trying to spawn memeater process on core 0\n");

    // domainid_t pid;

    // errval_t err =  aos_rpc_process_spawn(get_init_rpc(), "memeater", 0, &pid);
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "could not spawn a process on core 0\n");
    //     return err;
    // }
    // debug_printf("Pid for memeater on core 0 is %u\n", pid);

    // char* name;
    // debug_printf("Trying to retireve process name for pid %u on core 1\n", pid);
    // err =  aos_rpc_process_get_name(get_init_rpc(), pid, 0, &name);
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "could not retrieve process name from core 0\n");
    //     return err;
    // }
    // debug_printf("!!!!!!Process with pid %u on core 0 is %s\n", pid, name);

    // domainid_t *pids;
    // size_t pid_count;

    // debug_printf("Trying to retireve list of all PIDs on core 0\n");
    // err =  aos_rpc_process_get_all_pids(get_init_rpc(), 0, &pids, &pid_count);
    // if (err_is_fail(err)) {
    //     DEBUG_ERR(err, "could not retrieve process list from core 0\n");
    //     return err;
    // }
    // debug_printf("!!!!!!Process pid_count on core 0 %u \n", pid_count);

    return 0;
}

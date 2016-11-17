/**
 * \file
 * \brief Hello world application
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
#include <aos/waitset.h>
#include <aos/paging.h>



int main(int argc, char *argv[])
{

	debug_printf("RPC: Trying to spawn bybye process\n");

	domainid_t pid;

    errval_t err =  aos_rpc_process_spawn(get_init_rpc(), "byebye", 0, &pid);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not spawn a process\n");
        return err;
    }
    debug_printf("Pid for byebye is %u\n", pid);

    char* name;
    debug_printf("Trying to retireve process name for pid %u\n", pid);
    err =  aos_rpc_process_get_name(get_init_rpc(), pid, 0, &name);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not spawn a process\n");
        return err;
    }
    debug_printf("!!!!!!Process with pid %u is %s\n", pid, name);

    domainid_t *pids;
    size_t pid_count;

    err =  aos_rpc_process_get_all_pids(get_init_rpc(), 0, &pids, &pid_count);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not spawn a process\n");
        return err;
    }
    debug_printf("!!!!!!Process with pid_count %u \n", pid_count);


 //    /*err =  aos_rpc_process_spawn(get_init_rpc(), "byebye", 0, &pid);
 //    if (err_is_fail(err)) {
 //        DEBUG_ERR(err, "could not spawn a process\n");
 //        return err;
 //    }
 //    debug_printf("byebye 2 has pid %u\n", pid);*/

    

    printf("Hello, world! from THE numbawan original AOS homie waddup yo you know I'm THE BAWS\n");
    
    char* c = (char*) malloc(128 * 1024 * 1024);
    *c = 'H';
    sys_debug_flush_cache();
    *(c + 64 * 1024 * 1024) = 'I';
    sys_debug_flush_cache();

    debug_printf("%c%c\n", *c, *(c + 64 * 1024 * 1024));

    // debug_printf("Dereferencing NULL\n");
    // int *null_ptr = NULL;    
    // *null_ptr = 13;
    // sys_debug_flush_cache();

    // debug_printf("Accessing non-heap memory at 0xFFFFAFFF\n");
    // volatile char* c = (volatile char*) 0xFFFFAFFF;
    // // *c = 'D';
    // // sys_debug_flush_cache();

    // debug_printf("Accessing (unmapped) memory at 1073807360\n");
    // c = (volatile char*) 1073807360u;
    // for (size_t i = 0; i < 2 * BASE_PAGE_SIZE; ++i) {
    // 	c[i] = i % 4 == 0 ? 'A' : (i % 4 == 1 ? 'B' : (i % 4 == 2 ? 'C' : 'D'));
    // }
    // sys_debug_flush_cache();
    // for (size_t i = 0; i < 2 * BASE_PAGE_SIZE; ++i) {
    // 	printf("%c", c[i]);
    // }
    // printf("\n");


    return 0;
}

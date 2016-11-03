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
    //debug_printf("byebye 1 has pid %u\n", pid);

    char* name;
    debug_printf("Trying to retireve process name for pid %u\n", pid);
    err =  aos_rpc_process_get_name(get_init_rpc(), pid, &name);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not spawn a process\n");
        return err;
    }
    debug_printf("!!!!!!Process with pid %u is %s\n", pid, name);


    /*err =  aos_rpc_process_spawn(get_init_rpc(), "byebye", 0, &pid);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not spawn a process\n");
        return err;
    }
    debug_printf("byebye 2 has pid %u\n", pid);*/

    

    debug_printf("Hello, world! from THE numbawan original AOS homie waddup yo you know I'm THE BAWS\n");
    
    return 0;
}

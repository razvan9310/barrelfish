/**
 * \file
 * \brief Nameserver application
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
#include <nameserver_rpc.h>

int main(int argc, char *argv[])
{
    debug_printf("Hello, this is service_a\n");
    
    //Open channel to nameserver
    static struct aos_ns_rpc new_aos_ns_rpc;
    new_aos_ns_rpc.it = 0;

    static struct waitset ns_ws;
    waitset_init(&ns_ws);

    CHECK("initializing new aos_ns_rpc_init",
            aos_ns_init(&new_aos_ns_rpc, &ns_ws));

    debug_printf("Trying to register a new service service_a\n");
	CHECK("Registering a new service service_a",
            register_service(&new_aos_ns_rpc, "service_a"));
	debug_printf("[DONE]\n");

	debug_printf("Enumerating services ...\n");
	size_t num;
	char** result = NULL;
	CHECK("Ennumerating services",
			enumerate(&new_aos_ns_rpc, &num, &result));
	
	debug_printf("Number of enumerated services is %d\n", num);
	for (int i = 0; i < num; i++) {
		debug_printf("Service %d: %s\n", i, result[i]);
		// debug_printf("Service %d\n", i);
	}
	debug_printf("[DONE]\n");

	volatile int a;
	for (a = 0; a < 100000000; a++) {
		;
	}

	domainid_t pid;
	errval_t err =  aos_rpc_process_spawn(get_init_rpc(), "ns_client", 0, &pid);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "could not spawn a client process\n");
        return err;
    }
    // debug_printf("Pid for ns_client is %u\n", pid);

	// debug_printf("BEFORE getting a binding for service_a\n");
	// struct capref service_a;
	// CHECK("Registering a new service service_a",
 //            lookup(&new_aos_ns_rpc, "service_a", &service_a));
	// debug_printf("AFTER getting a binding for service_a\n");

    for (a = 0; a < 300000000; a++) {
		;
	}

    debug_printf("Deregistering ...\n");
	CHECK("Deregistering a service service_a",
            deregister_service(&new_aos_ns_rpc));
	debug_printf("[DONE]\n");

    return 0;
}

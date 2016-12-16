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
/*
* 1. Add endpoint to nameserver on process creation
* 2. Service registration and deregistration
* 3. 
*
*
*
*
*/
#include <stdio.h>
#include <aos/aos.h>

#define AOS_NS_OK 0              // General-purpose OK message.
#define AOS_NS_FAILED 1          // RPC failure.
#define AOS_NS_HANDSHAKE  1 << 2  // Message passed at handshake time.
#define AOS_NS_REGISTER   1 << 3  // Message passed at handshake time.
#define AOS_NS_DEREGISTER 1 << 4  // Message passed at handshake time.
#define AOS_NS_LOOKUP     1 << 5  // Message passed at handshake time.
#define AOS_NS_ENNUMERATE 1 << 6  // Message passed at handshake time.

//enum ns_service_class { CLASS_A, CLASS_B, CLASS_C, CLASS_D, CLASS_E };

struct service {
	struct lmp_chan lc;
	struct EndPoint remote_ep;
};

struct aos_ns_rpc {
    struct lmp_chan lc;
    struct waitset* ws;
    char** names;
    int* lengths;
    int number;
    int it;
    int total;
};

errval_t aos_ns_handshake_send_handler(void* void_args);
errval_t aos_ns_handshake_recv_handler(void* void_args);

errval_t aos_ns_register_send_handler(void* void_args);
errval_t aos_ns_register_recv_handler(void* void_args);

errval_t aos_ns_deregister_send_handler(void* void_args);
errval_t aos_ns_deregister_recv_handler(void* void_args);

errval_t aos_ns_lookup_send_handler(void* void_args);
errval_t aos_ns_lookup_recv_handler(void* void_args);

errval_t aos_ns_ennumerate_send_handler(void* void_args);
errval_t aos_ns_ennumerate_recv_handler(void* void_args);

errval_t aos_ns_send_and_receive(uintptr_t* args, void* send_handler,
        void* rcv_handler);



errval_t aos_ns_init(struct aos_ns_rpc *rpc, struct waitset* ws);




/**
* @brief register a name binding
*
* @param name  the name under which to register the service
* other parameters: the service endpoint itself
*
* @return SYS_ERR_OK on success
*         errval on failure
*/
errval_t register_service(struct aos_ns_rpc* rpc, char* name);


/**
* @brief deregister a name binding
*
* @param name  the name under which the service was registered
*
* @return SYS_ERR_OK on success
*          errval on failure
*/
errval_t deregister_service(struct aos_ns_rpc* rpc);


/**
* @brief lookup a name binding
*
* @param query  the query string to look up
* other parameters: the returned service endpoint itself
*
* @return SYS_ERR_OK on success
*          errval on failure
*/
errval_t lookup(struct aos_ns_rpc* rpc, char* name, struct capref* ep);



/**
* @brief lookup a name binding
*
* @param query  the query string to look up
* @param num    returns the number of names returned
* @param result array of <num> names returned
*
* @return SYS_ERR_OK on success
*          errval on failure
*/
errval_t enumerate(struct aos_ns_rpc* rpc, size_t *num, char*** result);
/**
 * \file
 * \brief Interface declaration for AOS rpc-like messaging
 */

/*
 * Copyright (c) 2013, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _LIB_BARRELFISH_AOS_MESSAGES_H
#define _LIB_BARRELFISH_AOS_MESSAGES_H

#include <aos/aos.h>

#define AOS_RPC_OK 0              // General-purpose OK message.
#define AOS_RPC_FAILED 1          // RPC failure.
#define AOS_RPC_HANDSHAKE 1 << 2  // Message passed at handshake time.
#define AOS_RPC_MEMORY 1 << 3  	  // ID for memory requests.
#define AOS_RPC_NUMBER 1 << 5     // ID for send number requests.
#define AOS_RPC_PUTCHAR 1 << 7    // ID for putchar requests.
#define AOS_RPC_STRING 1 << 11    // ID for send string requests.

struct aos_rpc {
    struct lmp_chan lc;
    struct waitset* ws;

    // Metadata about caps during get_ram_cap requests. aos_rpc_get_ram_cap is
    // blocking, hence there can not be more than one request in the system from
    // any one cient at any given time (thus no races).
    struct ram_cap_state {
    	struct capref* retcap;
    	size_t req_bytes;
    	size_t* ret_bytes;
    } rcs;
};

errval_t aos_rpc_putchar_send(void** args);
errval_t aos_rpc_putchar_recv(void** args);
errval_t aos_rpc_send_number_handler(void **args);
errval_t aos_rpc_number_recv_handler(void **args);

/**
 * \brief send a number over the given channel
 */
errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val);

/**
 * \brief send a string over the given channel
 */
errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string);

/**
 * \brief RAM cap request.
 */
errval_t aos_rpc_ram_send(void** arg);

/**
 * \brief RAM cap response.
 */
errval_t aos_rpc_ram_recv(void** arg);

/**
 * \brief request a RAM capability with >= request_bits of size over the given
 * channel.
 */
errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t bytes,
                             struct capref *retcap, size_t *ret_bytes);

/**
 * \brief get one character from the serial port
 */
errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc);

/**
 * \brief send one character to the serial port
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c);

/**
 * \brief Request process manager to start a new process
 * \arg name the name of the process that needs to be spawned (without a
 *           path prefix)
 * \arg newpid the process id of the newly spawned process
 */
errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *name,
                               coreid_t core, domainid_t *newpid);

/**
 * \brief Get name of process with id pid.
 * \arg pid the process id to lookup
 * \arg name A null-terminated character array with the name of the process
 * that is allocated by the rpc implementation. Freeing is the caller's
 * responsibility.
 */
errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name);

/**
 * \brief Get process ids of all running processes
 * \arg pids An array containing the process ids of all currently active
 * processes. Will be allocated by the rpc implementation. Freeing is the
 * caller's  responsibility.
 * \arg pid_count The number of entries in `pids' if the call was successful
 */
errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count);

/**
 * \brief Initiate handshake by sending local cap to server.
 */
errval_t aos_rpc_handshake_send(void** arg);

/**
 * \brief Finalize handshake by receiving ack from server.
 */
errval_t aos_rpc_handshake_recv(void** arg);

/**
 * \brief General-purpose blocking RPC send-and-receive function.
 */
errval_t aos_rpc_send_and_receive(struct aos_rpc* rpc, void* send_handler,
		void* rcv_handler);

/**
 * \brief Initialize given rpc channel.
 * TODO: you may want to change the inteface of your init function, depending
 * on how you design your message passing code.
 */
errval_t aos_rpc_init(struct aos_rpc *rpc, struct waitset* ws);

#endif // _LIB_BARRELFISH_AOS_MESSAGES_H

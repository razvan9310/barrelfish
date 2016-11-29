/**
 * \file
 * \brief Cross-core client-server RPC.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.  If
 * you do not find this file, copies can be found by writing to: ETH Zurich
 * D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_CROSS_CORE_RPC_H_
#define _INIT_CROSS_CORE_RPC_H_

#include <aos/aos.h>
#include <aos/paging.h>

#include <string.h>

#define RPC_STATUS_CONSUMED 0  // Response consumed, nothing pending.
#define RPC_STATUS_PRODUCED 1  // Client has produced new pending request.

/**
 * \brief Initializes an RPC buffer for inter-core communication by mapping the
 * c- and s-frames into vspace and setting the status to RPC_STATUS_CONSUMED.
 */
errval_t cross_core_rpc_init(struct capref* client_frame,
        struct capref* server_frame, void** client_buf, void** server_buf);

/**
 * \brief Writes a new request to the client RPC buffer and sets the client
 * status to RPC_STATUS_PRODUCED, if current status is RPC_STATUS_CONSUMED.
 */
errval_t cross_core_rpc_write_request(void* client_buf, uint32_t code,
        size_t msg_len, void* msg);

/**
 * \brief Reads a request from the RPC buffer and sets the client status to
 * RPC_STATUS_CONSUMED, if current status is RPC_STATUS_PRODUCED.
 */
errval_t cross_core_rpc_read_request(void* client_buf, uint32_t* code,
        size_t* msg_len, void** msg);

/**
 * \brief Writes a new response to the RPC buffer and sets the server status to
 * RPC_STATUS_PRODUCED, if current status is RPC_STATUS_CONSUMED.
 */
errval_t cross_core_rpc_write_response(void* server_buf, uint32_t code,
        size_t msg_len, void* msg);
/**
 * \brief Reads a response from the RPC buffer and sets the server status to
 * RPC_STATUS_CONSUMED, if current status is RPC_STATUS_PRODUCED.
 */
errval_t cross_core_rpc_read_response(void* server_buf, uint32_t* code,
        size_t* msg_len, void** msg);

#endif /* _INIT_CROSS_CORE_RPC_H_ */
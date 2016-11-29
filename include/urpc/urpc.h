/**
 * \file
 * \brief URPC functionality.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.  If
 * you do not find this file, copies can be found by writing to: ETH Zurich
 * D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_URPC_H_
#define _INIT_URPC_H_

#include <string.h>

#include <aos/aos.h>

#define URPC_STATUS_CLEAR 		1  // Nothing in the system, all clear.
#define URPC_STATUS_NEW_REQUEST 2  // Client has written a new request.
#define URPC_STATUS_PROCESSING  3  // Server is currently computing a response.
#define URPC_STATUS_READY 		4  // Server has written the response.

#define URPC_CODE_CHANNEL  1  // New RPC channel establishing.
#define URPC_CODE_REFILL   2  // Inter-core RPC channel buffer refilling.

/**
 * \brief Initializes an URPC buffer for inter-core communication by setting
 * the status flag for both cores to URPC_STATUS_CLEAR.
 * This is done on core 1 to ensure that all coreboot-related URPC communication
 * is done with.
 */
static inline void urpc_init(void* urpc_buf, coreid_t my_core_id)
{
	if (my_core_id != 1) {
		return;
	}
    *((uint32_t*) urpc_buf) = URPC_STATUS_CLEAR;
    urpc_buf += BASE_PAGE_SIZE;
    *((uint32_t*) urpc_buf) = URPC_STATUS_CLEAR;
}

/**
 * \brief Sets the URPC buffer status of a client to URPC_STATUS_NEW_REQUEST.
 */
static inline void urpc_mark_request_unread(void* urpc_buf,
        coreid_t client_core_id)
{
    if (client_core_id == 1) {
        urpc_buf += BASE_PAGE_SIZE;
    }
    *((uint32_t*) urpc_buf) = URPC_STATUS_NEW_REQUEST;
}

/**
 * \brief Writes a new request to the URPC buffer and sets the status to
 * URPC_STATUS_NEW_REQUEST, if current status is URPC_STATUS_CLEAR.
 */
errval_t urpc_write_request(void* urpc_buf, coreid_t client_core_id,
        uint32_t code, size_t msg_len, void* msg);

/**
 * \brief Reads a request from the URPC buffer and sets the status to
 * URPC_STATUS_PROCESSING, if current status is URPC_STATUS_NEW_REQUEST.
 */
errval_t urpc_read_request(void* urpc_buf, coreid_t client_core_id,
        uint32_t* code, size_t* msg_len, void** msg);

/**
 * \brief Writes a new response to the URPC buffer and sets  the status to
 * URPC_STATUS_READY, if current status is URPC_STATUS_PROCESSING.
 */
errval_t urpc_write_response(void* urpc_buf, coreid_t client_core_id,
        uint32_t code, size_t msg_len, void* msg);
/**
 * \brief Reads a response from the URPC buffer and sets the status to
 * URPC_STATUS_CLEAR, if current status is URPC_STATUS_READY.
 */
errval_t urpc_read_response(void* urpc_buf, coreid_t client_core_id,
        uint32_t* code, size_t* msg_len, void** msg);

#endif /* _INIT_CORE_BOOT_H_ */
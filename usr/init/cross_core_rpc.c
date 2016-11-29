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

#include "cross_core_rpc.h"

errval_t cross_core_rpc_init(struct capref* client_frame,
        struct capref* server_frame, void** client_buf, void** server_buf)
{
	struct paging_state* st = get_current_paging_state();
	CHECK("mapping c-frame into vspace",
			paging_map_frame(st, client_buf, BASE_PAGE_SIZE, *client_frame,
					NULL, NULL));
    CHECK("mapping s-frame into vspace",
            paging_map_frame(st, server_buf, BASE_PAGE_SIZE, *server_frame,
                    NULL, NULL));

    *((uint32_t*) *client_buf) = RPC_STATUS_CONSUMED;
    *((uint32_t*) *server_buf) = RPC_STATUS_CONSUMED;

    return SYS_ERR_OK;
}

errval_t cross_core_rpc_write_request(void* client_buf, uint32_t code,
		size_t msg_len, void* msg)
{
	uint32_t* client_status = (uint32_t*) client_buf;
	if (*client_status != RPC_STATUS_CONSUMED) {
		return INIT_ERR_RPC_CHANNEL_STATE;
	}

	__asm volatile ("dmb");

	client_buf += sizeof(uint32_t);
	*((uint32_t*) client_buf) = code;
	client_buf += sizeof(uint32_t);
	*((size_t*) client_buf) = msg_len;
	if (msg_len > 0) {
		client_buf += sizeof(size_t);
		memcpy(client_buf, msg, msg_len);
	}

	__asm volatile ("dmb");
	*client_status = RPC_STATUS_PRODUCED;
	return SYS_ERR_OK;
}

errval_t cross_core_rpc_read_request(void* client_buf, uint32_t* code,
		size_t* msg_len, void** msg)
{
	uint32_t* client_status = (uint32_t*) client_buf;
	if (*client_status != RPC_STATUS_PRODUCED) {
		return INIT_ERR_RPC_CHANNEL_STATE;
	}

	__asm volatile ("dmb");

	client_buf += sizeof(uint32_t);
	*code = *((uint32_t*) client_buf);
	client_buf += sizeof(uint32_t);
	*msg_len = *((size_t*) client_buf);
	if (*msg_len > 0) {
		client_buf += sizeof(size_t);
		*msg = malloc(*msg_len);
		memcpy(*msg, client_buf, *msg_len);
	} else {
		*msg = NULL;
	}

    __asm volatile ("dmb");
    *client_status = RPC_STATUS_CONSUMED;
    return SYS_ERR_OK;
}

errval_t cross_core_rpc_write_response(void* server_buf, uint32_t code,
		size_t msg_len, void* msg)
{
	uint32_t* server_status = (uint32_t*) server_buf;
	if (*server_status != RPC_STATUS_CONSUMED) {
		return INIT_ERR_RPC_CHANNEL_STATE;
	}

	__asm volatile ("dmb");

	server_buf += sizeof(uint32_t);
	*((uint32_t*) server_buf) = code;
	server_buf += sizeof(uint32_t);
	*((size_t*) server_buf) = msg_len;
	if (msg_len > 0) {
		server_buf += sizeof(size_t);
		memcpy(server_buf, msg, msg_len);
	}

	__asm volatile ("dmb");
	*server_status = RPC_STATUS_PRODUCED;
	return SYS_ERR_OK;
}

errval_t cross_core_rpc_read_response(void* server_buf, uint32_t* code,
		size_t* msg_len, void** msg)
{
	assert(server_buf != NULL);
	uint32_t* server_status = (uint32_t*) server_buf;
	if (*server_status != RPC_STATUS_PRODUCED) {
		return INIT_ERR_RPC_CHANNEL_STATE;
	}
	
	__asm volatile ("dmb");

	server_buf += sizeof(uint32_t);
	*code = *((uint32_t*) server_buf);
	server_buf += sizeof(uint32_t);
	*msg_len = *((size_t*) server_buf);
	if (*msg_len > 0) {
		server_buf += sizeof(size_t);
		*msg = malloc(*msg_len);
		memcpy(*msg, server_buf, *msg_len);
	} else {
		*msg = NULL;
	}

    __asm volatile ("dmb");
    *server_status = RPC_STATUS_CONSUMED;
    return SYS_ERR_OK;
}
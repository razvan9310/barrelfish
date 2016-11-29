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

#include <urpc/urpc.h>

errval_t urpc_write_request(void* urpc_buf, coreid_t client_core_id,
        uint32_t code, size_t msg_len, void* msg)
{
    if (client_core_id == 1) {
        urpc_buf += BASE_PAGE_SIZE;
    }
    uint32_t* status = (uint32_t*) urpc_buf;
    if (*status != URPC_STATUS_CLEAR) {
        return URPC_ERR_CHANNEL_STATE;
    }

    __asm volatile ("dmb");

    urpc_buf += sizeof(uint32_t);
    *((uint32_t*) urpc_buf) = code;
    urpc_buf += sizeof(uint32_t);
    *((size_t*) urpc_buf) = msg_len;
    if (msg_len > 0) {
        urpc_buf += sizeof(size_t);
        memcpy(urpc_buf, msg, msg_len);
    }

    __asm volatile ("dmb");
    *status = URPC_STATUS_NEW_REQUEST;
    return SYS_ERR_OK;
}

uint32_t urpc_read_request(void* urpc_buf, coreid_t client_core_id,
        uint32_t* code, size_t* msg_len, void** msg)
{
    if (client_core_id == 1) {
        urpc_buf += BASE_PAGE_SIZE;
    }
    uint32_t* status = (uint32_t*) urpc_buf;
    if (*status != URPC_STATUS_NEW_REQUEST) {
        return URPC_ERR_CHANNEL_STATE;
    }

    __asm volatile ("dmb");

    urpc_buf += sizeof(uint32_t);
    *code = *((uint32_t*) urpc_buf);
    urpc_buf += sizeof(uint32_t);
    *msg_len = *((size_t*) urpc_buf);
    if (*msg_len > 0) {
        urpc_buf += sizeof(size_t);
        *msg = malloc(*msg_len);
        memcpy(*msg, urpc_buf, *msg_len);
    } else {
        *msg = NULL;
    }

    __asm volatile ("dmb");
    *status = URPC_STATUS_PROCESSING;
    return SYS_ERR_OK;
}

uint32_t urpc_write_response(void* urpc_buf, coreid_t client_core_id,
        uint32_t code, size_t msg_len, void* msg)
{
    if (client_core_id == 1) {
        urpc_buf += BASE_PAGE_SIZE;
    }
    uint32_t* status = (uint32_t*) urpc_buf;
    if (*status != URPC_STATUS_PROCESSING) {
        return URPC_ERR_CHANNEL_STATE;
    }

    __asm volatile ("dmb");

    urpc_buf += sizeof(uint32_t);
    *((uint32_t*) urpc_buf) = code;
    urpc_buf += sizeof(uint32_t);
    *((size_t*) urpc_buf) = msg_len;
    if (msg_len > 0) {
        urpc_buf += sizeof(size_t);
        memcpy(urpc_buf, msg, msg_len);
    }

    __asm volatile ("dmb");
    *status = URPC_STATUS_READY;
    return SYS_ERR_OK;

}

uint32_t urpc_read_response(void* urpc_buf, coreid_t client_core_id,
        uint32_t* code, size_t* msg_len, void** msg)
{
    if (client_core_id == 1) {
        urpc_buf += BASE_PAGE_SIZE;
    }
    uint32_t* status = (uint32_t*) urpc_buf;
    if (*status != URPC_STATUS_READY) {
        return URPC_ERR_CHANNEL_STATE;
    }
    
    __asm volatile ("dmb");

    urpc_buf += sizeof(uint32_t);
    *code = *((uint32_t*) urpc_buf);
    urpc_buf += sizeof(uint32_t);
    *msg_len = *((size_t*) urpc_buf);
    if (*msg_len > 0) {
        urpc_buf += sizeof(size_t);
        *msg = malloc(*msg_len);
        memcpy(*msg, urpc_buf, *msg_len);
    } else {
        *msg = NULL;
    }

    __asm volatile ("dmb");
    *status = URPC_STATUS_CLEAR;
    return SYS_ERR_OK;
}

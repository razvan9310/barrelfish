/**
 * \file
 * \brief SDMA (System Direct Memory Access) RPC bindings.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _SDMA_RPC_H_
#define _SDMA_RPC_H_

#include <aos/aos.h>
#include <aos/waitset.h>

#define SDMA_RPC_OK         0
#define SDMA_RPC_FAILED     1
#define SDMA_RPC_HANDSHAKE  1 << 1
#define SDMA_RPC_MEMCPY_SRC 1 << 2
#define SDMA_RPC_MEMCPY_DST 1 << 3
#define SDMA_RPC_MEMSET		1 << 5
#define SDMA_RPC_ROTATE_SRC 1 << 7
#define SDMA_RPC_ROTATE_DST 1 << 11

struct sdma_rpc {
	struct lmp_chan lc;
	struct waitset* ws;

	bool request_pending;

    // uint64_t ipc_time;
};

/**
 * \brief Sends and receives an RPC request-response pair.
 */
errval_t sdma_rpc_send_and_receive(uintptr_t* args, void* send_handler,
        void* rcv_handler);

/**
 * \brief Initializes an sdma_rpc structure by connecting to the SDMA driver.
 */
errval_t sdma_rpc_init(struct sdma_rpc* rpc, struct waitset* ws);
/**
 * \brief Starts handshake by sending local endpoint cap to SDMA driver.
 */
errval_t sdma_rpc_handshake_send_handler(void* void_args);
/**
 * \brief Finalizes handshake by receiving ack from SDMA driver.
 */
errval_t sdma_rpc_handshake_recv_handler(void* void_args);

/**
 * \brief SDMA RPC for memcpy (dst, dst_offset, src, src_offset, len).
 */
errval_t sdma_rpc_memcpy(struct sdma_rpc* rpc,
        struct capref dst,
        size_t dst_offset,
		struct capref src,
        size_t src_offset,
        size_t len);
/**
 * \brief SDMA RPC for memset (dst, dst_offset, len, value).
 */
errval_t sdma_rpc_memset(struct sdma_rpc* rpc,
        struct capref dst,
        size_t dst_offset,
        size_t len,
        uint8_t val);
/**
 * \brief SDMA RPC for rotate (dst, dst_offset, src_src_offset, width, height).
 */
errval_t sdma_rpc_rotate(struct sdma_rpc* rpc,
        struct capref dst,
        size_t dst_offset,
        struct capref src,
        size_t src_offset,
        size_t width,
        size_t height);

/**
 * \brief Send handler for RPC memcpy.
 */
errval_t sdma_rpc_memcpy_send_handler(void* void_args);
/**
 * \brief Send handler for RPC memset.
 */
errval_t sdma_rpc_memset_send_handler(void* void_args);
/**
 * \brief Send handler for RPC rotate.
 */
errval_t sdma_rpc_rotate_send_handler(void* void_args);
/**
 * \brief Receive handler for RPC responses.
 */
errval_t sdma_rpc_response_recv_handler(void* void_args);

/**
 * \brief Checks if there's an RPC response for us, non-blockingly. This is
 * meant for when the user wants to block until SDMA memcpy et al have been
 * completed.
 */
bool sdma_rpc_check_for_response(struct sdma_rpc* rpc);
/**
 * \brief Blocks until there's an RPC response for us. This is meant for when
 * the user wants to block until SDMA memcpy et al have been completed.
 */
errval_t sdma_rpc_wait_for_response(struct sdma_rpc* rpc);

#endif /* _SDMA_RPC_H_ */
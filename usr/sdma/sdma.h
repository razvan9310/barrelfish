/**
 * \file
 * \brief SDMA (System Direct Memory Access) driver.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _INIT_SDMA_H_
#define _INIT_SDMA_H_

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/inthandler.h>
#include <dev/omap/omap44xx_sdma_dev.h>
#include <sdma/sdma_rpc.h>

#define SDMA_IRQ_ENABLE_ALL 4294967295u
#define SDMA_IRQ_LINE_0 (32 + 12)
#define SDMA_CHANNELS   32
#define SDMA_REGISTER_CLEAN -1

#define SDMA_MEMSET_SIZE (4 * 1024)

#define CAP_MASK_SRC   1
#define CAP_MASK_DST   2
#define CAP_MASK_READY 3

typedef size_t chanid_t;

enum OpType {
    OpType_Memcpy,
    OpType_Memset,
    OpType_Rotate
};

struct client_state {
    struct lmp_chan lc;  		// LMP channel.
    struct EndPoint remote_ep;  // Used to identify clients for handshake.
    
    struct frame_identity src_id;
    size_t src_offset;
    struct frame_identity dst_id;
    size_t dst_offset;
    uint8_t have_caps;
    size_t len;

    // Use to simulate memset via memcpy -- will use this as source frame.
    struct capref memset_frame;
    struct frame_identity memset_frame_id;
    uint8_t* memset_buf;

    // For rotate ops only:
    size_t width;
    size_t height;

    enum OpType op_type;  // Memcpy, memset.
    bool acked;

    // Doubly-linked list.
    struct client_state* next;
    struct client_state* prev;
};

struct channel_state {
	bool transfer_in_progress;   // Whether there's a transfer currently in
								 // progress over this channel.
	lpaddr_t src_addr;		     // Source address for transfer, if applicable.
	lpaddr_t dst_addr;			 // Destination address for transfer, if appl.

    struct client_state* client; // Client to ack to when transfer is complete.

    // bool transfer_fits_in_blocks;  // Whether the transfer size is 

	errval_t err;                // Error from last transfer attempt.
};

struct sdma_driver {
	// SDMA driver address.
	lvaddr_t sdma_vaddr;
	omap44xx_sdma_t sdma_dev;

    // For incoming RPC requests.
    struct lmp_chan lc;

	// Channel state, 32 channels.
	struct channel_state chan_state[SDMA_CHANNELS];

	// RPC clients.
	struct client_state* clients;
};

/**
 * \brief Identifies an RPC client from an incoming RPC cap.
 */
struct client_state* sdma_identify_client_cap(struct sdma_driver* sd,
		struct capref* cap);

/**
 * \brief Updates channel status from given IRQ status.
 */
void sdma_update_channel_status(struct sdma_driver* sd, uint8_t irq_line,
        uint32_t irq_status);

/**
 * \brief Maps the physical SDMA device into vspace.
 */
errval_t sdma_map_device(struct sdma_driver* sd);

/**
 * \brief Initializez the SDMA device driver.
 */
void sdma_initialize_driver(struct sdma_driver* sd);

/**
 * \brief Gets a cap for interrupts via RPC.
 */
static inline errval_t sdma_get_irq_cap(struct sdma_driver* sd)
{
    return aos_rpc_get_irq_cap(get_init_rpc(), &cap_irq);
}

/**
 * \brief SDMA-specific interrupt handler.
 */
void sdma_interrupt_handler(void* arg);

/**
 * \brief Initializes the interrupt handler, GCR, CICR and CSR registers as per
 * the technical manual section 16.5.1 Setup Configuration.
 */
errval_t sdma_setup_config(struct sdma_driver* sd);

/**
 * \brief Initializes the local (server-side) endpoint for SDMA RPC.
 */
errval_t sdma_setup_rpc_server(struct sdma_driver* sd);

/**
 * \brief Entry point for server-side SDMA processing.
 */
void sdma_serve_rpc(void* arg);

/**
 * \brief Processes an SDMA handshake request.
 */
void* sdma_process_handshake(struct sdma_driver* sd, struct capref* cap);
/**
 * \brief Processes an SDMA memcpy request.
 */
void* sdma_process_memcpy(struct sdma_driver* sd, struct client_state* client,
        struct lmp_recv_msg* msg, struct capref* cap);
/**
 * \brief Processes an SDMA memset request.
 */
void* sdma_process_memset(struct sdma_driver* sd, struct client_state* client,
        struct lmp_recv_msg* msg, struct capref* cap);
/**
 * \brief Processes an SDMA rotate request.
 */
void* sdma_process_rotate(struct sdma_driver* sd, struct client_state* client,
        struct lmp_recv_msg* msg, struct capref* cap);

/**
 * \brief SDMA RPC handshake send handler.
 */
void sdma_send_handshake(void* arg);
/**
 * \brief SDMA RPC error status send handler.
 */
void sdma_send_err(void* arg);

/**
 * \brief Finds an available SDMA channel.
 */
chanid_t sdma_avail_channel(struct sdma_driver* sd);

/**
 * \brief Returns whether the given channel is valid, i.e. between 0 and 31.
 */
static inline bool sdma_valid_channel(chanid_t chan)
{
	return chan < SDMA_CHANNELS;
}

/**
 * \brief Starts an SDMA job identified by the given (dst, src, len) tuple.
 */
errval_t sdma_start_transfer(struct sdma_driver* sd,
        struct client_state* client,
        size_t src_addr,
        size_t dst_addr,
        size_t len);
/**
 * \brief Starts an SDMA 2D-array rotate job.
 */
errval_t sdma_start_rotate(struct sdma_driver* sd,
        struct client_state* client,
        size_t src_addr,
        size_t dst_addr,
        size_t width,
        size_t height);

#endif /* _INIT_SDMA_H_ */
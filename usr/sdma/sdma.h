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

#define SDMA_IRQ_LINE_0 (32 + 12)
#define SDMA_CHANNELS   32

typedef size_t chanid_t;

struct channel_state {
	bool transfer_in_progress;  // Whether there's a transfer currently in
								// progress over this channel.
	lpaddr_t src_addr;		    // Source address for transfer, if applicable.
	lpaddr_t dst_addr;			// Destination address for transfer, if appl.
	size_t size;                // Transfer size, if applicable.

	errval_t err;               // Error from last transfer attempt.
};

struct sdma_driver {
	// SDMA driver address.
	lvaddr_t sdma_vaddr;
	omap44xx_sdma_t sdma_dev;

	// Channel state, 32 channels.
	struct channel_state chan_state[SDMA_CHANNELS];

	void* buf;  // Temporary testing.
};

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

void sdma_test_copy(struct sdma_driver* sd, struct capref* frame);

#endif /* _INIT_SDMA_H_ */
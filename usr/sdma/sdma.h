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

#include <aos/aos_rpc.h>
#include <aos/inthandler.h>
#include <dev/omap/omap44xx_sdma_dev.h>

#define SDMA_IRQ_LINE_0 (32 + 12)

struct sdma_driver {
	// SDMA driver address.
	lvaddr_t sdma_vaddr;
	omap44xx_sdma_t sdma_dev;
};

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

static inline errval_t sdma_enable_interrupt(void)
{
	return inthandler_setup_arm(sdma_interrupt_handler, NULL, SDMA_IRQ_LINE_0);
}

#endif /* _INIT_SDMA_H_ */
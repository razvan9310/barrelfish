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

#include <aos/aos.h>
#include <driverkit/driverkit.h>
#include <omap44xx_map.h>

#include "sdma.h"

errval_t sdma_map_device(struct sdma_driver* sd)
{
    return map_device_register(
            OMAP44XX_MAP_L4_CFG_SDMA,
            OMAP44XX_MAP_L4_CFG_SDMA_SIZE,
            &sd->sdma_vaddr);
}

void sdma_initialize_driver(struct sdma_driver* sd)
{
    omap44xx_sdma_initialize(&sd->sdma_dev, (mackerel_addr_t) sd->sdma_vaddr);
    debug_printf("omap44xx_sdma_dma4_revision_rd = 0x%x\n",
            omap44xx_sdma_dma4_revision_rd(&sd->sdma_dev));
}

void sdma_interrupt_handler(void* arg)
{
    debug_printf("!!! Got SDMA interrupt!\n");
}
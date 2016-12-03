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
#include <dev/omap/omap44xx_sdma_dev.h>
#include <driverkit/driverkit.h>
#include <omap44xx_map.h>

#include "sdma.h"

void test(void)
{
    lvaddr_t sdma_base;
    errval_t err = map_device_register(
            OMAP44XX_MAP_L4_CFG_SDMA,
            OMAP44XX_MAP_L4_CFG_SDMA_SIZE,
            &sdma_base);
    if (err_is_fail(err)) {
        USER_PANIC_ERR(err, "map_device_register failed");
    }

    omap44xx_sdma_t devsdma;
    omap44xx_sdma_initialize(&devsdma, (mackerel_addr_t) sdma_base);
    debug_printf("omap44xx_sdma_dma4_revision_rd = 0x%x\n",
            omap44xx_sdma_dma4_revision_rd(&devsdma));
}

int main(int argc, char** argv)
{
    test();
}
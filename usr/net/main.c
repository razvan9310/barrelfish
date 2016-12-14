/**
 * \file
 * \brief Sets up UART4 and wires it to the network stack via SLIP.
 */

/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetsstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */


#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <omap44xx_map.h> // for the serial's physical address
#include <netutil/user_serial.h>
#include <driverkit/driverkit.h>
#include <netutil/htons.h>
#include "slip.h"
#include "ip.h"

static struct aos_rpc init_rpc;

static struct circ_buf slip_buf;

static void handle_packet(struct ip_packet *packet) {
    uint32_t src = ntohl(packet->header.src4);
    uint32_t dst = ntohl(packet->header.dst4);
    debug_printf("got packet (%u bytes of stuff):\n", packet->len);
    debug_printf("src %hhu.%hhu.%hhu.%hhu\n", src >> 24, src >> 16, src >> 8, src);
    debug_printf("dst %hhu.%hhu.%hhu.%hhu\n", dst >> 24, dst >> 16, dst >> 8, dst);
    debug_printf("ttl=%hhu len=%hu protocol=%hhu\n",
                 packet->header.ttl, ntohs(packet->header.len), packet->header.protocol);
}

/**
 * Called by the serial driver whenever input is available.
 * Does not need to be reentrant in the current implementation.
 */
void serial_input(uint8_t *buf, size_t len) {
    circ_buf_write(&slip_buf, buf, len);
    slip_decode(&slip_buf, &handle_packet);
    // TODO poke IP layer
}

int main(int argc, char *argv[])
{
    errval_t err;
    debug_printf("starting networking....\n");

    init_rpc = *get_init_rpc();
    circ_buf_init(&slip_buf);

    // 1. setup the additional UART
    lvaddr_t serial_addr;

    CHECK("mapping serial's address to my vspace",
          map_device_register(OMAP44XX_MAP_L4_PER_UART4,
                              OMAP44XX_MAP_L4_PER_UART4_SIZE,
                              &serial_addr));
    // stick cap_irq into our CSpace so that serial_init() works
    CHECK("getting IRQ cap", aos_rpc_get_irq_cap(&init_rpc, &cap_irq));
    CHECK("serial_init", serial_init(serial_addr, UART4_IRQ));

    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    return EXIT_SUCCESS;
}

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
#include "icmp.h"
#include "udp.h"
// #include "net.h"

static struct aos_rpc init_rpc;

static struct circ_buf in_buf;
static struct circ_buf out_buf;

#define MY_IP (10<<24 | 0<<16 | 2<<8 | 1) // of course we can only have 1 IP :D

static errval_t dispatch_ip_packet(struct ip_packet *packet) {
    uint32_t src = ntohl(packet->header.src4);
    uint32_t dst = ntohl(packet->header.dst4);
    debug_printf("->IP"
                 " src=%hhu.%hhu.%hhu.%hhu"
                 " dst=%hhu.%hhu.%hhu.%hhu"
                 " ttl=%hhu len=%hu protocol=0x%hhx\n",
                 src >> 24, src >> 16, src >> 8, src,
                 dst >> 24, dst >> 16, dst >> 8, dst,
                 packet->header.ttl, ntohs(packet->header.len), packet->header.protocol);

    uint8_t *payload = packet->bytes + IP_HEADER_LEN(packet);
    size_t payload_len = packet->len - IP_HEADER_LEN(packet);
    switch (packet->header.protocol) {
        case PROTOCOL_ICMP:
            return handle_icmp(src, dst, (struct icmp_datagram *)payload, payload_len);
            break;
        case PROTOCOL_UDP:
            return handle_udp(src, dst, (struct udp_datagram *)payload, payload_len);
            break;
        default:
            debug_printf("No handler for protocol 0x%hhx, dropping packet\n", packet->header.protocol);
            return NET_ERR_NOT_SUPPORTED;
            break;
    }
}

errval_t pass_ip_to_link(struct ip_packet *packet) {
    uint32_t src = ntohl(packet->header.src4);
    uint32_t dst = ntohl(packet->header.dst4);
    debug_printf("<-IP"
             " src=%hhu.%hhu.%hhu.%hhu"
             " dst=%hhu.%hhu.%hhu.%hhu"
             " ttl=%hhu len=%hu protocol=0x%hhx\n",
             src >> 24, src >> 16, src >> 8, src,
             dst >> 24, dst >> 16, dst >> 8, dst,
             packet->header.ttl, ntohs(packet->header.len), packet->header.protocol);

    slip_encode(&out_buf, packet);
    uint8_t r;
    while (circ_buf_consume_byte(&out_buf, &r)) {
    serial_write(&r, 1);
    }
    return SYS_ERR_OK; // can't detect errors
}

/**
 * Called by the serial driver whenever input is available.
 * Does not need to be reentrant in the current implementation.
 */
void serial_input(uint8_t *buf, size_t len) {
    circ_buf_write(&in_buf, buf, len);
    slip_decode(&in_buf, &pass_link_to_ip);
}

static uint32_t udp_echo_server_socket;

// for now it is here, TODO separate domain
static void udp_echo_server_recv(uint32_t src4, uint16_t sport, uint8_t *data, size_t len) {
    debug_printf("Got %d bytes of UDP stuff: src4=0x%x sport=%d data=%s\n", len, src4, sport, data);
    errval_t err = udp_send(udp_echo_server_socket, src4, sport, data, len);
    assert(!err_is_fail(err));
}

static void udp_echo_server_subscribe(void) {
    assert(!err_is_fail(udp_get_socket(MY_IP, 47, udp_echo_server_recv, &udp_echo_server_socket)));
}

int main(int argc, char *argv[])
{
    errval_t err;
    debug_printf("starting networking....\n");

    init_rpc = *get_init_rpc();
    circ_buf_init(&in_buf);

    ip_register_packet_dispatcher(dispatch_ip_packet);

    // 1. setup the additional UART
    lvaddr_t serial_addr;

    CHECK("mapping serial's address to my vspace",
          map_device_register(OMAP44XX_MAP_L4_PER_UART4,
                              OMAP44XX_MAP_L4_PER_UART4_SIZE,
                              &serial_addr));
    // stick cap_irq into our CSpace so that serial_init() works
    CHECK("getting IRQ cap", aos_rpc_get_irq_cap(&init_rpc, &cap_irq));
    CHECK("in serial_init", serial_init(serial_addr, UART4_IRQ));

    udp_echo_server_subscribe();

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

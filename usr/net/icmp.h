/**
 * \file
 * \brief ICMP
 */

#ifndef _USR_NET_ICMP_H_
#define _USR_NET_ICMP_H_

#include <aos/aos.h>
#include <stdlib.h>
#include "ip.h"

#define PROTOCOL_ICMP 0x01

struct icmp_datagram {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint32_t ident;
    uint8_t  data[];
} __attribute__((packed));
#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

errval_t handle_icmp(uint32_t src, uint32_t dst, struct icmp_datagram *datagram, size_t len);

#endif

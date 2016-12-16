/**
 * \file
 * \brief IP layer
 */

#ifndef _USR_NET_IP_H_
#define _USR_NET_IP_H_

#include <aos/aos.h>
#include <stdlib.h>
#include "stack.h"

// because statically allocated buffers are awesome :D
#define MAX_PACKET_SIZE 1536

struct ip_header {
    uint8_t  ver_ihl;
    uint8_t  dscp_ecn;
    uint16_t len;
    uint16_t ident;
    uint16_t flags_fragoff;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src4;
    uint32_t dst4;
    uint8_t  options[];
} __attribute__((packed));

#define VER_MASK   0xf0
#define DSCP_MASK  0xfc
#define FLAGS_MASK 0xe000
#define FLAGS_EVIL 1<<(0+13)
#define FLAGS_DF   1<<(1+13)
#define FLAGS_MF   1<<(2+13)

#define IP_HEADER_LEN(packet) (4*(packet->header.ver_ihl & (~VER_MASK)))

struct ip_packet {
    size_t len;
    union {
        uint8_t bytes[MAX_PACKET_SIZE];
        struct ip_header header;
    };
};

errval_t ip_was_packet_good(struct ip_packet *packet);
void ip_register_packet_dispatcher(packet_handler dispatcher);
void ip_make_packet(struct ip_packet *packet, uint32_t src, uint32_t dst, uint8_t protocol, uint8_t *payload, size_t len);

#endif

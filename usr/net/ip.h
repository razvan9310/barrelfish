/**
 * \file
 * \brief IP layer
 */

#ifndef _USR_NET_IP_H_
#define _USR_NET_IP_H_

#include <aos/aos.h>
#include <stdlib.h>

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

#define VER_MASK   = 0xf0;
#define DSCP_MASK  = 0xfc;
#define FLAGS_MASK = 0xe000;
#define FLAGS_EVIL = 1<<(0+13);
#define FLAGS_DF   = 1<<(1+13);
#define FLAGS_MF   = 1<<(2+13);

struct ip_packet {
    size_t len;
    union {
        uint8_t bytes[MAX_PACKET_SIZE];
        struct ip_header header;
    };
};

// packet will be freed after this function exits
typedef void (*packet_handler)(struct ip_packet *packet);

// checks the packet and passes to callback if it looks good
// TODO probably shouldn't be told about what callback to use :D
void ip_consume_packet(struct ip_packet *packet, packet_handler callback);

#endif

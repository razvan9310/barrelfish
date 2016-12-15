/**
 * \file
 * \brief UDP
 */

#ifndef _USR_NET_UDP_H_
#define _USR_NET_UDP_H_

#include <aos/aos.h>
#include <stdlib.h>
#include "ip.h"

#define PROTOCOL_UDP 0x11

struct udp_header {
    u_short sport;       /* source port */
    u_short dport;       /* destination port */
    u_short len;         /* udp length */
    u_short sum;         /* udp checksum */
} __attribute__((packed));

struct udp_datagram {
    struct udp_header header;
    uint8_t  data[];
} __attribute__((packed));

typedef void (*udp_receive_callback)(uint32_t src4, uint16_t sport, uint8_t *buf, size_t len);

errval_t handle_udp(uint32_t src, uint32_t dst, struct udp_datagram *datagram, size_t len);
errval_t udp_socket_open(uint32_t laddr, uint16_t lport, udp_receive_callback cb, uint32_t *fd);
errval_t udp_send(uint32_t fd, uint32_t dst4, uint16_t dport, void* buf, size_t len);

#endif

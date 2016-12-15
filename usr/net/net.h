/**
 * \file
 * \brief net stuff useful for applications -- interface for the outside world
 */

#ifndef _USR_NET_NET_H_
#define _USR_NET_NET_H_

#include <aos/aos.h>
#include <stdlib.h>

struct ip_packet; // defined in ip.h; here an opaque forward-declaration

typedef errval_t (*packet_handler)(struct ip_packet *packet);

errval_t pass_transport_to_ip(uint32_t src, uint32_t dst, uint8_t protocol, uint8_t *payload, size_t len);
errval_t pass_ip_to_link(struct ip_packet *packet);

errval_t pass_link_to_ip(struct ip_packet *packet);
errval_t pass_ip_to_transport(uint32_t src, uint32_t dst, uint8_t protocol, uint8_t *payload, size_t len);

// errval_t send_raw_ip_packet(uint32_t src, uint32_t dst, uint8_t protocol, uint8_t *payload, size_t len);

#endif

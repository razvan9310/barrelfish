/**
 * \file
 * \brief IP layer
 */

#include <aos/aos.h>
#include <stdlib.h>
#include <netutil/htons.h>
#include <netutil/checksum.h>
#include "ip.h"
#include "util.h"

static packet_handler ip_packet_dispatcher;

void ip_register_packet_dispatcher(packet_handler dispatcher) {
    ip_packet_dispatcher = dispatcher;
}

// bad packets will not get candy
errval_t ip_was_packet_good(struct ip_packet *packet) {
    // CHECK ALL THE THINGS!
    CHECKP(packet->len >= sizeof(struct ip_header), "IP packet too short");
    CHECKP((packet->header.ver_ihl & VER_MASK) == 0x40, "IP version != 4");
    uint8_t header_len = IP_HEADER_LEN(packet);
    CHECKP(header_len >= sizeof(struct ip_header), "header length < min");
    CHECKP(header_len <= packet->len, "header length > packet length");
    CHECKP(ntohs(packet->header.len) == packet->len, "declared and actual length do not match");
    CHECKP(!(ntohs(packet->header.flags_fragoff) & FLAGS_EVIL), "Evil bit set! Booo!");
    CHECKP(!(ntohs(packet->header.flags_fragoff) & FLAGS_MF) &&
             (ntohs(packet->header.flags_fragoff) & (~FLAGS_MASK)) == 0, "Fragmented, chicken out");
    CHECKP(inet_checksum(packet->bytes, header_len) == 0, "Wrong IP header checksum");

    return true;
}

errval_t pass_link_to_ip(struct ip_packet *packet) {
    CHECK("sanity-checking packet", ip_was_packet_good(packet));
    return ip_packet_dispatcher(packet);
}

void ip_make_packet(struct ip_packet *packet, uint32_t src, uint32_t dst, uint8_t protocol, uint8_t *payload, size_t len) {
    memset((void*)&packet->header, 0, sizeof(struct ip_header));
    uint8_t header_len = sizeof(struct ip_header); // OPTIONS not supported
    packet->len = header_len + len;
    // everything else can stay at 0
    packet->header.ver_ihl = 0x40 | (header_len>>2);
    packet->header.len = htons(packet->len);
    packet->header.ttl = 64;
    packet->header.protocol = protocol;
    packet->header.src4 = htonl(src);
    packet->header.dst4 = htonl(dst);
    packet->header.checksum = inet_checksum((void*)&packet->header, sizeof packet->header);
    memcpy(packet->bytes + header_len, payload, len);
}


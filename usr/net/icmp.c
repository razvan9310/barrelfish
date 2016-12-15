/**
 * \file
 * \brief ICMP
 */

#include <aos/aos.h>
#include <stdlib.h>
#include <netutil/htons.h>
#include <netutil/checksum.h>
#include "icmp.h"
#include "stack.h"
#include "util.h"

static errval_t good(struct icmp_datagram *datagram, size_t len) {
    CHECKP(len >= sizeof(struct icmp_datagram), "ICMP packet too short");
    CHECKP(inet_checksum(datagram, len) == 0, "Wrong ICMP checksum");
    return SYS_ERR_OK;
}

static errval_t handle_echo_request(uint32_t src, uint32_t dst, struct icmp_datagram *datagram, size_t len) {
    CHECKP(datagram->code == 0, "ICMP ECHO REQUEST code != 0");
    debug_printf("->ICMP ECHO REQUEST (ident %x), payload: %s\n", htonl(datagram->ident), datagram->data);

    struct icmp_datagram *reply = malloc(sizeof(struct icmp_datagram));
    reply->type = ICMP_ECHO_REPLY;
    reply->code = 0;
    reply->ident = datagram->ident;
    memcpy(reply->data, datagram->data, len - sizeof(struct icmp_datagram));
    reply->checksum = 0;
    reply->checksum = inet_checksum(reply, len);

    // TODO
    // CHECK("replying to ICMP ECHO", send_packet(dst, src, PROTOCOL_ICMP, (uint8_t*)reply, len));
    CHECK("replying to ICMP ECHO", pass_transport_to_ip(dst, src, PROTOCOL_ICMP, (uint8_t*)reply, len));

    free(reply);
    return true;
}

errval_t handle_icmp(uint32_t src, uint32_t dst, struct icmp_datagram *datagram, size_t len) {
    CHECK("checking ICMP datagram", good(datagram, len));
    switch (datagram->type) {
        case ICMP_ECHO_REQUEST:
            return handle_echo_request(src, dst, datagram, len);
            break;
        default:
            debug_printf("->ICMP unknown type %hhu, dropping");
            return NET_ERR_NOT_SUPPORTED;
            break;
    }
    return SYS_ERR_OK;
}

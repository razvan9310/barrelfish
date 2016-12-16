#include "stack.h"
#include "ip.h"

static errval_t _real_send_ip_packet(uint32_t src, uint32_t dst, uint8_t protocol, uint8_t *payload, size_t len) {
    struct ip_packet *packet = malloc(sizeof(struct ip_packet));

    ip_make_packet(packet, src, dst, protocol, payload, len);
    assert(!err_is_fail(ip_was_packet_good(packet)));
    CHECK("passing IP packet to link layer", pass_ip_to_link(packet));

    free(packet);
    return SYS_ERR_OK;
}

errval_t pass_transport_to_ip(uint32_t src, uint32_t dst, uint8_t protocol, uint8_t *payload, size_t len) {
    return _real_send_ip_packet(src, dst, protocol, payload, len); // this should be through an RPC
}

#include "stack.h"
#include "udp.h"
#include "util.h"
#include <netutil/checksum.h>
#include <netutil/htons.h>

// How applications should work:
struct udp_socket {
    uint32_t laddr;
    uint16_t lport;
    // TODO
    // aos_rpc recv;
    // aos_rpc send;
    udp_receive_callback recv;
};

#define MAX_SOCKETS 64

struct udp_socket open_sockets[MAX_SOCKETS]; // static buffers are awesome! ...or something...

static errval_t good(struct udp_datagram *datagram, size_t len) {
    CHECKP(len >= sizeof(struct udp_datagram), "UDP packet too short");
    // CHECKP(datagram->header.sum == 0 || inet_checksum((void*)datagram, len) == 0, "Wrong UDP checksum");
    // nobody needs the checksum :D
    return SYS_ERR_OK;
}

errval_t udp_socket_open(uint32_t laddr, uint16_t lport, udp_receive_callback cb, uint32_t *fd) {
    if (lport == 0) return NET_ERR_PORT_RESERVED;
    for (int i = 0; i < MAX_SOCKETS; ++i) {
        // race conditions? come on, who would use networking? ;-)
        if (open_sockets[i].lport == 0) { // free
            open_sockets[i].laddr = laddr;
            open_sockets[i].lport = lport;
            open_sockets[i].recv = cb;
            *fd = i;
            return SYS_ERR_OK;
        }
    }
    return NET_ERR_SOCKETS_EXHAUSTED;
}

// TODO this will use a cap, not a fd
errval_t udp_send(uint32_t fd, uint32_t dst4, uint16_t dport, void* buf, size_t len) {
    assert(open_sockets[fd].lport != 0);
    size_t payload_len = sizeof(struct udp_header) + len;
    struct udp_datagram *d = (struct udp_datagram *)malloc(payload_len);
    d->header.sport = htons(open_sockets[fd].lport);
    d->header.dport = htons(dport);
    d->header.len = htons(payload_len);
    d->header.sum = 0;
    // d->header.sum = inet_checksum((void*)d, payload_len); // checksums are optional in UDP :D
    memcpy(d->data, buf, len);
    assert(!err_is_fail(good(d, payload_len)));
    CHECK("sending UDP packet",
          pass_transport_to_ip(open_sockets[fd].laddr, dst4, PROTOCOL_UDP, (uint8_t *)d, payload_len));
    free(d);
    return SYS_ERR_OK;
}

errval_t handle_udp(uint32_t src, uint32_t dst, struct udp_datagram *datagram, size_t len) {
    CHECK("sanity-checking received UDP datagram", good(datagram, len));
    for (uint32_t i = 0; i < MAX_SOCKETS; ++i) {
        if (open_sockets[i].laddr == dst &&
            open_sockets[i].lport == ntohs(datagram->header.dport)) {
            open_sockets[i].recv(src,
                                 ntohs(datagram->header.sport),
                                 datagram->data,
                                 ntohs(datagram->header.len) - sizeof(struct udp_header));
            return SYS_ERR_OK;
        }
    }
    return NET_ERR_CONN_REFUSED;
}

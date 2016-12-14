/**
 * \file
 * \brief Implements the network stack. Expects to be the only thing on this device.
 */

#include "slip.h"

enum SLIP_SPECIAL_BYTES {
    SLIP_END = 0xc0,
    SLIP_ESC = 0xdb,
    SLIP_ESC_END = 0xdc,
    SLIP_ESC_ESC = 0xdd,
    SLIP_ESC_NUL = 0xde,
};

static struct {
    struct ip_packet packet; // the packet currently being written
    bool escape_next; // true iff the next char should be treated as escaped
} slip_current_state;

static void slip_finish_packet(packet_handler callback) {
    struct ip_packet *packet = (struct ip_packet *)malloc(sizeof(struct ip_packet));
    packet->len = slip_current_state.packet.len;
    memcpy(packet->bytes, slip_current_state.packet.bytes, packet->len);
    ip_consume_packet(packet, callback);
    free(packet);
}

static void slip_add_byte(uint8_t r) {
    assert(slip_current_state.packet.len < MAX_PACKET_SIZE);
    slip_current_state.packet.bytes[slip_current_state.packet.len] = r;
    ++slip_current_state.packet.len;
}

/**
 * Consumes an IP packet from the SLIP-encoded in_buf.
 */
void slip_decode(struct circ_buf *in_buf, packet_handler callback) {
    while (circ_buf_len(in_buf) > 0) {
        uint8_t r;
        if (!circ_buf_consume_byte(in_buf, &r))
            continue;

        if (slip_current_state.escape_next) {
            switch (r) {
                case SLIP_ESC_END:
                    slip_add_byte(SLIP_END);
                    break;
                case SLIP_ESC_ESC:
                    slip_add_byte(SLIP_ESC);
                    break;
                case SLIP_ESC_NUL:
                    slip_add_byte(0x0);
                    break;
                default:
                    debug_printf("slip_decode got SLIP_ESC %x, this should not happen\n");
                    slip_add_byte(r);
                    break;
            }
            slip_current_state.escape_next = false;
        } else {
            switch (r) {
                case SLIP_END:
                    slip_finish_packet(callback);
                    break;
                case SLIP_ESC:
                    slip_current_state.escape_next = true;
                    break;
                default:
                    slip_add_byte(r);
                    break;
            }
        }
    }
}

/**
 * \file
 * \brief Implements the network stack. Expects to be the only thing on this device.
 */

#include "util.h"

///// circ_buf_* //////////////////////////////////////////////////

void circ_buf_init(struct circ_buf *buf) {
    memset(buf, 0, sizeof(struct circ_buf));
}

void circ_buf_write(struct circ_buf *dest, uint8_t *src, size_t len) {
    // we could do a memcpy and it could be faster, but nobody cares
    for (int i = 0; i < len; ++i) {
        dest->buf[dest->end] = src[i];
        dest->end += 1; dest->end %= CIRC_BUF_SIZE;
        assert(dest->end != dest->start); // overflow
    }
}

// Returns false if buffer empty
bool circ_buf_consume_byte(struct circ_buf *src, uint8_t *r) {
    if (src->end == src->start) return false;
    *r = src->buf[src->start];
    src->start += 1; src->start %= CIRC_BUF_SIZE;
    return true;
}

size_t circ_buf_len(struct circ_buf *buf) {
    if (buf->end < buf->start) return buf->end + CIRC_BUF_SIZE - buf->start;
    return buf->end - buf->start;
}

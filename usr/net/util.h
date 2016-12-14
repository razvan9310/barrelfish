/**
 * \file
 * \brief Format declarations for various structs in the network stack.
 */

#ifndef _USR_NET_UTIL_H_
#define _USR_NET_UTIL_H_

#include <aos/aos.h>
#include <stdlib.h>

#define CIRC_BUF_SIZE 128 // because statically allocated buffers are awesome :D

struct circ_buf {
    uint8_t buf[CIRC_BUF_SIZE];
    size_t start;
    size_t end;
};

void circ_buf_init(struct circ_buf *buf);
void circ_buf_write(struct circ_buf *dest, uint8_t *src, size_t len);
bool circ_buf_consume_byte(struct circ_buf *src, uint8_t *r);
size_t circ_buf_len(struct circ_buf *buf);

#endif

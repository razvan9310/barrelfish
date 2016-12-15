/**
 * \file
 * \brief net stuff useful for applications -- interface for the outside world
 */

#ifndef _USR_NET_NET_H_
#define _USR_NET_NET_H_

#include <aos/aos.h>
#include <stdlib.h>

typedef void (*socket_receive_callback)(uint32_t src4, uint16_t sport, uint8_t *buf, size_t len);

/**
 * \brief Send data over a previously-opened UDP socket.
 */
errval_t socket_send(struct capref socket, uint32_t dst4, uint16_t dport, void* buf, size_t len);

/**
 * \brief Subscribe for receiving data (asynchronously).
 */
errval_t socket_listen(struct capref socket, socket_receive_callback cb);

#endif

/**
 * \file
 * \brief SLIP
 */

#ifndef _USR_NET_SLIP_H_
#define _USR_NET_SLIP_H_

#include <aos/aos.h>
#include <stdlib.h>
#include "ip.h"
#include "util.h"

void slip_decode(struct circ_buf *in_buf, packet_handler consume_packet);

#endif

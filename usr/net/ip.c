/**
 * \file
 * \brief IP layer
 */

#include <aos/aos.h>
#include <stdlib.h>
#include "ip.h"

void ip_consume_packet(struct ip_packet *packet, packet_handler callback) {
    // TODO checks
    // TODO pass to the appropriate protocol's handler :D
    callback(packet);
}

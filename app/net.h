#ifndef NET_H
#define NET_H

#include "type.h"
#include "ip4.h"
#include "udp.h"

bool net_ready(void);
bool net_connect(void);
bool net_get_target_ip (u32 *ip, u16 *port, const ascii *url);
bool net_udp_tx(udp_packet_t *pkt);
void net_udp_rx(udp_packet_t *pkt);


#endif // ! NET_H


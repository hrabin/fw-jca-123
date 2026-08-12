#ifndef NET_UDP_H
#define NET_UDP_H

#include "type.h"
#include "ip4.h"
#include "buf.h"

typedef struct {
    ip_addr_t dst_ip;   //
    ip_addr_t src_ip;   //
    u8 *data;           //
    u16 dst_port;       //
    u16 src_port;       //
    u16 datalen;        //
    bool packet_ready;  //
} udp_packet_t;

typedef void (*udp_rx_callback_t)(udp_packet_t *pkt);

// RX dispatch and stats — implemented by net/udp.c
void udp_register_rx_callback(udp_rx_callback_t cb);
void udp_rx(u8 *data, u16 len, ip_addr_t *addr, u16 port);
void udp_count_tx(u16 len);
void udp_info(buf_t *dest);

// Transport API — implemented by the platform layer (modem/modem_udp.c)
bool udp_ready(void);
bool udp_tx(udp_packet_t *pkt);

#endif // ! NET_UDP_H

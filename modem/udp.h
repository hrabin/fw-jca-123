#ifndef UDP_H
#define UDP_H

#include "type.h"
#include "ip4.h"
#include "buf.h"

typedef struct {
	ip_addr_t dst_ip;	// 
	ip_addr_t src_ip;	//
	u8 *data;			// 
	u16 dst_port;		// 
	u16 src_port;		// 
	u16 datalen;		// 
	bool packet_ready;	// 
} udp_packet_t;

bool udp_ready(void);
bool udp_tx(udp_packet_t *pkt);
void udp_rx (u8 *data, u16 len, ip_addr_t *addr, u16 port);
void udp_info(buf_t *dest);

#endif // ! UDP_H


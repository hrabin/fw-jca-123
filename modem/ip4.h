#ifndef IP4_H
#define IP4_H

#include    "type.h"

#define	IP_HDR_LEN  20

#define IP_KNOWNVERSION	0x40 	// IPv.4
#define IP_IHL	5 		// Internet Header Length. 4 bits. Specifies the length of the IP packet header in 32 bit words
#define IP_TYPEOFSERV_PR_HREL  0x24 // Priority + High Relibility
#define IP_FRAGMENT_DONTDO     0x40,0x00 // Zakazuji fragmentaci paketu

#define IP_PROTO_ICMP    1  // Internet Control Message Protocol
#define IP_PROTO_TCP     6  // Transmission Control Protocol
#define IP_PROTO_UDP     17 // User Datagram Protocol


typedef struct {
	u32 addr;
} ip_addr_t;

extern s16 ip_protocol (u8 *data);
extern u16 ip_checksum (u8 *data);

#endif // ! IP4_H

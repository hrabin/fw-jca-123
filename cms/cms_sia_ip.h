#ifndef CMS_SIA_IP_H
#define CMS_SIA_IP_H

#include "type.h"
#include "event.h"
#include "buf.h"

// SIA-DC-09 (SIA over IP) protocol codec.
//
// Message frame:
//   <LF><crc><0LLL>"SIA-DCS"<seq>L<prefix>#<account>[<data>]<timestamp><CR>
//   <crc>  4 hex chars, CRC-16 of the data region
//   <0LLL> 4 hex chars, '0' + 3-char hex length of the data region

#define SIA_IDX_CRC     1
#define SIA_IDX_LEN     5
#define SIA_IDX_DATA    9

#define CMS_SIA_IP_MAX_MSG_LEN      240
#define CMS_SIA_IP_MAX_PACKET_LEN  (CMS_SIA_IP_MAX_MSG_LEN + SIA_IDX_DATA)

typedef struct {
    u32 account;      // subscriber account (SIA account number)
    u16 cnt;          // packet counter 1..9999, incremented per new event
    u16 prefix;       // line prefix "L"
    u8  enable_event_name:1;   // append event name text [I...]
    u8  enable_event_time:1;   // append event time [H...]
    u8  enable_time_stamp:1;   // append current timestamp
} cms_sia_ip_state_t;

void cms_sia_ip_init (cms_sia_ip_state_t *s);

// Build a complete SIA message into dataout (buf must be pre-initialized).
// Returns total packet length, or 0 on error.
u16  cms_sia_ip_build_msg (cms_sia_ip_state_t *s, buf_t *dataout, event_t *e, ascii *data);

// Validate a received SIA frame (LF header, CRC-16, length).
bool cms_sia_ip_rx (u8 *udpdata, u16 len);

// Parse an ACK/NAK frame. Returns true and fills *ack_seq / *is_nak
// if the frame is a valid ACK or NAK matching our account and prefix.
bool cms_sia_ip_rx_ack (cms_sia_ip_state_t *s, u8 *udpdata, u16 len,
                        u16 *ack_seq, bool *is_nak);

#endif // ~CMS_SIA_IP_H

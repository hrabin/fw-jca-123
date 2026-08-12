#ifndef CMS_PROTO_H
#define CMS_PROTO_H

#include "type.h"
#include "event.h"
#include "buf.h"

// Protocol interface for security-center communication.
// Each protocol (SIA-IP, ...) provides one const instance and keeps
// its own internal state (sequence counter, packet buffer).
// Adding a new protocol means implementing this struct in a new .c
// file; cms_main.c does not need to change.

// must be >= every protocol's maximum packet length
#define CMS_MAX_PACKET_LEN 260

typedef enum {
    CMS_REPLY_NONE = 0,   // not a reply / not for us
    CMS_REPLY_ACK,        // positive acknowledgment
    CMS_REPLY_NAK,        // negative acknowledgment
} cms_reply_t;

typedef struct {
    void (*reinit)(u32 account);
    bool (*new_event)(event_t *e);              // increments seq, builds the packet
    bool (*packet_ready)(void);
    u16  (*packet_size)(void);
    void (*get_packet)(u8 *dest);
    void (*packet_done)(void);
    cms_reply_t (*packet_reply)(u8 *data, u16 len);
} cms_proto_t;

extern const cms_proto_t cms_proto_sia_ip;

#endif // ! CMS_PROTO_H

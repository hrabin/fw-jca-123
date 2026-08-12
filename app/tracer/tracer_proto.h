#ifndef TRACER_PROTO_H
#define TRACER_PROTO_H

#include "type.h"
#include "gps.h"
#include "tracer.h"

// Protocol interface for tracker communication.
// Each protocol (H02, SIA, ...) provides one const instance.
// Adding a new protocol means implementing this struct in a new .c file;
// tracer.c does not need to change.

typedef struct {
    void (*reinit)(u32 unit_id);
    void (*new_track)(u32 track);
    bool (*packet_ready)(void);
    void (*packet_done)(void);
    u16  (*packet_size)(void);
    bool (*packet_reply_ok)(u8 *data, u16 len);
    void (*new_point)(gps_stamp_t *pos, u16 track, bool last, track_info_t *info);

    // Optional — reserved for future use
    void (*get_packet)(u8 *dest);
    void (*track_end)(void);
    void (*rq_add_auth)(u8 id, ascii *data);
} tracer_proto_t;

extern const tracer_proto_t tracer_proto_h02;
extern const tracer_proto_t tracer_proto_sia;

#endif // ! TRACER_PROTO_H

#ifndef TRACER_BUFFER_H
#define TRACER_BUFFER_H

#include "type.h"
#include "gps.h"

#define TRACER_FLAG_DELIVERED (1 << 0) // this point was delivered to server
#define TRACER_FLAG_VALID     (1 << 1) // location is valid
#define TRACER_FLAG_LAST      (1 << 2) // last point of track

struct _tracer_buf_t {
    u16 track;
    u16 point;
    gps_stamp_t position;
    u32 info; // some additional info like alarms, inputs ...
    u8 flags;
    u8 chsum;
} PACK; // sizeof(tracer_buf_t) = 32

typedef struct _tracer_buf_t tracer_buf_t;

#define TRACER_BUF_VERSION 1 // by changing version is possible to invalidate all data in FLASH

extern bool tracer_buf_init (u16 *track, u32 *info);
extern bool tracer_buf_hard_erase (void);
extern bool tracer_buf_store_position (gps_stamp_t *pos, u16 track, u16 point, u8 flags, u32 info);
extern s16  tracer_buf_read_next (gps_stamp_t *pos, u16 *track, u16 *point, u8 *flags, u32 *info);
extern bool tracer_buf_delivered_all (void);
extern bool tracer_buf_empty (void);


#endif // ~TRACER_BUFFER_H

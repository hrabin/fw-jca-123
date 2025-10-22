#ifndef TRACER_H02_H
#define TRACER_H02_H

#include "type.h"
#include "gps.h"
#include "tracer.h"

void tracer_h02_reinit(u32 unit_id);
u32  tracer_h02_unit_id (void);
u16  tracer_h02_packet_size(void);
void tracer_h02_new_track(u32 track);
bool tracer_h02_packet_ready (void);
void tracer_h02_get_packet (u8 *dest);
void tracer_h02_packet_done (void);
bool tracer_h02_packet_reply_ok (u8 *data, u16 len);
void tracer_h02_new_point (gps_stamp_t *pos, u16 track, bool last, track_info_t *info);
void tracer_h02_track_end(void);
void tracer_h02_rq_add_auth(u8 id, ascii *data);

#endif // ! TRACER_H02_H



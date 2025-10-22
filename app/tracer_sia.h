#ifndef TRACER_SIA_H
#define	TRACER_SIA_H

#include "type.h"
#include "gps.h"
#include "tracer.h"


extern void tr_sia_reinit(u32 unit_id);
extern u32  tr_sia_unit_id (void);
extern u16  tr_sia_packet_size(void);
extern void tr_sia_new_track(u32 track);
extern bool tr_sia_packet_ready (void);
extern void tr_sia_get_packet (u8 *dest);
extern void tr_sia_packet_done (void);
extern bool tr_sia_packet_reply_ok (u8 *data, u16 len);
extern void tr_sia_new_point (gps_stamp_t *pos, u16 track, bool last, track_info_t *info);
extern void tr_sia_track_end(void);
extern void tr_sia_rq_add_auth(u8 id, ascii *data);

#endif // ~TRACER_SIA_H

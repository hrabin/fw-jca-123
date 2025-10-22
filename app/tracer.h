#ifndef TRACER_H
#define	TRACER_H

#include "type.h"

typedef union {
	struct {
		u32 driver_id:7;
		u32 track_type:1;
		u32 inputs:8;
		u32 outputs:8;
		u32 res:8;
	} s;
	u32 dw;
} track_info_t;

#define	TRACER_PACKET_BUFFER_SIZE (256)
extern u8 tracer_packet_buffer[];

bool tracer_init (void);
bool tracer_reinit (void);
void tracer_set_track_id (u16 id);
u16  tracer_get_track_id(void);
u32  tracer_unit_id (void);
void tracer_driver_id_needed (void);
bool tracer_set_id (u32 id);
void tracer_comm_process (void);
void tracer_start (void);
void tracer_stop (void);
bool tracer_test (void);
bool tracer_server_setup_ok (void);
bool tracer_setup_ok (void);
bool tracer_is_active (void);
bool tracer_wait_for_driver_id (void);
bool tracer_packet_rx (u8 *data, u16 len, u16 port);
void tracer_set_user_id (u8 id);
u8   tracer_get_user_id (void);
void tracer_shock_start(bool state);

void tracer_task (void);
void tracer_tick (void);

#endif // ! TRACER_H


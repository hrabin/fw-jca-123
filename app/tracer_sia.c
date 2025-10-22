#include "common.h"

#include "buf.h"
#include "tracer.h"
#include "tracer_sia.h"
#include "system.h" // inputs/outputs
#include "util.h"
#include "cms_sia_ip.h"
#include "event_defs.h"


cms_sia_ip_state_t  sia_state;

static bool sia_packet_ok = false;
static buf_t sia_packet;

void tr_sia_reinit(u32 unit_id)
{
	memset (&sia_state, 0, sizeof(cms_sia_ip_state_t));
	sia_state.enable_event_name = 1;
	sia_state.enable_event_time = 1;
	sia_state.enable_time_stamp = 1;
	sia_state.enable_time_sync  = 1;
	sia_state.object_id = unit_id;

 	buf_init(&sia_packet, (char *)tracer_packet_buffer, TRACER_PACKET_BUFFER_SIZE);
}

u32  tr_sia_unit_id (void)
{
	return (sia_state.object_id);
}

u16  tr_sia_packet_size(void)
{
	return (buf_length(&sia_packet));
}

void tr_sia_new_track(u32 track)
{
}

bool tr_sia_packet_ready (void)
{
	return (sia_packet_ok);
}

void tr_sia_get_packet (u8 *dest)
{
	memcpy (dest, buf_data(&sia_packet), buf_length(&sia_packet));
}

void tr_sia_packet_done (void)
{
	buf_adjust (&sia_packet, 0); // reset to position zero
	sia_packet_ok = false;
}

bool tr_sia_packet_reply_ok (u8 *data, u16 len)
{
	int a,b,c;

	// if (sscanf(msg, "ACK\"%dL%X#%X[", &a, &b, &c) == 3)
	// else if (sscanf(msg, "NAK\"%dL%X", &a, &b) == 2)
	
	if (! cms_sia_ip_rx(&sia_state, data, len))
		return (false);

	data += SIA_IDX_DATA;
	
	if (sscanf((char *)data, "\"ACK\"%dL%X#%X[", &a, &b, &c) == 3)
	{
		if (a != sia_state.cnt)
		{	// neni to odpoved na moji zpravu
			LOG_ERROR("SIA ACK cnt");
			return (false);
		}
	}
	else if (sscanf((char *)data, "\"NAK\"%dL%X", &a, &b) == 2)
	{
		LOG_ERROR("SIA NACK");
		// muze to byt NACK z duvodu ujeteho casu
		// return (false);
	}
	else
	{
		LOG_ERROR("SIA RX");
		// return (false);
	}
	
	// TODO
	sia_state.cnt++;
	return (true);
}

void tr_sia_new_point (gps_stamp_t *pos, u16 track, bool last, track_info_t *info)
{
#define	SIA_MAX_DATA_LEN 48

	ascii data[SIA_MAX_DATA_LEN];
	event_t e;
	buf_t buf;

	s32 a, s, m, f;
	char c;
	// SIA location :
	// Longitude "X" "[X093W23.456]"
	// Latitude  "Y" "[Y45N23.456]"
	// Altitude  "Z" "[Z123.2M]"
	// 

	if (sia_packet_ok)
	{
		LOG_ERROR ("pending packet");
		return;
	}
 	e.evt = EVENT_TRACKING;
	e.src = SOURCE_SELF;
	e.time.dw = pos->time.dw;
	
	buf_init(&buf, data, SIA_MAX_DATA_LEN);

	// podle standardu SIA je to ukecane a navic chybi rychlost atd.
	a = pos->lon_sec; c = 'E';
	if (a<0) { a=0-a; c = 'W'; }
	s = a/(60*60*100); a%=(60*60*100); // stupne
	m = a/(60*100); a%=(60*100); // minuty
	f = a/60; // zlomek minut
	buf_append_fmt (&buf, "[X%03d%c%02d.%02d0]", s, c, m, f);

	a = pos->lat_sec; c = 'N';
	if (a<0) { a=0-a; c = 'S'; }
	s = a/(60*60*100); a%=(60*60*100); // stupne
	m = a/(60*100); a%=(60*100); // minuty
	f = a/60;  // zlomek minut
	buf_append_fmt (&buf, "[Y%02d%c%02d.%02d0]", s, c, m, f);
	
	a = pos->alt;
	buf_append_fmt (&buf, "[Z%04dM]", a);

	cms_sia_ip_build_msg (&sia_state, &sia_packet, &e, data);
	
	sia_packet_ok = true;
}

void tr_sia_track_end(void)
{
}

void tr_sia_rq_add_auth(u8 id, ascii *data)
{
}



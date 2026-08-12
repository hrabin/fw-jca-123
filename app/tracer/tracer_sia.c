#include "common.h"

#include "buf.h"
#include "tracer.h"
#include "tracer_sia.h"
#include "tracer_proto.h"
#include "cms_sia_ip.h"
#include "event.h"
#include "log.h"

LOG_DEF("SIA");

static cms_sia_ip_state_t sia_state;
static bool sia_packet_ok = false;
static buf_t sia_packet;

void tr_sia_reinit(u32 unit_id)
{
    cms_sia_ip_init(&sia_state);
    sia_state.account = unit_id;

    buf_init(&sia_packet, (char *)tracer_packet_buffer, TRACER_PACKET_BUFFER_SIZE);
    sia_packet_ok = false;
}

u32 tr_sia_unit_id (void)
{
    return (sia_state.account);
}

u16 tr_sia_packet_size(void)
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
    u16 ack_seq;
    bool is_nak;

    if (! cms_sia_ip_rx_ack(&sia_state, data, len, &ack_seq, &is_nak))
        return (false);

    if (ack_seq != sia_state.cnt)
    {   // not the reply to our message
        LOG_ERROR("SIA ACK seq mismatch %d != %d", ack_seq, sia_state.cnt);
        return (false);
    }

    if (is_nak)
        LOG_ERROR("SIA NAK");

    return (true);
}

void tr_sia_new_point (gps_stamp_t *pos, u16 track, bool last, track_info_t *info)
{
#define SIA_MAX_DATA_LEN 48

    ascii data[SIA_MAX_DATA_LEN];
    event_t e;
    buf_t buf;

    s32 a, s, m, f;
    ascii c;
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

    e.id = EVENT_ID_TRACKING;
    e.source = EVENT_SOURCE_UNIT;
    e.time = pos->time;

    if (++sia_state.cnt > 9999)
        sia_state.cnt = 1;

    buf_init(&buf, data, SIA_MAX_DATA_LEN);

    // longitude
    a = pos->lon_sec; c = 'E';
    if (a<0) { a = 0 - a; c = 'W'; }
    s = a/(60*60*100); a%=(60*60*100); // degrees
    m = a/(60*100); a%=(60*100);       // minutes
    f = a/60;                          // fraction of minutes
    buf_append_fmt (&buf, "[X%03d%c%02d.%02d0]", s, c, m, f);

    // latitude
    a = pos->lat_sec; c = 'N';
    if (a<0) { a = 0 - a; c = 'S'; }
    s = a/(60*60*100); a%=(60*60*100);
    m = a/(60*100); a%=(60*100);
    f = a/60;
    buf_append_fmt (&buf, "[Y%02d%c%02d.%02d0]", s, c, m, f);

    // altitude
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

const tracer_proto_t tracer_proto_sia = {
    .reinit          = tr_sia_reinit,
    .new_track       = tr_sia_new_track,
    .packet_ready    = tr_sia_packet_ready,
    .packet_done     = tr_sia_packet_done,
    .packet_size     = tr_sia_packet_size,
    .packet_reply_ok = tr_sia_packet_reply_ok,
    .new_point       = tr_sia_new_point,
    .get_packet      = tr_sia_get_packet,
    .track_end       = tr_sia_track_end,
    .rq_add_auth     = tr_sia_rq_add_auth,
};

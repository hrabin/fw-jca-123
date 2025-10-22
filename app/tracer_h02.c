#include "common.h"
#include "tracer_h02.h"
#include "parse.h"
#include "log.h"

LOG_DEF("H02");

// Traccar: ID: 1000000001, port 5013
// /opt/traccar/logs/tracker-server.log

static u32 _unit_id = 0;
static size_t _packet_len;

void tracer_h02_reinit(u32 unit_id)
{
    _unit_id = unit_id;

    _packet_len = 0;
}

u32 tracer_h02_unit_id (void)
{
    return (_unit_id);
}

u16 tracer_h02_packet_size(void)
{
    return (_packet_len + 1);
}

void tracer_h02_new_track(u32 track)
{

}

bool tracer_h02_packet_ready (void)
{
    return (_packet_len ? true : false);
}

void tracer_h02_packet_done (void)
{
    _packet_len = 0;
}

bool tracer_h02_packet_reply_ok (u8 *data, u16 len)
{
    // *HQ,1000000001,V4,V1,20240930174037#
    char *p;
    parse_number_t id;

    if (len < 11)
        return (false);

    if ((p = parse_pattern((char *)data, "*HQ,")) == NULL)
        return (false);

    LOG_DEBUGL(LOG_SELECT_H02, "ack: \"%s\"", (char *)data);

    if ((p = parse_number(&id, p)) == NULL)
        return (false);

    if (id != _unit_id)
    {
        LOG_ERROR("unknown ID");
        return (false);
    }
    if ((p = parse_pattern(p, ",V4,V1")) == NULL)
    {
        LOG_ERROR("no ACK");
        return (false);
    }
    // 
    return (true);
}

#define H02_STATUS_CUT_OFF_ENGINE (1 << (3 + 0*8))
#define H02_STATUS_DOOR           (1 << (0 + 2*8))


void tracer_h02_new_point (gps_stamp_t *pos, u16 track, bool last, track_info_t *info)
{
    // *HQ,1000000001,V1,102411,A,5123.1549,N,01123.3183,E,0.00,333,011024,BFFFFBFF#
    u32 sec, dg, min, sub_min, s;
    float f;
    u32 status = 0xFFFFFFFF;
    char symbol;
    buf_t buf;
    buf_init(&buf, (char *)tracer_packet_buffer, TRACER_PACKET_BUFFER_SIZE);

    // packet ID and device ID
    buf_append_fmt(&buf, "*HQ,%" PRIu32 ",V1,", _unit_id);
    // Time
    buf_append_fmt(&buf, "%02d%02d%02d,", pos->time.hour, pos->time.minute, pos->time.second);

    // LAT:
    symbol = pos->lat_sec >= 0 ? 'N' : 'S';
    sec = ABS(pos->lat_sec);

    // A/V/B == data valid/data invalid/compas
    buf_append_fmt(&buf, "%c,", pos->fix == 0 ? 'V' : 'A');

    dg  = sec / (6000UL*60);
    sec = sec % (6000UL*60);
    min = sec / 6000;
    sec = sec % 6000;
    sub_min = 10000 * sec / 6000;

    buf_append_fmt(&buf, "%02d%02d.%04d,%c,", dg, min, sub_min, symbol);

    // LON:
    symbol = pos->lat_sec >= 0 ? 'E' : 'W';
    sec = ABS(pos->lon_sec);

    dg  = sec / (6000UL*60);
    sec = sec % (6000UL*60);
    min = sec / 6000;
    sec = sec % 6000;
    sub_min = 10000 * sec / 6000;

    buf_append_fmt(&buf, "%03d%02d.%04d,%c,", dg, min, sub_min, symbol);  

    // Speed (in knots, 1Kn=1.852 km/h)
    f = pos->speed; // my speed is in km/h*10
    f = 10 * f / 1.852;
    s = f;
    buf_append_fmt(&buf, "%03d.%02d,", s/100, s%100);

    // Direction
    buf_append_fmt(&buf, "%03d,", pos->angle);
    
    // Date
    if (pos->fix == 0)
    {   // sent invalid date when no fix to be sure not accepted by server
        // Traccar accepted zero coordinates even vit "V" as invalid
        buf_append_str(&buf, "010100,");
    }
    else
    {
        buf_append_fmt(&buf, "%02d%02d%02d,", pos->time.day, pos->time.month, pos->time.year);
    }
    
    // additiona info (alarms)
    buf_append_fmt(&buf, "%08X", status); // TODO: set values, use 'track_info_t';

    // end of data
    buf_append_str(&buf, "#");

    LOG_DEBUGL(LOG_SELECT_H02, "pkt: \"%s\"", (char *)tracer_packet_buffer);

    _packet_len = buf_length(&buf) - 1; // dont send trailing "\0"
}

void tracer_h02_track_end(void)
{
}

void tracer_h02_rq_add_auth(u8 id, ascii *data)
{
}

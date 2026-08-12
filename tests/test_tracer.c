#include "test.h"
#include "tracer.h"
#include "tracer_h02.h"
#include "tracer_sia.h"
#include "tracer_proto.h"
#include "cms_sia_ip.h"
#include <string.h>

// Tracker protocol build tests — H02 (Traccar) and SIA (SIA-DC-09).

// tracer.c is not compiled in tests — provide its shared buffer
u8 tracer_packet_buffer[TRACER_PACKET_BUFFER_SIZE];

static void _pos_set(gps_stamp_t *pos)
{
    memset(pos, 0, sizeof(*pos));
    pos->time.hour   = 10;
    pos->time.minute = 24;
    pos->time.second = 11;
    pos->time.day    = 5;
    pos->time.month  = 10;
    pos->time.year   = 24;
    pos->lat_sec = 18270000;   // 50°45'00.0000' N
    pos->lon_sec = 5580000;    // 15°30'00.0000' E
    pos->fix  = 1;
    pos->speed = 185;          // 18.5 km/h → 9.98 knots*100
    pos->angle = 333;
}

// ---- H02 (Traccar) ----

TEST(h02_build_point)
{
    gps_stamp_t pos;
    track_info_t info;

    tracer_h02_reinit(1000000001);
    info.dw = 0;

    _pos_set(&pos);
    tracer_h02_new_point(&pos, 1, false, &info);

    ASSERT(tracer_h02_packet_ready());
    ASSERT_EQ(tracer_h02_packet_size(), strlen((char *)tracer_packet_buffer));
    ASSERT_STREQ((char *)tracer_packet_buffer,
                 "*HQ,1000000001,V1,102411,A,5045.0000,N,01530.0000,E,009.98,333,051024,FFFFFFFF#");
}

TEST(h02_build_no_fix)
{
    gps_stamp_t pos;
    track_info_t info;

    tracer_h02_reinit(1000000001);
    info.dw = 0;

    _pos_set(&pos);
    pos.fix = 0;   // no fix — 'V' flag and invalid date
    tracer_h02_new_point(&pos, 1, false, &info);

    ASSERT_STREQ((char *)tracer_packet_buffer,
                 "*HQ,1000000001,V1,102411,V,5045.0000,N,01530.0000,E,009.98,333,010100,FFFFFFFF#");
}

TEST(h02_packet_done)
{
    gps_stamp_t pos;
    track_info_t info;

    tracer_h02_reinit(1000000001);
    info.dw = 0;

    _pos_set(&pos);
    tracer_h02_new_point(&pos, 1, false, &info);
    ASSERT(tracer_h02_packet_ready());

    tracer_h02_packet_done();
    ASSERT(!tracer_h02_packet_ready());
    ASSERT_EQ(tracer_h02_packet_size(), 1);   // only the trailing NUL
}

TEST(h02_ack_valid)
{
    tracer_h02_reinit(1000000001);

    // *HQ,<id>,V4,V1,<timestamp>#
    ASSERT(tracer_h02_packet_reply_ok((u8 *)"*HQ,1000000001,V4,V1,20240930174037#",
                                      strlen("*HQ,1000000001,V4,V1,20240930174037#")));
}

TEST(h02_ack_wrong_id)
{
    tracer_h02_reinit(1000000001);

    ASSERT(!tracer_h02_packet_reply_ok((u8 *)"*HQ,9999999999,V4,V1,20240930174037#",
                                       strlen("*HQ,9999999999,V4,V1,20240930174037#")));
}

TEST(h02_ack_wrong_version)
{
    tracer_h02_reinit(1000000001);

    ASSERT(!tracer_h02_packet_reply_ok((u8 *)"*HQ,1000000001,V4,V2,20240930174037#",
                                       strlen("*HQ,1000000001,V4,V2,20240930174037#")));
}

TEST(h02_ack_too_short)
{
    tracer_h02_reinit(1000000001);

    ASSERT(!tracer_h02_packet_reply_ok((u8 *)"*HQ,1,V4,V1#", 11));
}

TEST(h02_proto_lifecycle)
{
    gps_stamp_t pos;
    track_info_t info;
    u8 dest[TRACER_PACKET_BUFFER_SIZE];

    tracer_proto_h02.reinit(1000000001);
    info.dw = 0;

    _pos_set(&pos);
    tracer_proto_h02.new_point(&pos, 1, false, &info);

    ASSERT(tracer_proto_h02.packet_ready());
    u16 len = tracer_proto_h02.packet_size();
    ASSERT(len > 0);

    // H02 writes directly into the shared buffer, get_packet is NULL
    ASSERT(tracer_proto_h02.get_packet == NULL);
    memcpy(dest, tracer_packet_buffer, len);

    ASSERT(tracer_proto_h02.packet_reply_ok((u8 *)"*HQ,1000000001,V4,V1,20240930174037#",
                                            strlen("*HQ,1000000001,V4,V1,20240930174037#")));

    tracer_proto_h02.packet_done();
    ASSERT(!tracer_proto_h02.packet_ready());
}

// ---- SIA (tracking) ----

TEST(sia_build_track_point)
{
    gps_stamp_t pos;
    track_info_t info;
    u8 dest[TRACER_PACKET_BUFFER_SIZE];
    u16 len;

    tr_sia_reinit(0x1234);
    info.dw = 0;

    _pos_set(&pos);
    pos.lon_sec = -5580000;   // 15°30' W
    pos.alt = 330;
    tr_sia_new_point(&pos, 1, false, &info);

    ASSERT(tr_sia_packet_ready());

    len = tr_sia_packet_size();
    tr_sia_get_packet(dest);

    // location extensions X/Y/Z and the TL event code
    ASSERT(strstr((char *)dest, "\"SIA-DCS\"0001L0#1234[|Nri01/TL0^^]") != NULL);
    ASSERT(strstr((char *)dest, "[X015W30.000][Y50N45.000][Z0330M]") != NULL);

    // the built frame must pass the SIA validation
    ASSERT(cms_sia_ip_rx(dest, len));
}

// Build a raw SIA frame with correct CRC-16 (reference implementation)
static u16 _sia_frame(u8 *out, const ascii *data)
{
    u16 len = strlen(data);
    u16 crc = 0;
    u8 ch;
    const u8 *p = (const u8 *)data;

    while ((ch = *p++) != '\0')
    {
        u8 i;

        crc ^= ch;
        for (i = 0; i < 8; i++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }

    out[0] = 0x0A;
    sprintf((char *)out + SIA_IDX_CRC, "%04X", crc);
    sprintf((char *)out + SIA_IDX_LEN, "0%03X", len);
    memcpy(out + SIA_IDX_DATA, data, len);
    out[SIA_IDX_DATA + len] = 0x0D;
    return (SIA_IDX_DATA + len + 1);
}

TEST(sia_packet_reply)
{
    gps_stamp_t pos;
    track_info_t info;
    u8 ack[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 ack_len;

    tr_sia_reinit(0x1234);
    info.dw = 0;

    _pos_set(&pos);
    tr_sia_new_point(&pos, 1, false, &info);

    // valid ACK for seq 1
    ack_len = _sia_frame(ack, "\"ACK\"0001L0#1234[]");
    ASSERT(tr_sia_packet_reply_ok(ack, ack_len));

    // ACK for the wrong sequence number (valid CRC, wrong seq)
    ack_len = _sia_frame(ack, "\"ACK\"0007L0#1234[]");
    ASSERT(!tr_sia_packet_reply_ok(ack, ack_len));
}

TEST(sia_proto_lifecycle)
{
    gps_stamp_t pos;
    track_info_t info;

    tracer_proto_sia.reinit(0x1234);
    info.dw = 0;

    _pos_set(&pos);
    tracer_proto_sia.new_point(&pos, 1, false, &info);

    ASSERT(tracer_proto_sia.packet_ready());
    ASSERT(tracer_proto_sia.packet_size() > 0);
    ASSERT(tracer_proto_sia.get_packet != NULL);

    tracer_proto_sia.packet_done();
    ASSERT(!tracer_proto_sia.packet_ready());
}

#include "common.h"
#include "cms_sia_ip.h"
#include "cms_proto.h"
#include "rtc.h"
#include "cfg.h"
#include "log.h"

LOG_DEF("CMS");

// SIA event code translation — our event ids to standard SIA 2-letter codes.
// Events with code 0 are not reported.
typedef struct {
    event_id_e id;
    const ascii *code;
} _sia_event_t;

static const _sia_event_t _SIA_EVENT_TABLE[] = {
    {EVENT_ID_BOOT_UP,          "RR"},   // system reset
    {EVENT_ID_SHUT_DOWN,        NULL},   // not reported
    {EVENT_ID_POWER_FAIL,       "AT"},   // AC trouble
    {EVENT_ID_POWER_RECOVERY,   "AR"},   // AC restore
    {EVENT_ID_LO_BATT,          "YT"},   // battery trouble
    {EVENT_ID_LO_BATT_RECOVERY, "YR"},   // battery restore
    {EVENT_ID_SET,              "CL"},   // closing report (arm)
    {EVENT_ID_UNSET,            "OP"},   // opening report (disarm)
    {EVENT_ID_ALARM,            "BA"},   // burglary alarm
    {EVENT_ID_ALARM_CANCEL,     "BC"},   // burglary cancel
    {EVENT_ID_ALARM_TIMEOUT,    NULL},   // not reported
    {EVENT_ID_FAULT,            "UT"},   // untested trouble
    {EVENT_ID_FAULT_RECOVERY,   "UR"},   // untested restore
    {EVENT_ID_JAMMING_ACT,      "XQ"},   // RF jamming
    {EVENT_ID_JAMMING_DACT,     "XH"},   // RF jamming restore
    {EVENT_ID_TRACKING,         "TL"},   // track location (position report)
};

// SIA zone id translation — our event sources to a single hex digit.
typedef struct {
    event_source_e source;
    u8 zone;
} _sia_source_t;

static const _sia_source_t _SIA_SOURCE_TABLE[] = {
    {EVENT_SOURCE_UNKNOWN,  0x0},
    {EVENT_SOURCE_UNIT,     0x0},
    {EVENT_SOURCE_KEY,      0x1},
    {EVENT_SOURCE_DOOR,     0x2},
    {EVENT_SOURCE_INPUT1,   0x3},
    {EVENT_SOURCE_SHOCK,    0x4},
    {EVENT_SOURCE_LOCK,     0x5},
    {EVENT_SOURCE_CELL,     0x6},
    {EVENT_SOURCE_BATTERY,  0x7},
    {EVENT_SOURCE_ADMIN,    0x8},
    {EVENT_SOURCE_USER1,    0x9},
    {EVENT_SOURCE_USER2,    0xA},
    {EVENT_SOURCE_USER3,    0xB},
    {EVENT_SOURCE_USER4,    0xC},
};

// text ids for event/source names (mirrors event.c tables)
static const cfg_id_t _EVENT_TEXT_ID[] = {
    CFG_ID_TEXT_EVENT_NAME_000,   // BOOT_UP
    CFG_ID_TEXT_EVENT_NAME_001,   // SHUT_DOWN
    CFG_ID_TEXT_EVENT_NAME_002,   // POWER_FAIL
    CFG_ID_TEXT_EVENT_NAME_003,   // POWER_RECOVERY
    CFG_ID_TEXT_EVENT_NAME_004,   // LO_BATT
    CFG_ID_TEXT_EVENT_NAME_005,   // LO_BATT_RECOVERY
    CFG_ID_TEXT_EVENT_NAME_006,   // SET
    CFG_ID_TEXT_EVENT_NAME_007,   // UNSET
    CFG_ID_TEXT_EVENT_NAME_008,   // ALARM
    CFG_ID_TEXT_EVENT_NAME_010,   // ALARM_CANCEL
    CFG_ID_TEXT_EVENT_NAME_011,   // ALARM_TIMEOUT
    CFG_ID_TEXT_EVENT_NAME_012,   // FAULT
    CFG_ID_TEXT_EVENT_NAME_013,   // FAULT_RECOVERY
    CFG_ID_TEXT_EVENT_NAME_014,   // JAMMING_ACT
    CFG_ID_TEXT_EVENT_NAME_015,   // JAMMING_DACT
    CFG_ID_TEXT_EVENT_NAME_016,   // TRACKING
};

static const cfg_id_t _SOURCE_TEXT_ID[] = {
    CFG_ID_TEXT_SOURCE_NAME_000,   // UNKNOWN
    CFG_ID_TEXT_SOURCE_NAME_001,   // UNIT
    CFG_ID_TEXT_SOURCE_NAME_002,   // KEY
    CFG_ID_TEXT_SOURCE_NAME_003,   // DOOR
    CFG_ID_TEXT_SOURCE_NAME_004,   // INPUT1
    CFG_ID_TEXT_SOURCE_NAME_005,   // SHOCK
    CFG_ID_TEXT_SOURCE_NAME_006,   // LOCK
    CFG_ID_TEXT_SOURCE_NAME_007,   // CELL
    CFG_ID_TEXT_SOURCE_NAME_008,   // BATTERY
    CFG_ID_ADMIN_NAME,             // ADMIN
    CFG_ID_USER1_NAME,             // USER1
    CFG_ID_USER2_NAME,             // USER2
    CFG_ID_USER3_NAME,             // USER3
    CFG_ID_USER4_NAME,             // USER4
};

static const ascii *_sia_event_code (event_id_e id)
{
    u8 i;

    for (i = 0; i < sizeof(_SIA_EVENT_TABLE)/sizeof(_SIA_EVENT_TABLE[0]); i++)
    {
        if (_SIA_EVENT_TABLE[i].id == id)
            return (_SIA_EVENT_TABLE[i].code);
    }
    return (NULL);
}

static u8 _sia_zone (event_source_e source)
{
    u8 i;

    for (i = 0; i < sizeof(_SIA_SOURCE_TABLE)/sizeof(_SIA_SOURCE_TABLE[0]); i++)
    {
        if (_SIA_SOURCE_TABLE[i].source == source)
            return (_SIA_SOURCE_TABLE[i].zone);
    }
    return (0);
}

static const ascii *_sia_text (cfg_id_t id)
{
    return (cfg_read_static(id));
}

// ---- CRC-16 (per SIA spec, table-driven) ----

static const u16 SIA_IP_CRC_TABLE[] = {
    0x0000,0xc0c1,0xc181,0x0140,0xc301,0x03c0,0x0280,0xc241,
    0xc601,0x06c0,0x0780,0xc741,0x0500,0xc5c1,0xc481,0x0440,
    0xcc01,0x0cc0,0x0d80,0xcd41,0x0f00,0xcfc1,0xce81,0x0e40,
    0x0a00,0xcac1,0xcb81,0x0b40,0xc901,0x09c0,0x0880,0xc841,
    0xd801,0x18c0,0x1980,0xd941,0x1b00,0xdbc1,0xda81,0x1a40,
    0x1e00,0xdec1,0xdf81,0x1f40,0xdd01,0x1dc0,0x1c80,0xdc41,
    0x1400,0xd4c1,0xd581,0x1540,0xd701,0x17c0,0x1680,0xd641,
    0xd201,0x12c0,0x1380,0xd341,0x1100,0xd1c1,0xd081,0x1040,
    0xf001,0x30c0,0x3180,0xf141,0x3300,0xf3c1,0xf281,0x3240,
    0x3600,0xf6c1,0xf781,0x3740,0xf501,0x35c0,0x3480,0xf441,
    0x3c00,0xfcc1,0xfd81,0x3d40,0xff01,0x3fc0,0x3e80,0xfe41,
    0xfa01,0x3ac0,0x3b80,0xfb41,0x3900,0xf9c1,0xf881,0x3840,
    0x2800,0xe8c1,0xe981,0x2940,0xeb01,0x2bc0,0x2a80,0xea41,
    0xee01,0x2ec0,0x2f80,0xef41,0x2d00,0xedc1,0xec81,0x2c40,
    0xe401,0x24c0,0x2580,0xe541,0x2700,0xe7c1,0xe681,0x2640,
    0x2200,0xe2c1,0xe381,0x2340,0xe101,0x21c0,0x2080,0xe041,
    0xa001,0x60c0,0x6180,0xa141,0x6300,0xa3c1,0xa281,0x6240,
    0x6600,0xa6c1,0xa781,0x6740,0xa501,0x65c0,0x6480,0xa441,
    0x6c00,0xacc1,0xad81,0x6d40,0xaf01,0x6fc0,0x6e80,0xae41,
    0xaa01,0x6ac0,0x6b80,0xab41,0x6900,0xa9c1,0xa881,0x6840,
    0x7800,0xb8c1,0xb981,0x7940,0xbb01,0x7bc0,0x7a80,0xba41,
    0xbe01,0x7ec0,0x7f80,0xbf41,0x7d00,0xbdc1,0xbc81,0x7c40,
    0xb401,0x74c0,0x7580,0xb541,0x7700,0xb7c1,0xb681,0x7640,
    0x7200,0xb2c1,0xb381,0x7340,0xb101,0x71c0,0x7080,0xb041,
    0x5000,0x90c1,0x9181,0x5140,0x9301,0x53c0,0x5280,0x9241,
    0x9601,0x56c0,0x5780,0x9741,0x5500,0x95c1,0x9481,0x5440,
    0x9c01,0x5cc0,0x5d80,0x9d41,0x5f00,0x9fc1,0x9e81,0x5e40,
    0x5a00,0x9ac1,0x9b81,0x5b40,0x9901,0x59c0,0x5880,0x9841,
    0x8801,0x48c0,0x4980,0x8941,0x4b00,0x8bc1,0x8a81,0x4a40,
    0x4e00,0x8ec1,0x8f81,0x4f40,0x8d01,0x4dc0,0x4c80,0x8c41,
    0x4400,0x84c1,0x8581,0x4540,0x8701,0x47c0,0x4680,0x8641,
    0x8201,0x42c0,0x4380,0x8341,0x4100,0x81c1,0x8081,0x4040,
};

static u16 _crc_calc (u8 *data)
{
    u16 i;
    u16 crc = 0;
    u8 ch;

    for (i = 0; i < CMS_SIA_IP_MAX_MSG_LEN; i++)
    {
        ch = (*data++);
        if ((ch == '\0')     // freshly built message
         || (ch == 0x0D))    // normal end of message
            break;

        crc = (crc >> 8) ^ SIA_IP_CRC_TABLE[ch ^ (crc & 0xFF)];
    }
    return (crc);
}

void cms_sia_ip_init (cms_sia_ip_state_t *s)
{
    memset (s, 0, sizeof(cms_sia_ip_state_t));
    s->enable_event_name = 1;
    s->enable_event_time = 1;
    s->enable_time_stamp = 1;
    s->cnt = 0;   // the first new_event() increments to 1
}

static bool _sia_msg_add_event (buf_t *dest, event_t *e)
{
    const ascii *code;
    u8 zone;

    if ((code = _sia_event_code(e->id)) == NULL)
        return (false);

    zone = _sia_zone(e->source);

    // data field: Nri01/<CODE><zone>^<source name>^
    buf_append_fmt(dest, "|Nri01/%s%X^", code, zone);
    if (_sia_text(_SOURCE_TEXT_ID[e->source]) != NULL)
    {
        buf_append_str(dest, _sia_text(_SOURCE_TEXT_ID[e->source]));
    }
    buf_append_str(dest, "^");
    return (true);
}

static void _sia_msg_add_eventname (buf_t *dest, event_t *e)
{
    buf_append_str(dest, "[I");
    if (_sia_text(_EVENT_TEXT_ID[e->id]) != NULL)
    {
        buf_append_str(dest, _sia_text(_EVENT_TEXT_ID[e->id]));
    }
    buf_append_str(dest, "]");
}

static void _sia_msg_add_eventtime (buf_t *dest, event_t *e)
{
    rtc_t *t = &e->time;

    buf_append_fmt (dest, "[H%d:%02d:%02d]", t->hour, t->minute, t->second);
}

static void _sia_msg_add_timestamp (buf_t *dest)
{
    // The CSR validates the timestamp against its own GMT reference.
    // Format: <_HH:MM:SS,MM-DD-YYYY>, length is exactly 20 characters.
    rtc_t t;

    rtc_get_time(&t);
    buf_append_fmt (dest, "_%02d:%02d:%02d,%02d-%02d-%d",
                    t.hour, t.minute, t.second, t.month, t.day, 2000+t.year);
}

u16 cms_sia_ip_build_msg (cms_sia_ip_state_t *s, buf_t *dataout, event_t *e, ascii *data)
{
    u8 *data_ptr;
    u16 crc;
    u16 out_len;

    // message template (SIA-DC-09):
    // <LF><crc><0LLL>
    // <"SIA-DCS"><seq>L<prefix>#<acct>[<data>]<timestamp>
    // <CR>
    buf_adjust (dataout, SIA_IDX_DATA);
    buf_append_fmt (dataout, "\"SIA-DCS\"%04dL%X#%X[", s->cnt, s->prefix, s->account);
    data_ptr = (u8 *)(buf_data(dataout) + SIA_IDX_DATA);

    if (! _sia_msg_add_event(dataout, e))
    {
        LOG_ERROR("cms_sia_ip_build_msg() adding event");
        return (0);
    }
    buf_append_char (dataout, ']');

    if (s->enable_event_name)
    {
        _sia_msg_add_eventname(dataout, e);
    }
    if (data != NULL)
    {   // additional information (i.e. GPS position)
        buf_append_str(dataout, data);
    }
    if (s->enable_event_time)
    {
        _sia_msg_add_eventtime(dataout, e);
    }
    if (s->enable_time_stamp)
    {
        _sia_msg_add_timestamp(dataout);
    }

    out_len = buf_length(dataout) - SIA_IDX_DATA;  // logical data length
    crc = _crc_calc(data_ptr);

    OS_ASSERT ((SIA_IDX_DATA + out_len) < CMS_SIA_IP_MAX_MSG_LEN, "cms_sia_ip_build_msg() len");

    data_ptr = (u8 *)buf_data(dataout);
    data_ptr[0] = '\x0A';  // LF

    // CRC is stored as 4 hex chars, big-endian value
    sprintf((ascii *)(data_ptr + SIA_IDX_CRC), "%04X", crc);
    sprintf((ascii *)(data_ptr + SIA_IDX_LEN), "0%03X", out_len);
    *(data_ptr + SIA_IDX_DATA) = '"';  // sprintf above overwrote the quote with NUL
    buf_append_char (dataout, '\x0d'); // CR

    return (buf_length(dataout));
}

bool cms_sia_ip_rx (u8 *udpdata, u16 len)
{
    u16 crc;
    u16 msg_len;

    if (len < 20)
        return (false);

    if (udpdata[0] != 0x0A)
        return (false);  // every message starts with LF

    if (len > CMS_SIA_IP_MAX_MSG_LEN)
    {
        LOG_ERROR("cms_sia_ip_rx() too long");
        return (false);
    }

    udpdata[len] = '\0';

    if (sscanf((char *)udpdata + SIA_IDX_CRC, "%04" SCNx16, &crc) != 1)
        return (false);

    if (sscanf((char *)udpdata + SIA_IDX_LEN, "%04" SCNx16, &msg_len) != 1)
        return (false);

    // the length field is "0LLL" — the leading 0 is part of the hex value
    if ((msg_len & 0xF000) != 0)
        return (false);

    if (crc != _crc_calc(udpdata + SIA_IDX_DATA))
    {
        LOG_ERROR("SIA CRC failed");
        return (false);
    }

    if ((len - SIA_IDX_DATA - 1) != (msg_len & 0x0FFF))
    {
        LOG_ERROR("SIA LEN failed");
        return (false);
    }

    return (true);
}

bool cms_sia_ip_rx_ack (cms_sia_ip_state_t *s, u8 *udpdata, u16 len,
                        u16 *ack_seq, bool *is_nak)
{
    u32 a, b, c;

    if (! cms_sia_ip_rx(udpdata, len))
        return (false);

    if (strncmp((char *)udpdata + SIA_IDX_DATA, "\"ACK\"", 5) == 0)
    {
        if (sscanf((char *)udpdata + SIA_IDX_DATA, "\"ACK\"%" SCNu32 "L%" SCNx32 "#%" SCNx32 "[",
                   &a, &b, &c) == 3)
        {
            if ((b == s->prefix) && (c == s->account))
            {
                *ack_seq = a & 0xFFFF;
                *is_nak = false;
                return (true);
            }
        }
        return (false);
    }

    if (strncmp((char *)udpdata + SIA_IDX_DATA, "\"NAK\"", 5) == 0)
    {
        if (sscanf((char *)udpdata + SIA_IDX_DATA, "\"NAK\"%" SCNu32 "L%" SCNx32,
                   &a, &b) == 2)
        {
            if (b == s->prefix)
            {
                *ack_seq = a & 0xFFFF;
                *is_nak = true;
                return (true);
            }
        }
        return (false);
    }

    return (false);  // not an ACK/NAK frame
}

// ---- cms_proto_t interface implementation ----

static cms_sia_ip_state_t _sia;
static u8 _sia_packet[CMS_SIA_IP_MAX_PACKET_LEN];
static buf_t _sia_packet_buf;
static bool _sia_packet_ok = false;

static void _sia_proto_reinit(u32 account)
{
    cms_sia_ip_init(&_sia);
    _sia.account = account;
    _sia_packet_ok = false;
}

static bool _sia_proto_new_event(event_t *e)
{
    if (_sia_packet_ok)
    {
        LOG_ERROR("pending packet");
        return (false);
    }

    if (++_sia.cnt > 9999)
        _sia.cnt = 1;

    buf_init(&_sia_packet_buf, (char *)_sia_packet, sizeof(_sia_packet));

    if (cms_sia_ip_build_msg(&_sia, &_sia_packet_buf, e, NULL) == 0)
        return (false);  // event not reportable by this protocol

    _sia_packet_ok = true;
    return (true);
}

static bool _sia_proto_packet_ready(void)
{
    return (_sia_packet_ok);
}

static u16 _sia_proto_packet_size(void)
{
    return (buf_length(&_sia_packet_buf));
}

static void _sia_proto_get_packet(u8 *dest)
{
    memcpy(dest, _sia_packet, buf_length(&_sia_packet_buf));
}

static void _sia_proto_packet_done(void)
{
    _sia_packet_ok = false;
}

static cms_reply_t _sia_proto_packet_reply(u8 *data, u16 len)
{
    u16 ack_seq;
    bool is_nak;

    if (! cms_sia_ip_rx_ack(&_sia, data, len, &ack_seq, &is_nak))
        return (CMS_REPLY_NONE);

    if (ack_seq != _sia.cnt)
    {
        LOG_WARNING("SIA ACK seq mismatch %d != %d", ack_seq, _sia.cnt);
        return (CMS_REPLY_NONE);
    }

    return (is_nak ? CMS_REPLY_NAK : CMS_REPLY_ACK);
}

const cms_proto_t cms_proto_sia_ip = {
    .reinit       = _sia_proto_reinit,
    .new_event    = _sia_proto_new_event,
    .packet_ready = _sia_proto_packet_ready,
    .packet_size  = _sia_proto_packet_size,
    .get_packet   = _sia_proto_get_packet,
    .packet_done  = _sia_proto_packet_done,
    .packet_reply = _sia_proto_packet_reply,
};

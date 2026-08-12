#include "test.h"
#include "cms_sia_ip.h"
#include "rtc.h"
#include <string.h>

// SIA-DC-09 codec tests.
//
// Frame: <LF><crc><0LLL>"SIA-DCS"<seq>L<prefix>#<acct>[<data>]<CR>
//   <crc>  4 hex chars, CRC-16-IBM of the data region
//   <0LLL> 4 hex chars, '0' + 3-char hex length of the data region

// ---- mocks ----

void rtc_get_time(rtc_t *time)
{
    // fixed current time for deterministic timestamps
    time->second = 56;
    time->minute = 34;
    time->hour   = 12;
    time->day    = 12;
    time->month  = 8;
    time->year   = 26;
}

// Reference CRC-16-IBM (poly 0xA001, init 0) — bitwise implementation,
// equivalent to the table-driven version inside the codec.
static u16 _test_crc(u8 *data)
{
    u16 crc = 0;
    u8 ch;

    while ((ch = *data++) != '\0' && ch != 0x0D)
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
    return (crc);
}

// Build a raw SIA frame from a data string (computes CRC + length header)
static u16 _test_frame(u8 *out, const ascii *data)
{
    u16 len = strlen(data);
    u16 crc = _test_crc((u8 *)data);

    out[0] = 0x0A;
    sprintf((char *)out + SIA_IDX_CRC, "%04X", crc);
    sprintf((char *)out + SIA_IDX_LEN, "0%03X", len);
    memcpy(out + SIA_IDX_DATA, data, len);
    out[SIA_IDX_DATA + len] = 0x0D;
    return (SIA_IDX_DATA + len + 1);
}

static void _event_set(event_t *e, event_id_e id, event_source_e source)
{
    memset(e, 0, sizeof(*e));
    e->cnt = 1;
    e->id = id;
    e->source = source;
    e->time.hour = 12;
    e->time.minute = 34;
    e->time.second = 56;
}

// ---- message build ----

TEST(sia_build_basic)
{
    cms_sia_ip_state_t s;
    event_t e;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    buf_t b;
    u16 len;

    cms_sia_ip_init(&s);
    s.account = 0x1234;
    s.cnt = 1;
    s.prefix = 0;

    _event_set(&e, EVENT_ID_ALARM, EVENT_SOURCE_SHOCK);

    buf_init(&b, (char *)buf, sizeof(buf));
    len = cms_sia_ip_build_msg(&s, &b, &e, NULL);

    ASSERT(len > 0);
    ASSERT_EQ(buf[0], 0x0A);            // LF
    ASSERT_EQ(buf[len-1], 0x0D);        // CR

    // data region: "SIA-DCS"0001L0#1234[|Nri01/BA4^^][I][H12:34:56]_timestamp
    const ascii *data = (char *)buf + SIA_IDX_DATA;
    ASSERT(strncmp(data, "\"SIA-DCS\"0001L0#1234[|Nri01/BA4^^][I][H12:34:56]_12:34:56,08-12-2026", 60) == 0);
}

TEST(sia_build_valid_frame)
{
    cms_sia_ip_state_t s;
    event_t e;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    buf_t b;
    u16 len;

    cms_sia_ip_init(&s);
    s.account = 0x1234;

    _event_set(&e, EVENT_ID_POWER_FAIL, EVENT_SOURCE_UNIT);

    buf_init(&b, (char *)buf, sizeof(buf));
    len = cms_sia_ip_build_msg(&s, &b, &e, NULL);

    ASSERT(len > 0);
    // the built frame must pass its own validation (CRC + length)
    ASSERT(cms_sia_ip_rx(buf, len));
}

TEST(sia_build_crc_and_len)
{
    cms_sia_ip_state_t s;
    event_t e;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    buf_t b;
    u16 len;
    u16 crc, msg_len;

    cms_sia_ip_init(&s);
    s.account = 0x1234;

    _event_set(&e, EVENT_ID_ALARM, EVENT_SOURCE_KEY);

    buf_init(&b, (char *)buf, sizeof(buf));
    len = cms_sia_ip_build_msg(&s, &b, &e, NULL);

    ASSERT(len > 0);

    // verify the CRC and length header fields independently
    sscanf((char *)buf + SIA_IDX_CRC, "%04hX", &crc);
    sscanf((char *)buf + SIA_IDX_LEN, "%04hX", &msg_len);

    ASSERT_EQ(crc, _test_crc(buf + SIA_IDX_DATA));
    ASSERT_EQ(msg_len, len - SIA_IDX_DATA - 1);   // data + CR excluded
}

TEST(sia_build_event_codes)
{
    typedef struct {
        event_id_e id;
        const ascii *code;
    } code_t;

    static const code_t CODES[] = {
        {EVENT_ID_POWER_FAIL,       "/AT"},
        {EVENT_ID_POWER_RECOVERY,   "/AR"},
        {EVENT_ID_LO_BATT,          "/YT"},
        {EVENT_ID_LO_BATT_RECOVERY, "/YR"},
        {EVENT_ID_SET,              "/CL"},
        {EVENT_ID_UNSET,            "/OP"},
        {EVENT_ID_ALARM,            "/BA"},
        {EVENT_ID_ALARM_CANCEL,     "/BC"},
        {EVENT_ID_FAULT,            "/UT"},
        {EVENT_ID_FAULT_RECOVERY,   "/UR"},
        {EVENT_ID_JAMMING_ACT,      "/XQ"},
        {EVENT_ID_JAMMING_DACT,     "/XH"},
    };

    u8 i;

    for (i = 0; i < sizeof(CODES)/sizeof(CODES[0]); i++)
    {
        cms_sia_ip_state_t s;
        event_t e;
        u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
        buf_t b;
        u16 len;

        cms_sia_ip_init(&s);
        s.account = 0x1;

        _event_set(&e, CODES[i].id, EVENT_SOURCE_UNIT);

        buf_init(&b, (char *)buf, sizeof(buf));
        len = cms_sia_ip_build_msg(&s, &b, &e, NULL);

        ASSERT(len > 0);
        ASSERT(strstr((char *)buf, CODES[i].code) != NULL);
    }
}

TEST(sia_build_zone_digits)
{
    typedef struct {
        event_source_e source;
        ascii zone_char;
    } zone_t;

    static const zone_t ZONES[] = {
        {EVENT_SOURCE_UNIT,     '0'},
        {EVENT_SOURCE_KEY,      '1'},
        {EVENT_SOURCE_DOOR,     '2'},
        {EVENT_SOURCE_INPUT1,   '3'},
        {EVENT_SOURCE_SHOCK,    '4'},
        {EVENT_SOURCE_LOCK,     '5'},
        {EVENT_SOURCE_BATTERY,  '7'},
        {EVENT_SOURCE_USER4,    'C'},
    };

    u8 i;

    for (i = 0; i < sizeof(ZONES)/sizeof(ZONES[0]); i++)
    {
        cms_sia_ip_state_t s;
        event_t e;
        u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
        buf_t b;
        ascii pattern[8];

        cms_sia_ip_init(&s);
        s.account = 0x1;

        _event_set(&e, EVENT_ID_ALARM, ZONES[i].source);

        buf_init(&b, (char *)buf, sizeof(buf));
        cms_sia_ip_build_msg(&s, &b, &e, NULL);

        // "/BA<zone>^"
        sprintf(pattern, "/BA%c^", ZONES[i].zone_char);
        ASSERT(strstr((char *)buf, pattern) != NULL);
    }
}

TEST(sia_build_no_code_event)
{
    // EVENT_ID_ALARM_TIMEOUT has no SIA code — build must fail
    cms_sia_ip_state_t s;
    event_t e;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    buf_t b;

    cms_sia_ip_init(&s);
    s.account = 0x1;

    _event_set(&e, EVENT_ID_ALARM_TIMEOUT, EVENT_SOURCE_UNIT);

    buf_init(&b, (char *)buf, sizeof(buf));
    ASSERT_EQ(cms_sia_ip_build_msg(&s, &b, &e, NULL), 0);
}

// ---- RX validation ----

TEST(sia_rx_valid)
{
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;

    len = _test_frame(buf, "\"ACK\"0001L0#1234[]");
    ASSERT(cms_sia_ip_rx(buf, len));
}

TEST(sia_rx_no_lf)
{
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;

    len = _test_frame(buf, "\"ACK\"0001L0#1234[]");
    buf[0] = 'X';
    ASSERT(!cms_sia_ip_rx(buf, len));
}

TEST(sia_rx_bad_crc)
{
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;

    len = _test_frame(buf, "\"ACK\"0001L0#1234[]");
    buf[1] ^= 0xFF;   // corrupt the CRC field
    ASSERT(!cms_sia_ip_rx(buf, len));
}

TEST(sia_rx_bad_len)
{
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;

    len = _test_frame(buf, "\"ACK\"0001L0#1234[]");
    buf[SIA_IDX_LEN] = '1';   // corrupt the length field
    ASSERT(!cms_sia_ip_rx(buf, len));
}

TEST(sia_rx_too_short)
{
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];

    buf[0] = 0x0A;
    ASSERT(!cms_sia_ip_rx(buf, 10));
}

// ---- ACK/NAK parsing ----

TEST(sia_ack_valid)
{
    cms_sia_ip_state_t s;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;
    u16 seq = 0;
    bool is_nak = false;

    cms_sia_ip_init(&s);
    s.account = 0x1234;
    s.prefix = 0;
    s.cnt = 7;

    len = _test_frame(buf, "\"ACK\"0007L0#1234[]");

    ASSERT(cms_sia_ip_rx_ack(&s, buf, len, &seq, &is_nak));
    ASSERT_EQ(seq, 7);
    ASSERT(!is_nak);
}

TEST(sia_nak_valid)
{
    cms_sia_ip_state_t s;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;
    u16 seq = 0;
    bool is_nak = false;

    cms_sia_ip_init(&s);
    s.account = 0x1234;
    s.prefix = 0;
    s.cnt = 7;

    len = _test_frame(buf, "\"NAK\"0007L0");

    ASSERT(cms_sia_ip_rx_ack(&s, buf, len, &seq, &is_nak));
    ASSERT_EQ(seq, 7);
    ASSERT(is_nak);
}

TEST(sia_ack_wrong_account)
{
    cms_sia_ip_state_t s;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;
    u16 seq = 0;
    bool is_nak = false;

    cms_sia_ip_init(&s);
    s.account = 0x1234;

    // ACK for a different account
    len = _test_frame(buf, "\"ACK\"0001L0#9999[]");

    ASSERT(!cms_sia_ip_rx_ack(&s, buf, len, &seq, &is_nak));
}

TEST(sia_ack_wrong_prefix)
{
    cms_sia_ip_state_t s;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;
    u16 seq = 0;
    bool is_nak = false;

    cms_sia_ip_init(&s);
    s.account = 0x1234;
    s.prefix = 3;

    len = _test_frame(buf, "\"ACK\"0001L0#1234[]");

    ASSERT(!cms_sia_ip_rx_ack(&s, buf, len, &seq, &is_nak));
}

TEST(sia_ack_bad_crc)
{
    cms_sia_ip_state_t s;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;
    u16 seq = 0;
    bool is_nak = false;

    cms_sia_ip_init(&s);
    s.account = 0x1234;

    len = _test_frame(buf, "\"ACK\"0001L0#1234[]");
    buf[SIA_IDX_CRC] ^= 0x01;

    ASSERT(!cms_sia_ip_rx_ack(&s, buf, len, &seq, &is_nak));
}

TEST(sia_not_ack_frame)
{
    cms_sia_ip_state_t s;
    u8 buf[CMS_SIA_IP_MAX_PACKET_LEN];
    u16 len;
    u16 seq = 0;
    bool is_nak = false;

    cms_sia_ip_init(&s);
    s.account = 0x1234;

    // a regular SIA data message is not an ACK
    len = _test_frame(buf, "\"SIA-DCS\"0001L0#1234[|Nri01/BA4^^]");

    ASSERT(!cms_sia_ip_rx_ack(&s, buf, len, &seq, &is_nak));
}

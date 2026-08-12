#include "test.h"
#include "modem.h"
#include "modem_at.h"
#include <string.h>

// URC (Unsolicited Result Code) state-machine tests.
// modem_parse_urc() reads m->at.rx_buf and updates modem state flags,
// counters and fires application events.

static modem_event_e _event;
static int _event_cnt;

static void _event_mock(modem_event_e event)
{
    _event = event;
    _event_cnt++;
}

// Prepare a fresh modem instance with one URC line in rx_buf
static void _urc_init(modem_t *m, const char *urc)
{
    memset(m, 0, sizeof(*m));
    m->pfunc_event = _event_mock;
    strcpy(m->at.rx_buf, urc);
    _event = MODEM_EVENT_SIZE;
    _event_cnt = 0;
}

#define ASSERT_EVENT(e) do {           \
    ASSERT_EQ(_event_cnt, 1);          \
    ASSERT_EQ(_event, (e));            \
} while (0)

#define ASSERT_NO_EVENT() ASSERT_EQ(_event_cnt, 0)

// ---- +CPIN ----

TEST(urc_cpin_ready)
{
    modem_t m;

    _urc_init(&m, "+CPIN: READY");
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_PIN_READY);
}

TEST(urc_cpin_sim_pin)
{
    modem_t m;

    _urc_init(&m, "+CPIN: SIM PIN");
    m.flags |= MODEM_FLAG_PIN_READY;   // was ready before

    ASSERT(modem_parse_urc(&m));
    ASSERT(!(m.flags & MODEM_FLAG_PIN_READY));
}

// ---- +CREG (GSM network registration) ----

TEST(urc_creg_home_short)
{
    modem_t m;

    _urc_init(&m, "+CREG: 1");         // one-char form
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_NET_READY);
    ASSERT(!(m.flags & MODEM_FLAG_ROAMING));
}

TEST(urc_creg_home_long)
{
    modem_t m;

    _urc_init(&m, "+CREG: 0,1");       // two-param form
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_NET_READY);
    ASSERT(!(m.flags & MODEM_FLAG_ROAMING));
}

TEST(urc_creg_roaming)
{
    modem_t m;

    _urc_init(&m, "+CREG: 0,5");
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_NET_READY);
    ASSERT(m.flags & MODEM_FLAG_ROAMING);
}

TEST(urc_creg_searching)
{
    modem_t m;

    _urc_init(&m, "+CREG: 0,2");
    m.flags |= MODEM_FLAG_NET_READY | MODEM_FLAG_DATA_READY;
    m.signal_level = 42;
    m.error_counter = 5;

    ASSERT(modem_parse_urc(&m));
    ASSERT(!(m.flags & MODEM_FLAG_NET_READY));
    ASSERT(!(m.flags & MODEM_FLAG_DATA_READY));
    ASSERT_EQ(m.error_counter, 6);     // _set_offline() increments
    ASSERT_EQ(m.signal_level, 0);      // and resets signal level
}

// ---- +CEREG (LTE network registration) ----

TEST(urc_cereg_lte)
{
    modem_t m;

    _urc_init(&m, "+CEREG: 1");
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_LTE);
    ASSERT(m.flags & MODEM_FLAG_DATA_READY);
}

TEST(urc_cereg_roaming)
{
    modem_t m;

    _urc_init(&m, "+CEREG: 5");
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_LTE);
    ASSERT(m.flags & MODEM_FLAG_DATA_READY);
}

TEST(urc_cereg_lost)
{
    modem_t m;

    _urc_init(&m, "+CEREG: 0");
    m.flags |= MODEM_FLAG_LTE | MODEM_FLAG_DATA_READY;

    ASSERT(modem_parse_urc(&m));
    ASSERT(!(m.flags & MODEM_FLAG_LTE));
    ASSERT(!(m.flags & MODEM_FLAG_DATA_READY));
}

// ---- +CGREG (GPRS registration — GSM only) ----

TEST(urc_cgreg_gsm)
{
    modem_t m;

    _urc_init(&m, "+CGREG: 1");
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_DATA_READY);
}

TEST(urc_cgreg_ignored_when_lte)
{
    modem_t m;

    _urc_init(&m, "+CGREG: 1");
    m.flags |= MODEM_FLAG_LTE;         // in LTE, CGREG is not valid

    ASSERT(modem_parse_urc(&m));
    ASSERT(!(m.flags & MODEM_FLAG_DATA_READY));  // untouched by CGREG
}

// ---- +CMTI / +CDSI (incoming SMS and delivery reports) ----

TEST(urc_cmti_sms)
{
    modem_t m;

    _urc_init(&m, "+CMTI: \"SM\",2");
    m.error_counter = 7;

    ASSERT(modem_parse_urc(&m));
    ASSERT_EQ(m.sms_counter, 2);
    ASSERT_EQ(m.error_counter, 0);     // incoming SMS resets error counter
    ASSERT(!m.sms_storage_me);
    ASSERT_EVENT(MODEM_EVENT_SMS_INCOMMING);
}

TEST(urc_cmti_me_storage)
{
    modem_t m;

    _urc_init(&m, "+CMTI: \"ME\",1");

    ASSERT(modem_parse_urc(&m));
    ASSERT(m.sms_storage_me);          // modem stored SMS to wrong memory
    ASSERT_EQ(m.sms_counter, 1);
}

TEST(urc_cdsi_report)
{
    modem_t m;

    _urc_init(&m, "+CDSI: \"SM\",3");

    ASSERT(modem_parse_urc(&m));
    ASSERT_EQ(m.sms_counter, 3);
    ASSERT_NO_EVENT();
}

// ---- +CME / +CMS errors ----

TEST(urc_cme_sim_failure)
{
    modem_t m;

    _urc_init(&m, "+CME ERROR: 13");
    m.flags |= MODEM_FLAG_PIN_READY | MODEM_FLAG_NET_READY;

    ASSERT(modem_parse_urc(&m));
    ASSERT(!(m.flags & MODEM_FLAG_PIN_READY));
    ASSERT(!(m.flags & MODEM_FLAG_NET_READY));
    ASSERT_EQ(m.sim, MODEM_SIM_ST_ERROR);
    ASSERT(m.at.flags & AT_ST_ERROR);
    ASSERT_EQ(m.at.last_error_result, 13);
}

TEST(urc_cms_error)
{
    modem_t m;

    _urc_init(&m, "+CMS ERROR: 331");
    m.error_counter = 4;

    ASSERT(modem_parse_urc(&m));
    ASSERT_EQ(m.error_counter, 6);     // CMS error adds 2
    ASSERT(m.at.flags & AT_ST_ERROR);
    ASSERT_EQ(m.at.last_error_result, 331);
}

// ---- calls ----

TEST(urc_ring)
{
    modem_t m;

    _urc_init(&m, "RING");
    m.error_counter = 3;

    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_CLCC_RQ);
    ASSERT_EQ(m.error_counter, 0);
    ASSERT_EVENT(MODEM_EVENT_CALL_INCOMMING);
}

TEST(urc_clip)
{
    modem_t m;

    _urc_init(&m, "+CLIP: \"+420777123456\",145,\"\",,\"TEL1\",0");

    ASSERT(modem_parse_urc(&m));
    ASSERT_EVENT(MODEM_EVENT_CALL_INCOMMING);
}

TEST(urc_no_carrier)
{
    modem_t m;

    _urc_init(&m, "NO CARRIER");
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_CALL_STOP);
}

TEST(urc_busy)
{
    modem_t m;

    _urc_init(&m, "BUSY");
    ASSERT(modem_parse_urc(&m));
    ASSERT(m.flags & MODEM_FLAG_CALL_STOP);
}

// ---- +CSQ (signal quality) ----

TEST(urc_csq_signal)
{
    modem_t m;

    _urc_init(&m, "+CSQ: 15,0");
    ASSERT(modem_parse_urc(&m));
    ASSERT_EQ(m.signal_level, 50);     // 15/31 → 50%
}

TEST(urc_csq_max)
{
    modem_t m;

    _urc_init(&m, "+CSQ: 31,0");
    ASSERT(modem_parse_urc(&m));
    ASSERT_EQ(m.signal_level, 100);
}

TEST(urc_csq_unknown)
{
    modem_t m;

    _urc_init(&m, "+CSQ: 99,99");      // 99 = not known
    m.signal_level = 42;

    ASSERT(modem_parse_urc(&m));
    ASSERT_EQ(m.signal_level, 42);     // unchanged
}

// ---- misc ----

TEST(urc_unknown_returns_false)
{
    modem_t m;

    _urc_init(&m, "$GPGGA,132321,5043.789,N,01510.609,E,1,06,1.6,553,M");

    ASSERT(!modem_parse_urc(&m));
    ASSERT_NO_EVENT();
    ASSERT_EQ(m.flags, 0);             // no state change
    ASSERT_EQ(m.error_counter, 0);
}

TEST(urc_event_without_callback)
{
    modem_t m;

    _urc_init(&m, "+CMTI: \"SM\",1");
    m.pfunc_event = NULL;              // no callback registered

    ASSERT(modem_parse_urc(&m));       // must not crash
    ASSERT_EQ(m.sms_counter, 1);
}

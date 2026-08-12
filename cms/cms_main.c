#include "common.h"
#include "cms_main.h"
#include "cms_sia_ip.h"
#include "event_buf.h"
#include "cfg.h"
#include "net.h"
#include "log.h"

LOG_DEF("CMS");

#ifndef NO_CMS

#define CMS_BUF_SIZE     32
#define CMS_RETRY_DEFAULT 5
#define CMS_WAIT_TMOUT  (10 * OS_TIMER_SECOND)   // wait for ACK per attempt

typedef struct {
    cms_sia_ip_state_t sia;
    ip_addr_t server_ip;
    u16 server_port;
    os_timer_t wait_tmr;
    event_t pending_event;   // event being sent (for retries)
    u8 retry;
    u8 waiting:1;
} cms_t;

static cms_t _cms;

static event_buf_item_t _cms_items[CMS_BUF_SIZE];
static event_buf_t _cms_buf;
static u8 _packet_buf[CMS_SIA_IP_MAX_PACKET_LEN];

bool cms_init(void)
{
    cms_sia_ip_init(&_cms.sia);
    event_buf_init(&_cms_buf, _cms_items, CMS_BUF_SIZE);
    _cms.waiting = false;
    _cms.retry = 0;

    return (cms_reinit());
}

bool cms_reinit(void)
{
    ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;
    u32 account;

    buf_init(&buf, cfg, sizeof(cfg));

    if (cfg_read(&buf, CFG_ID_SERVER_ADDR, ACCESS_SYSTEM))
        net_get_target_ip(&_cms.server_ip.addr, &_cms.server_port, cfg);

    buf_clear(&buf);
    if (cfg_read(&buf, CFG_ID_CMS_ACCOUNT, ACCESS_SYSTEM))
    {
        if (sscanf(cfg, "%" SCNx32, &account) == 1)
            _cms.sia.account = account;
    }

    LOG_INFO("CMS %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ":%d, acct=%" PRIX32,
             _cms.server_ip.addr & 0xFF,
             (_cms.server_ip.addr >> 8) & 0xFF,
             (_cms.server_ip.addr >> 16) & 0xFF,
             (_cms.server_ip.addr >> 24) & 0xFF,
             _cms.server_port, _cms.sia.account);

    return ((_cms.server_ip.addr != 0) && (_cms.server_port != 0));
}

bool cms_new_event(event_t *event)
{
    if ((_cms.server_ip.addr == 0) || (_cms.server_port == 0))
        return (false);  // not configured

    if (++_cms.sia.cnt > 9999)
        _cms.sia.cnt = 1;

    // store event, send it later from cms_main_process()
    if (! event_buf_add_event(&_cms_buf, event, EVENT_PRIO_STD, EVENT_USER_NONE))
    {
        LOG_ERROR("CMS buffer full");
        return (false);
    }
    return (true);
}

static bool _cms_send_packet(u8 *data, u16 len)
{
    udp_packet_t packet;

    packet.dst_ip.addr = _cms.server_ip.addr;
    packet.dst_port    = _cms.server_port;
    packet.src_port    = _cms.server_port;
    packet.data        = data;
    packet.datalen     = len;

    return (net_udp_tx(&packet));
}

static bool _cms_build_and_send(event_t *event)
{
    buf_t buf;
    u16 len;

    if (! buf_init(&buf, (char *)_packet_buf, sizeof(_packet_buf)))
        return (false);

    len = cms_sia_ip_build_msg(&_cms.sia, &buf, event, NULL);
    if (len == 0)
        return (false);

    LOG_INFO("SEND seq=%d", _cms.sia.cnt);

    if (! _cms_send_packet(_packet_buf, len))
    {
        LOG_ERROR("send failed");
        return (false);
    }
    return (true);
}

static void _cms_fail(void)
{   // retries exhausted, discard the event
    LOG_ERROR("CMS event delivery failed");
    event_buf_done_events(&_cms_buf, false);
    _cms.waiting = false;
    _cms.retry = 0;
}

void cms_udp_rx(u8 *data, u16 len)
{
    u16 ack_seq;
    bool is_nak;

    if (! cms_sia_ip_rx_ack(&_cms.sia, data, len, &ack_seq, &is_nak))
        return;  // not an ACK/NAK for us

    if (! _cms.waiting)
        return;

    if (ack_seq != _cms.sia.cnt)
    {
        LOG_WARNING("ACK seq mismatch %d != %d", ack_seq, _cms.sia.cnt);
        return;
    }

    if (is_nak)
    {
        LOG_ERROR("NAK received");
        _cms_fail();
        return;
    }

    LOG_INFO("ACK seq=%d", ack_seq);
    event_buf_done_events(&_cms_buf, true);
    _cms.waiting = false;
    _cms.retry = 0;
}

void cms_main_process(void)
{
    os_timer_t now = os_timer_get();

    if ((_cms.server_ip.addr == 0) || (_cms.server_port == 0))
        return;  // not configured

    if (_cms.waiting)
    {   // waiting for ACK
        if (now < _cms.wait_tmr)
            return;  // still waiting

        // timeout — retry the same event with the same counter
        if (_cms.retry < CMS_RETRY_DEFAULT)
        {
            _cms.retry++;
            LOG_WARNING("CMS retry %d", _cms.retry);
            _cms_build_and_send(&_cms.pending_event);
            _cms.wait_tmr = now + CMS_WAIT_TMOUT;
        }
        else
        {
            _cms_fail();
        }
        return;
    }

    // idle — send the next event if there is one
    event_t event;
    u16 user;

    if (! event_buf_get_event(&_cms_buf, &event, &user, NULL))
        return;  // nothing to send

    if (! _cms_build_and_send(&event))
    {   // event without SIA code or build error — discard it
        event_buf_done_events(&_cms_buf, false);
        return;
    }

    _cms.pending_event = event;
    _cms.waiting = true;
    _cms.wait_tmr = now + CMS_WAIT_TMOUT;
}

void cms_tick(void)
{
    // reserved for periodic reports (alive checks)
}

#endif // NO_CMS

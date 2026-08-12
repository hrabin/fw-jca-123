#include "common.h"
#include "cms_main.h"
#include "cms_proto.h"
#include "event_buf.h"
#include "cfg.h"
#include "net.h"
#include "log.h"

LOG_DEF("CMS");

#ifndef NO_CMS

#define CMS_BUF_SIZE      32
#define CMS_RETRY_DEFAULT 5
#define CMS_WAIT_TMOUT  (10 * OS_TIMER_SECOND)   // wait for ACK per attempt

#define CMS_PROTO_SIA_IP 0
#define CMS_PROTO_DEFAULT CMS_PROTO_SIA_IP

typedef struct {
    ip_addr_t server_ip;
    u16 server_port;
    u32 account;
    os_timer_t wait_tmr;
    u8 retry;
    u8 waiting:1;
} cms_t;

static cms_t _cms;

static event_buf_item_t _cms_items[CMS_BUF_SIZE];
static event_buf_t _cms_buf;
static u8 _packet_buf[CMS_MAX_PACKET_LEN];
static u16 _packet_len = 0;

// protocol interface — set by _proto_select()
static const cms_proto_t *_proto = NULL;

static void _proto_select(u8 protocol)
{
    switch (protocol)
    {
    case CMS_PROTO_SIA_IP:
    default:
        _proto = &cms_proto_sia_ip;
        break;
    }
}

bool cms_init(void)
{
    event_buf_init(&_cms_buf, _cms_items, CMS_BUF_SIZE);
    _cms.waiting = false;
    _cms.retry = 0;

    _proto_select(CMS_PROTO_DEFAULT);

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
            _cms.account = account;
    }

    _proto->reinit(_cms.account);

    LOG_INFO("CMS %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ":%d, acct=%" PRIX32,
             _cms.server_ip.addr & 0xFF,
             (_cms.server_ip.addr >> 8) & 0xFF,
             (_cms.server_ip.addr >> 16) & 0xFF,
             (_cms.server_ip.addr >> 24) & 0xFF,
             _cms.server_port, _cms.account);

    return ((_cms.server_ip.addr != 0) && (_cms.server_port != 0));
}

bool cms_new_event(event_t *event)
{
    if ((_cms.server_ip.addr == 0) || (_cms.server_port == 0))
        return (false);  // not configured

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

static void _cms_fail(void)
{   // retries exhausted, discard the event
    LOG_ERROR("CMS event delivery failed");
    event_buf_done_events(&_cms_buf, false);
    _proto->packet_done();
    _cms.waiting = false;
    _cms.retry = 0;
}

bool cms_udp_rx(u8 *data, u16 len, u16 port)
{
    cms_reply_t reply;

    if (port != _cms.server_port)
        return (false);  // not from our CMS server

    reply = _proto->packet_reply(data, len);
    if (reply == CMS_REPLY_NONE)
        return (false);  // not a reply for the pending packet

    if (! _cms.waiting)
        return (true);

    if (reply == CMS_REPLY_NAK)
    {
        LOG_ERROR("NAK received");
        _cms_fail();
        return (true);
    }

    LOG_INFO("ACK");
    event_buf_done_events(&_cms_buf, true);
    _proto->packet_done();
    _cms.waiting = false;
    _cms.retry = 0;
    return (true);
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

        // timeout — resend the same packet
        if (_cms.retry < CMS_RETRY_DEFAULT)
        {
            _cms.retry++;
            LOG_WARNING("CMS retry %d", _cms.retry);
            _cms_send_packet(_packet_buf, _packet_len);
            _cms.wait_tmr = now + CMS_WAIT_TMOUT;
        }
        else
        {
            _cms_fail();
        }
        return;
    }

    // idle — prepare the next event
    if (! _proto->packet_ready())
    {
        event_t event;
        u16 user;

        if (! event_buf_get_event(&_cms_buf, &event, &user, NULL))
            return;  // nothing to send

        if (! _proto->new_event(&event))
        {   // event not reportable by this protocol — discard it
            event_buf_done_events(&_cms_buf, false);
            return;
        }
    }

    // send the prepared packet
    _packet_len = _proto->packet_size();
    _proto->get_packet(_packet_buf);
    LOG_INFO("SEND len=%d", _packet_len);

    if (! _cms_send_packet(_packet_buf, _packet_len))
    {
        LOG_ERROR("send failed");
        return;  // packet stays ready, next process() call resends it
    }

    _cms.waiting = true;
    _cms.wait_tmr = now + CMS_WAIT_TMOUT;
}

void cms_tick(void)
{
    // reserved for periodic reports (alive checks)
}

#endif // NO_CMS

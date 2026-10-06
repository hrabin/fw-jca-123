#include "common.h"
#include "net.h"
#include "net_target.h"
#include "udp.h"
#include "cfg.h"
#include "cms_main.h"
#include "modem_main.h"
#include "log.h"

LOG_DEF("NET");

bool net_ready(void)
{
    return (modem_main_data());
}

bool net_connect(void)
{
    if (udp_ready())
        return (true);

    ascii *apn = cfg_read_static(CFG_ID_APN);
    if (apn == NULL)
    {
        return (false);
    }
    return (modem_main_data_connect(apn));
}

// Parse a target string without resolving it — no modem access.  Returns false
// only when the string is not a usable "<host>:<port>".  A DNS name returns
// true with *ip == 0 and *port set.  Safe to call from any task.
bool net_parse_target_ip (u32 *ip, u16 *port, const ascii *url)
{
    char host[NET_TARGET_HOST_MAX];

    *ip = 0;
    return (net_target_parse (url, host, sizeof(host), port, ip) != NET_TARGET_INVALID);
}

// Resolve a configured target to an IPv4 address + port.
//
// The DNS lookup is issued over the modem, so this must only be called from
// task_app_comm, the only task that owns the data connection.  Everything that
// can run elsewhere parses with net_parse_target_ip() and defers the resolve
// to its task_app_comm path (tracer_comm_process() / cms_main_process() /
// update_task() / _reinit_task()).
bool net_get_target_ip (u32 *ip, u16 *port, const ascii *url)
{
    char host[NET_TARGET_HOST_MAX];
    net_target_type_e type;

    *ip = 0;

    type = net_target_parse (url, host, sizeof(host), port, ip);

    if (type == NET_TARGET_IP)
        return (true);

    if (type == NET_TARGET_NAME)
    {   // "host.name:8080" — resolve through the modem
        ip_addr_t addr;

        addr.addr = 0;
        if (modem_main_dns_resolve (host, &addr) && (addr.addr != 0))
        {
            *ip = addr.addr;
            LOG_INFO("DNS %s -> %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32,
                     host, *ip & 0xFF, (*ip >> 8) & 0xFF,
                     (*ip >> 16) & 0xFF, (*ip >> 24) & 0xFF);
            return (true);
        }

        // *port was already set, so the caller can retry later (DNS needs an
        // active data connection, which may not be up yet)
        LOG_ERROR("unresolved DNS \"%s\"", host);
        return (false);
    }

    LOG_ERROR("bad target \"%s\"", url);
    return (false);
}

bool net_udp_tx(udp_packet_t *pkt)
{
    return (udp_tx(pkt));
}

extern bool tracer_packet_rx (u8 *data, u16 len, u16 port);
extern bool update_packet_rx (u8 *data, u16 len, u16 port);

void net_udp_rx (udp_packet_t *pkt)
{
    if (tracer_packet_rx(pkt->data, pkt->datalen,pkt->src_port))
        return;
    if (update_packet_rx(pkt->data, pkt->datalen,pkt->src_port))
        return;
    if (cms_udp_rx(pkt->data, pkt->datalen, pkt->src_port))
        return;
    LOG_DEBUGL(3, "RX UNKNOWN packet, p=%d, typ=0x%02x, len=%d", pkt->src_port, *pkt->data, pkt->datalen);
}

void net_init(void)
{
    udp_register_rx_callback(net_udp_rx);
}


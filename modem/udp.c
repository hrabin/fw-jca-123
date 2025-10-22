#include "common.h"
#include "udp.h"
#include "modem_main.h"
#include "cfg.h"
#include "net.h"
#include "log.h"

LOG_DEF("UDP");

u32 udp_data_tx = 0;
u32 udp_data_rx = 0;

bool udp_ready(void)
{
    return (modem_main_data());
}

bool udp_tx(udp_packet_t *pkt)
{
    LOG_DEBUGL(5, "TX %d", pkt->datalen);
    udp_data_tx += pkt->datalen;
    return modem_main_udp_send(pkt);
}

void udp_rx (u8 *data, u16 len, ip_addr_t *addr, u16 port)
{   // callback from modem
    LOG_DEBUGL(5, "RX %d", len);
    udp_data_rx += len;
    if (port == 53)
    {   // DNS
    }
    else
    {
        udp_packet_t p;
        memset(&p, 0, sizeof(p));
        p.src_ip.addr = addr->addr;
        p.data=data;
        p.src_port=port;
        p.datalen=len;

        net_udp_rx(&p);
    }
}

void udp_info(buf_t *dest)
{
    buf_append_fmt(dest, "RX: %d, TX: %d", udp_data_rx, udp_data_tx);
}

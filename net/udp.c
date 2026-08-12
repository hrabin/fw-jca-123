#include "common.h"
#include "udp.h"
#include "log.h"

LOG_DEF("UDP");

static u32 udp_data_tx = 0;
static u32 udp_data_rx = 0;

static udp_rx_callback_t rx_callback = NULL;

void udp_register_rx_callback(udp_rx_callback_t cb)
{
    rx_callback = cb;
}

void udp_rx(u8 *data, u16 len, ip_addr_t *addr, u16 port)
{   // callback from transport layer (e.g. modem)
    LOG_DEBUGL(5, "RX %d", len);
    udp_data_rx += len;

    if (port == 53)
    {   // DNS — not implemented
        return;
    }

    if (rx_callback != NULL)
    {
        udp_packet_t p;
        memset(&p, 0, sizeof(p));
        p.src_ip.addr = addr->addr;
        p.data = data;
        p.src_port = port;
        p.datalen = len;

        rx_callback(&p);
    }
}

void udp_count_tx(u16 len)
{
    udp_data_tx += len;
}

void udp_info(buf_t *dest)
{
    buf_append_fmt(dest, "RX: %d, TX: %d", udp_data_rx, udp_data_tx);
}

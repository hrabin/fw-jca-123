#include "common.h"
#include "net.h"
#include "cfg.h"
#include "modem_main.h"
#include "log.h"

LOG_DEF("NET");

bool net_ready(void)
{
    return (modem_main_data());
}

bool net_connect(void)
{
    ascii apn[CFG_ITEM_SIZE];
    buf_t buf;

	if (udp_ready())
		return (true);

    buf_init(&buf, apn, sizeof(apn));
    if (! cfg_read(&buf, CFG_ID_APN, ACCESS_SYSTEM))
	{
		return (false);
	}
	return (modem_main_data_connect(apn));
}

bool _ip_parse (u32 *ip, u16 *port, const ascii * src)
{
	int a,b,c,d,p;

	if (sscanf(src, "%d.%d.%d.%d:%d", &a, &b, &c ,&d, &p)==5)
	{
		if ((a>255) || (b>255) || (c>255) || (d>255))
			return (false);
		if ((a==0) || (d==0))
			return (false);
		*ip = a + (b<<8) + (c<<16) + (d<<24);
		*port = p;
		return (true);
	}
	return (false);
}

bool net_get_target_ip (u32 *ip, u16 *port, const ascii *url)
{
    *ip = 0;
	// Check if there is IP in format "177.78.98.199:8080"
	if (_ip_parse (ip, port, url))
		return (true);

	// it is not IP, it will need use DNS
	// expected format "www.gmmultimedia.cz:8080"
	LOG_ERROR("unsupported DNS resolv");
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
    LOG_DEBUGL(3, "RX UNKNOWN packet, p=%d, typ=0x%02x, len=%d", pkt->src_port, *pkt->data, pkt->datalen);
}


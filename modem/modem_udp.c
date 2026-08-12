#include "common.h"
#include "udp.h"
#include "modem_main.h"
#include "log.h"

LOG_DEF("UDP");

bool udp_ready(void)
{
    return (modem_main_data());
}

bool udp_tx(udp_packet_t *pkt)
{
    LOG_DEBUGL(5, "TX %d", pkt->datalen);
    udp_count_tx(pkt->datalen);
    return modem_main_udp_send(pkt);
}

#ifndef MODEM_BG95_H
#define MODEM_BG95_H

#include "modem.h"
#include "udp.h"

#define MODEM_BG95_PWR_SWITCH_TIME (700) // [ms]
// extern modem_cmd_set_t MODEM_BG95_CMD_SET;

bool modem_bg95_init(modem_t *m);
bool modem_bg95_check(modem_t *m);
bool modem_bg95_urc(modem_t *m);

bool modem_bg95_udp_init(modem_t *m);
bool modem_bg95_udp_send(modem_t *m, udp_packet_t *packet);
void modem_bg95_udp_rx_task(modem_t *m);

bool modem_bg95_dns_resolve(modem_t *m, const ascii *host, ip_addr_t *ip);
// Parse a "+QIURC: \"dnsgip\",..." URC line (exposed for host tests)
bool modem_bg95_dnsgip_parse(const ascii *line, bool *done, bool *ok, u32 *ip);

#endif // ! MODEM_BG95_H



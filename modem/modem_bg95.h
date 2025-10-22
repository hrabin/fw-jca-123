#ifndef MODEM_BG95_H
#define	MODEM_BG95_H

#include "modem.h"
#include "udp.h"

#define	MODEM_BG95_PWR_SWITCH_TIME (700) // [ms]
// extern modem_cmd_set_t MODEM_BG95_CMD_SET;

bool modem_bg95_init(modem_t *m);
bool modem_bg95_check(modem_t *m);
bool modem_bg95_urc(modem_t *m);

bool modem_bg95_udp_init(modem_t *m);
bool modem_bg95_udp_send(modem_t *m, udp_packet_t *packet);
void modem_bg95_udp_rx_task(modem_t *m);

#endif // ! MODEM_BG95_H



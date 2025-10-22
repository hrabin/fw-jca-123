#ifndef _MODEM_CFG_H
#define _MODEM_CFG_H

#include "modem.h"

const char    *modem_cfg_get_apn(void);
const uint8_t *modem_cfg_get_server_ip(void);
const uint16_t modem_cfg_get_server_port(void);
const char    *modem_cfg_get_sim_pin(void);

#endif // ! _MODEM_CFG_H


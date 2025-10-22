#include "modem_cfg.h"

__attribute__((weak))  const char *cfg_get_apn(void)
{
    return ("");
}

__attribute__((weak))  const uint8_t *cfg_get_server_ip(void)
{
    return (NULL);
}

__attribute__((weak))  const uint16_t cfg_get_server_port(void)
{
    return (0);
}

__attribute__((weak))  const char *cfg_get_sim_pin(void)
{
    return ("1234");
}

__attribute__((weak))  void cfg_update_modem_type(uint8_t type)
{
}

const char *modem_cfg_get_apn(void)
{
    return (cfg_get_apn());
}

const uint8_t *modem_cfg_get_server_ip(void)
{
    return (cfg_get_server_ip());
}

const uint16_t modem_cfg_get_server_port(void)
{
    return (cfg_get_server_port());
}

const char *modem_cfg_get_sim_pin(void)
{
    return (cfg_get_sim_pin());
}

void modem_cfg_update_type(modem_type_e modem_type)
{
    cfg_update_modem_type(modem_type);
}

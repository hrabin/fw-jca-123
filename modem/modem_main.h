#ifndef MODEM_MAIN_H
#define MODEM_MAIN_H

#include "type.h"
#include "modem.h"
#include "sms.h"
#include "udp.h"
#include "rtc.h"

typedef enum {
    MODEM_MAIN_STATE_OFF = 0,
    MODEM_MAIN_STATE_ON,
    MODEM_MAIN_STATE_ERROR,
    MODEM_MAIN_STATE_INIT,
    MODEM_MAIN_STATE_NET_OK,
} modem_main_state_e;

typedef enum {
	MODEM_COMMAND_NONE = 0,
	MODEM_COMMAND_ON,
	MODEM_COMMAND_START,
	MODEM_COMMAND_DATA,
	MODEM_COMMAND_OFF,
	MODEM_COMMAND_RESET,
	MODEM_COMMAND_HB,

} modem_command_e;

bool modem_main_init(void);

bool modem_main_command(modem_command_e command);
void modem_main_rq_state(modem_main_state_e state);
void modem_main_delivery_report (u8 sms_id, u8 result);

modem_main_state_e modem_main_state(void);
bool modem_main_lte(void);
bool modem_main_sms_incomming(void);
bool modem_main_sms_read (sms_struct_t *sms);
bool modem_main_sms_send (sms_struct_t *sms);
bool modem_main_at_cmd(buf_t *response, const char *text);

bool modem_main_data(void);
bool modem_main_data_connect(const ascii *apn);
void modem_main_data_disconnect(void);
bool modem_main_udp_send(udp_packet_t *packet);
void modem_main_jamming(bool state);
bool modem_main_roaming(void);
bool modem_main_get_time(rtc_t *t);

bool modem_echo(void);
void modem_echo_set(bool state);
void modem_main_task(void);
void modem_main_data_rx_process(void);
void modem_main_uart_rx_task(void);


#endif // ! MODEM_MAIN_H

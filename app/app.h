#ifndef APP_H
#define	APP_H

#include "type.h"
#include "cfg.h"

#define APP_LED_IDLE    	 0x0000,10
#define APP_LED_SET     	 0x0001,10
#define APP_LED_SET_PART	 0x0005,10
#define APP_LED_ALARM        0x3333,16
#define APP_LED_ALARM_MEMORY 0x1111,16
#define APP_LED_MODEM_INIT   0x00FF,16
#define APP_LED_MODEM_ERROR  0x0F0F,16
#define APP_LED_TRACKING     0xFFFF,16

// single instant signalization
#define APP_LED_SMS_INCOMMING   0xFFFF,15
#define APP_LED_SMS_SENDING     0xFFFF,15
#define APP_LED_SMS_ERROR       0xFFFF,15
#define APP_LED_CALL_INCOMMING  0x55555555,20
#define APP_LED_SHOCK 		    0x000F,15

void app_init(void);
void app_main_reinit(void);
void app_main_led(u32 mask, u8 length);
void app_main_led_single(u32 mask, u8 length);
void app_main_siren_beep(u32 mask, u8 length);
void app_reinit_req(cfg_id_t id);

void app_main_init(void);
void app_task_fast(void);
void app_task_slow(void);
void app_task_comm(void);
void app_task_modem(void);
void app_task_timer(void);

#endif // ! APP_H


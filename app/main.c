/*
 * Author: jarda https://github.com/hrabin
 */

#include "common.h"
#include "main.h"

#include "app.h"
#include "app_info.h"
#include "buf.h"
#include "cmd.h"
#include "flash_lib.h"
#include "gpreg.h"
#include "hardware.h"
#include "hw_info.h"
#include "led.h"
#include "reset.h"
#include "tty.h"
#include "wdog.h"
#include "sys.h"
#include "log.h"

LOG_DEF("MAIN");

#if (MAIN_DEBUG == 1)
  #warning "MAIN_DEBUG directive active"
#endif // MAIN_DEBUG

#define TASK_TIMER_TMOUT                  500 // [ms]
#define TASK_APP_FAST_TMOUT               500 // 
#define TASK_APP_SLOW_TMOUT           30*1000 // 
#define TASK_APP_COMM_TMOUT         5*60*1000 // 
#define TASK_APP_MODEM_TMOUT             1000 // 

static u8 reset_type = RESET_POWER_ON;

OS_TASK_DEF_STATIC(task_timer,      512, OS_TASK_PRIO_HI);
OS_TASK_DEF_STATIC(task_app_fast,  2048, OS_TASK_PRIO_NORM);
OS_TASK_DEF_STATIC(task_app_slow,  2048, OS_TASK_PRIO_NORM);
OS_TASK_DEF_STATIC(task_app_comm,  2048, OS_TASK_PRIO_NORM);
OS_TASK_DEF_STATIC(task_app_modem, 2048, OS_TASK_PRIO_NORM);

static bool tasks_run = false;

static access_t _uart_access = {
    .auth = ACCESS_SYSTEM,  // temporary acces from start
    .source = EVENT_SOURCE_ADMIN
};

os_mutex_t spi1_mutex; // shared SPI mutex (FLASH / SHOCK)

OS_TASK(task_timer)
{
#define TASK_TIMER_PERIOD 100 // [ms]
    OS_WAIT_TO_DEFINE;

    wdog_task_id_t wdid = wdog_task_register("timer", TASK_TIMER_TMOUT*MS, task_timer_stack);
   
    OS_WAIT_TO_SET(TASK_TIMER_PERIOD);
    while (! tasks_run)
    {
        wdog_task_feed(wdid);
        wdog_main_task();
        
        OS_WAIT_TO(TASK_TIMER_PERIOD);
    }
    while (1)
    {
        wdog_task_feed(wdid);
        wdog_main_task();
        app_task_timer();

        OS_WAIT_TO(TASK_TIMER_PERIOD);
    }
    OS_TASK_EXIT();
}

OS_TASK(task_app_fast)
{
    wdog_task_id_t wdid = wdog_task_register("fast", TASK_APP_FAST_TMOUT*MS, task_app_fast_stack);

    cmd_init();
    app_main_init();

    tasks_run = true;

    while (1)
    {
        wdog_task_feed(wdid);
        app_task_fast();

        OS_DELAY(10);

    }
    OS_TASK_EXIT();
}

OS_TASK(task_app_slow)
{
    wdog_task_id_t wdid = wdog_task_register("slow", TASK_APP_SLOW_TMOUT*MS, task_app_slow_stack);

    while (! tasks_run)
    {
        wdog_task_feed(wdid);
        OS_DELAY(10);
    }

    while (1)
    {
        wdog_task_feed(wdid);
        tty_rx_task();
        app_task_slow();

        OS_DELAY(10);

    }
    OS_TASK_EXIT();
}

OS_TASK(task_app_comm)
{
#define HB_TIME (60*OS_TIMER_SECOND)

    os_timer_t hb;
    wdog_task_id_t wdid = wdog_task_register("comm", TASK_APP_COMM_TMOUT*MS, task_app_comm_stack);

    while (! tasks_run)
    {
        wdog_task_feed(wdid);
        OS_DELAY(10);
    }

    hb = os_timer_get() + HB_TIME;
    
    LOG_DEBUG("comm start");

    GPREG_WRITE(GPREG_BOOTLOOP, 0);

    while (1)
    {
        os_timer_t now = os_timer_get();

        wdog_task_feed(wdid);
        app_task_comm();

        if (now > hb)
        {
            hb += HB_TIME;
            LOG_DEBUG("HB");
        }
        OS_DELAY(100);

    }
    OS_TASK_EXIT();
}

OS_TASK(task_app_modem)
{
    wdog_task_id_t wdid = wdog_task_register("modem", TASK_APP_MODEM_TMOUT*MS, task_app_modem_stack);
    
    while (! tasks_run)
    {
        wdog_task_feed(wdid);
        OS_DELAY(10);
    }
    while (1)
    {
        wdog_task_feed(wdid);
        app_task_modem();
        OS_DELAY(10);
    }
    OS_TASK_EXIT();
}

void wd_feed (void);


void main_cmd_process(char *data)
{
    buf_t *result = cmd_get_buf();

    if (os_timer_get() > (_uart_access.time + ACCESS_UART_TIMEOUT))
        _uart_access.auth = ACCESS_NONE;

    _uart_access.time = os_timer_get();

    if (cmd_process_text(result, data, &_uart_access))
    {
        OS_FLUSH();
        log_lock();
        tty_put_text(NL);
        tty_put_text(buf_data(result));
        tty_put_text(NL);
        tty_put_text(TEXT_DONE);
        OS_FLUSH();
        log_unlock();
    }
    else
    {
        tty_put_text(TEXT_ERROR);
    }
    cmd_return_buf(result);
}

void adc_test(void);

void enter_sleep_mode(void) 
{
    // wdog_disable();
    HW_LED_OFF;

    // Enable access to backup domain
    PWR->CR1 |= PWR_CR1_DBP;

    // Clear Standby and Wakeup flags
    PWR->SCR |= PWR_SCR_CSBF | PWR_SCR_CWUF;

    // Set Standby mode
    PWR->CR1 |= PWR_CR1_LPMS; // PWR_CR1_PDDS;

    // Set the SLEEPDEEP bit
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    
    // Clear the SLEEPDEEP bit to select Sleep mode (not Deep Sleep)
    // SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;

    // Enter Standby mode
    __WFI();
}

static void _tty_rx_parser(char *data)
{
    if (log_debug_level > 2) // set level >2 using DL command
    {
        if (data[1] == '\0')
        {   // DEBUG commands (single character only)
        #warning "remove this debug command set"
            OS_PRINTF("DBG %c:", data[0]);
            switch (data[0])
            {
            case 'B':
                OS_PRINTF("go to BL" NL);
                OS_FLUSH();
                wdog_reset(GPREG_BOOT_STAY_IN_BOOT);
                break;

            case 'd':
                adc_test();
                break;

            case 'F': 
                main_flash();
                break;

            case 't': 
                u32 t =  os_timer_get();
                OS_PRINTF("Tick %ld, tm %ld", xTaskGetTickCount(), t);
                break;

            case 'R':
                flash_flush_cache();
                OS_PRINTF("RESET");
                wdog_reset(2);
                break;

            case 'S':
                OS_PRINTF("DEEP-SLEEP");
                OS_DELAY(10);
                enter_sleep_mode();
                break;

            case 'T': 
                OS_PRINTF("TEST OK");
                break;

            case 'W':
                OS_PRINTF("WDR TEST" NL);
                while (1)
                    OS_DELAY(100);
            }
            OS_PRINTF(NL);
            return;
        }
    }

    app_main_led_single(0xFFFF, 10);
    main_cmd_process(data);
}

void main_flash(void)
{
    LOG_INFO("upgrade now !");
    OS_DELAY(100);
    flash_flush_cache();
    wdog_reset(GPREG_BOOT_FLASH_RQ);
}

int main (void) 
{
    sys_init();
    sys_clock_config();
    sys_run();

    wdog_init();
    wdog_run();

    // log_init();
    gpio_init();

    HW_LED_INIT;
    HW_LED_ON;
    
    log_init(); 
    tty_init(_tty_rx_parser);

    OS_PRINTF(NL);
    OS_PRINTF("APP START" NL);
    HW_LED_OFF;

    OS_PRINTF("# PLATFORM: " OS_PLATFORM_NAME NL);
    OS_PRINTF("# CRC: 0x%" PRIx32 NL, APP_INFO.crc);
    OS_PRINTF("# APP BUILD DATE: " __DATE__ NL);

/*    OS_PRINTF("# DEVICE ID: %d" NL, HW_INFO.device_id);
    OS_PRINTF("# HW: %s" NL, HW_INFO.hw_name);*/
    OS_PRINTF("# SN: 0x%" PRIx32 NL, HW_INFO.addr);


    OS_PRINTF("# RESET TYPE: ");
    switch (reset_type)
    {
    case RESET_POWER_ON: OS_PRINTF("POWER_ON"); break;
    case RESET_USER_RQ:  OS_PRINTF("USER_RQ");  break;
    case RESET_WDT:      OS_PRINTF("WDT");      break;
    case RESET_BOD:      OS_PRINTF("BOD");      break;
    default:             OS_PRINTF("UNKNOWN");  break;
    }
    OS_PRINTF(NL);
    OS_PRINTF(NL);

    app_init();
    
    OS_INIT();
    OS_TASK_CREATE_STATIC(task_timer);
    OS_TASK_CREATE_STATIC(task_app_fast);
    OS_TASK_CREATE_STATIC(task_app_slow);
    OS_TASK_CREATE_STATIC(task_app_comm);
    OS_TASK_CREATE_STATIC(task_app_modem);
   
    OS_MUTEX_INIT(spi1_mutex);
    OS_START();

    OS_ERROR("OS DIED !");
    
    HW_LED_ON;

    while (1)
        ;
}


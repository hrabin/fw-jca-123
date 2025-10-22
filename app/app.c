#include "app.h"
#include "alarm.h"
#include "analog.h"
#include "ble.h"
#include "can.h"
#include "cfg.h"
#include "cmd.h"
#include "event.h"
#include "gps.h"
#include "hardware.h"
#include "io.h"
#include "led.h"
#include "log.h"
#include "modem_main.h"
#include "power.h"
#include "rtc.h"
#include "shock.h"
#include "sms_processing.h"
#include "storage.h"
#include "system.h"
#include "text.h"
#include "tracer.h"
#include "update.h"

LOG_DEF("APP");

static u32 _reinit_mask = 0;
static led_t led;
static led_t siren; // we use LED machine for siren signalization (beep only)

static u32 _inp_state;

static void _led_on(void) { HW_LED_ON; }
static void _led_off(void) { HW_LED_OFF; }

static u32 _siren_beep_time = 0;
#define _SIREN_BEEP_TIME_MIN  0 // [ms]
#define _SIREN_BEEP_TIME_DEF  5 // [ms]
#define _SIREN_BEEP_TIME_MAX 50 // [ms]

static access_t _ble_access = {ACCESS_NONE};

static void _siren_beep(void) 
{   // signalization beep
    HW_SIREN_ON; 
    OS_DELAY(_siren_beep_time);
    HW_SIREN_OFF;
}

static void _led_update(void)
{

    switch (alarm_state())
    {
    case ALARM_ACTIVE:  app_main_led(APP_LED_ALARM); return;
    case ALARM_TIMEOUT: app_main_led(APP_LED_ALARM_MEMORY); return;
    default:
        break;
    }

    switch (modem_main_state())
    {
    // case MODEM_MAIN_STATE_OFF;
    case MODEM_MAIN_STATE_INIT:
        app_main_led(APP_LED_MODEM_INIT); return;
    case MODEM_MAIN_STATE_ERROR:
        app_main_led(APP_LED_MODEM_ERROR); return;
    default:
        break;
    }

    if (tracer_is_active())
    {
        app_main_led(APP_LED_TRACKING);
        return;
    }

    switch (system_state())
    {
    case SECTION_ST_UNSET:    app_main_led(APP_LED_IDLE); return;
    case SECTION_ST_SET_PART: app_main_led(APP_LED_SET_PART); return;
    case SECTION_ST_SET:      app_main_led(APP_LED_SET); return;
    default:
        break;
    }
}

static inline void _io_input_task(void)
{
    int i;
    u32 inp = io_get_inp();

    if (_inp_state == inp)
        return; // no change

    for (i=0; i<IO_INPUT_SIZE; i++)
    {
        if ((_inp_state ^ inp) & (1 << i))
        {
            if (inp & (1 << i))
            {
                LOG_INFO("%s ON", io_inp_name(i));
                system_inp_activated(i);
            }
            else
            {
                LOG_INFO("%s OFF", io_inp_name(i));
                system_inp_deactivated(i);
            }
        }
    }

    _inp_state = inp;
}

static inline void _io_output_task(void)
{   // 
}

void app_init(void)
{   // multitasking does not run during this init
    led_init(&led);
    led.on = _led_on;
    led.off = _led_off;

    app_main_led(0x0F, 8);
    
    led_init(&siren);
    siren.on = _siren_beep;
    siren.off = NULL;
}

void app_main_reinit(void)
{
#define CFG_FORMAT "%d,%d,%d"
    int a,b,c;
 
    ascii cfg[CFG_ITEM_SIZE];
    buf_t buf;

    buf_init(&buf, cfg, sizeof(cfg));

    if (! cfg_read(&buf, CFG_ID_MAIN_SETUP, ACCESS_SYSTEM))
    {
        return;
    }
    _siren_beep_time = _SIREN_BEEP_TIME_DEF;
    a=0; b=0; c=0;
    switch (sscanf(cfg, CFG_FORMAT, &a, &b, &c))
    {
    case 3:
    case 2:
    case 1:
        if ((a >= _SIREN_BEEP_TIME_MIN) && (a <= _SIREN_BEEP_TIME_MAX))
            _siren_beep_time = a;
        break;
    default:
        return;
    }
}

void app_main_led(u32 mask, u8 length)
{
    led_cyclic_sequence(&led, mask, length, 1);
}

void app_main_led_single(u32 mask, u8 length)
{
    led_instant_sequence(&led, mask, length);
}

void app_main_siren_beep(u32 mask, u8 length)
{
    led_instant_sequence(&siren, mask, length);
}

#define _REINIT_MAIN   (1 << 0)
#define _REINIT_TRACER (1 << 1)

static void _reinit_task(void)
{
    if (_reinit_mask == 0)
        return;

    u32 mask = _reinit_mask;

    if (mask & _REINIT_MAIN)
        app_main_reinit();

    if (mask & _REINIT_TRACER)
        tracer_reinit();

    _reinit_mask &= ~mask;
}

static void _time_sync_utc(rtc_t *time)
{
    rtc_add_hours(time, cfg_read_nparam(CFG_ID_TIME_ZONE, 0));
    if (cfg_read_nparam(CFG_ID_TIME_ZONE, 1) == 1)
    {
        if (rtc_dst_active(time))
            rtc_add_hours(time, 1);
    }
    rtc_set_time(time);
}

static void _time_sync_task(void)
{
#define _TIME_SYNC_PERIOD (30 * OS_TIMER_MINUTE)

    static os_timer_t sync_time = 0 - _TIME_SYNC_PERIOD;
    static bool sync_modem = true;
    os_timer_t now = os_timer_get();

    
    if (now < (sync_time + _TIME_SYNC_PERIOD))
        return;

    if (gps_fix_ok())
    {
        gps_stamp_t gps;

        if (! gps_get_current_stamp(&gps))
            return;

        sync_time = now;
        _time_sync_utc(&gps.time);
    }
    else if (sync_modem && (modem_main_state() == MODEM_MAIN_STATE_NET_OK))
    {
        rtc_t time;

        sync_modem = false;
        if (modem_main_get_time(&time))
        {
            _time_sync_utc(&time);
        }
    }
}

static void _ble_rx_parser(char *data)
{
    buf_t *result = cmd_get_buf();

    app_main_led_single(0xFFFF, 10);
    
    if (os_timer_get() > (_ble_access.time + ACCESS_BLE_TIMEOUT))
        _ble_access.auth = ACCESS_NONE;

    _ble_access.time = os_timer_get();

    if (cmd_process_text(result, data, &_ble_access))
    {
        log_lock();
        ble_send(NL, strlen(NL));
        ble_send(buf_data(result), buf_length(result));
        ble_send(NL, strlen(NL));
        ble_send(TEXT_DONE, strlen(TEXT_DONE));
        log_unlock();
    }
    else
    {
        ble_send(TEXT_ERROR, strlen(TEXT_ERROR));
    }
    cmd_return_buf(result);
}

static void _ble_access_update(void)
{
    if ( _ble_access.auth == ACCESS_NONE)
        return; // inactive

    if (ble_connected() == false)
    {    // disconnected, clear access
         _ble_access.auth = ACCESS_NONE;
         LOG_INFO("BLE access disable");
    }
}

void app_reinit_req(cfg_id_t id)
{
    switch (id)
    {
    case CFG_ID_TRACER_ADDR:
    case CFG_ID_TRACER_PARAM:
        _reinit_mask |= _REINIT_TRACER;
        break;

    case CFG_ID_MAIN_SETUP:
        _reinit_mask |= _REINIT_MAIN;
        break;
    default:
        break;
    }
}

#define APP_INIT(msg,x)          \
    OS_DELAY(20);                \
    OS_PUTTEXT(NL);              \
    OS_PUTTEXT(msg); OS_FLUSH(); \
    if(x)                        \
        OS_PUTTEXT("OK ");       \
    else                         \
        OS_PUTTEXT("FAILED ");

void app_main_init (void)
{
    rtc_init();

    APP_INIT ("INIT STORAGE  ... ", storage_init());
    APP_INIT ("INIT CFG      ... ", cfg_init());
    APP_INIT ("INIT POWER    ... ", power_init());
    APP_INIT ("INIT IO       ... ", io_init());
    APP_INIT ("INIT CAN      ... ", can_init());
    APP_INIT ("INIT MODEM    ... ", modem_main_init());
    APP_INIT ("INIT EVENTS   ... ", event_init());
    APP_INIT ("INIT TRACER   ... ", tracer_init());
    APP_INIT ("INIT SHOCK    ... ", shock_init());
    APP_INIT ("INIT ANALOG   ... ", analog_init());
    APP_INIT ("INIT SYSTEM   ... ", system_init());
    APP_INIT ("INIT GPS      ... ", gps_init());
    APP_INIT ("INIT BLE      ... ", ble_init(_ble_rx_parser));
    
    app_main_reinit();
   
    _inp_state = io_get_inp();

    OS_PUTTEXT(NL);
    modem_main_rq_state(MODEM_MAIN_STATE_NET_OK);
    OS_DELAY(10);

    event_create(EVENT_ID_BOOT_UP, EVENT_SOURCE_UNIT);
}

void app_task_fast(void)
{   // tasks may take up to 10ms
    io_task();
    analog_task();
    system_task();
    gps_task();
    shock_task();
}

void app_task_slow(void)
{   // tasks may take up to 1s
    _io_input_task();
    _io_output_task();
    _led_update();
    power_task();
    storage_maintenance();
    tracer_task();
    event_task();
    ble_task();
    _ble_access_update();

    modem_main_data_rx_process();
}

void app_task_comm(void)
{   // tasks may take more than 1s
    _reinit_task();
    _time_sync_task();
    modem_main_task();
    gps_maintenance();

    if (modem_main_state() != MODEM_MAIN_STATE_NET_OK)
    {
        return;
    }

    tracer_comm_process();
    event_comm_task();
    sms_process();
    update_task();
}

void app_task_modem(void)
{
    modem_main_uart_rx_task();
}

void app_task_timer(void)
{   // 100ms precise timing
    led_tick(&led);
    led_tick(&siren);
    system_tick();
    alarm_tick();
    gps_tick();
}

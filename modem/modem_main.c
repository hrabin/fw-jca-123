#include "common.h"
#include "hardware.h"
#include "modem_main.h"
#include "modem_cfg.h"
#include "modem_at.h"
#include "modem_sms.h"
#include "modem.h"
#include "modem_bg95.h"
#include "sms.h"
#include "multi_sms.h"
#include "log.h"

LOG_DEF("MM");

#define MODEM_BAUDRATE 115200

#if (MODEM_ENABLED == 1)

static bool _modem_error = false;
static os_timer_t _timer;
#define MODEM_ERROR_TIMEOUT (60 * OS_TIMER_SECOND)

#define _SMS_DAY_LIMIT (50)
static u32 _sms_limiter = 0;
static u32 _err_wait_time = 0;

typedef struct {
    modem_t m;
    modem_model_e model;
    modem_main_state_e state;
    modem_main_state_e requested_state;
    modem_command_e command;
    os_timer_t tmout;
    int err_cnt;
} modem_state_t;

static modem_state_t _ms;

#define MODEM _ms.m

static void _hw_io_init(void)
{
    HW_MODEM_UART_INIT(MODEM_BAUDRATE);
    HW_MODEM_PWRKEY_INIT;
    HW_MODEM_OE_INIT;
    HW_MODEM_PON_TRIG_INIT;
    HW_MODEM_DTR_INIT;

    HW_MODEM_PWRKEY_HI;
    HW_MODEM_OE_LOW;
    HW_MODEM_DTR_HI;
    HW_MODEM_PON_TRIG_HI;
}

static void _hw_on(void)
{
    HW_MODEM_UART_ON;
    HW_MODEM_OE_HI;
    HW_MODEM_DTR_LOW; // == wakeup
    OS_DELAY(10);
    if (modem_at_ok_cmd_nolock(&_ms.m, "!AT"))
    {   // modem is alive, dont need to switch on
        LOG_DEBUG("skip PWRKEY");
        return;
    }
    LOG_DEBUG("PWRKEY");
    HW_MODEM_PWRKEY_LOW;
    OS_DELAY(1000);
    HW_MODEM_PWRKEY_HI;
}

static void _hw_off(void)
{
    const ascii *at_off = "AT+QPOWD=1";
    if (! modem_at_ok_cmd_nolock(&_ms.m, at_off))
    {
        OS_DELAY(1000);
        modem_at_cmd_nolock(&_ms.m, at_off);
    }
    HW_MODEM_OE_LOW;
    HW_MODEM_UART_OFF;
}

static void _hw_sleep(void)
{
    HW_MODEM_DTR_HI; // == sleep (when AT+QSCLK=1)
}

static void _hw_wakeup(void)
{
    HW_MODEM_OE_HI;
    HW_MODEM_UART_ON;
    HW_MODEM_DTR_LOW; // == wakeup
}

static bool _hw_tx_char(u8 c)
{
    return (HW_MODEM_UART_PUTCHAR(c));
}
static bool _hw_rx_char(u8 *c)
{
    s16 result;
    if ((result = HW_MODEM_UART_GETCHAR()) >= 0)
    {
        *c = result & 0xFF;
        return (true);
    }
    return (false);
}
    
bool modem_main_init(void)
{
    memset(&_ms, 0, sizeof(_ms));

    MODEM.pfunc_io_init  = _hw_io_init;
    MODEM.pfunc_on       = _hw_on;
    MODEM.pfunc_off      = _hw_off;
    MODEM.pfunc_sleep    = _hw_sleep;
    MODEM.pfunc_wakeup   = _hw_wakeup;
    MODEM.pfunc_tx_char  = _hw_tx_char;
    MODEM.pfunc_rx_char  = _hw_rx_char;
   
    // force BG95 setup without modem type detection
    MODEM.pfunc_init        = modem_bg95_init;
    MODEM.pfunc_check       = modem_bg95_check;
    MODEM.pfunc_urc         = modem_bg95_urc;
    MODEM.pfunc_udp_init    = modem_bg95_udp_init;
    MODEM.pfunc_udp_send    = modem_bg95_udp_send;
    MODEM.pfunc_udp_rx_task = modem_bg95_udp_rx_task; 

    modem_hw_init(&_ms.m);
    _ms.requested_state = MODEM_MAIN_STATE_OFF;
    _ms.tmout = os_timer_get() + 15*OS_TIMER_SECOND;
    ms_init();
    return (true);
}

bool modem_main_command(modem_command_e command)
{
    if (_ms.command != MODEM_COMMAND_NONE)
        return (false);

    _ms.command = command;
    return (true);
}

void modem_main_rq_state(modem_main_state_e state)
{
    if (_ms.requested_state != state)
    {   // status change request, discard timeut
        _ms.tmout = 0;
    }
    _ms.requested_state = state;
    _timer = os_timer_get() + MODEM_ERROR_TIMEOUT;
}

void modem_main_delivery_report (u8 sms_id, u8 result)
{
    // TODO
}

static void _set_state(modem_main_state_e new_state)
{
    os_timer_t now;

    if (new_state == _ms.state)
        return;
    
    now = os_timer_get();

    switch (new_state)
    {
    case MODEM_MAIN_STATE_OFF:
        _ms.tmout = now + _err_wait_time * OS_TIMER_SECOND;
        break;

    case MODEM_MAIN_STATE_ON:
        _ms.tmout = now + 1 * OS_TIMER_SECOND;
        break;

    case MODEM_MAIN_STATE_ERROR:
        _err_wait_time += 30; // in case permanent error increase off wait state
        _ms.tmout = now + 10 * OS_TIMER_SECOND;
        break;
    
    case MODEM_MAIN_STATE_INIT:
        _ms.tmout = now + (_modem_error ? 600 : 120) * OS_TIMER_SECOND;
        break;

    case MODEM_MAIN_STATE_NET_OK:
        _err_wait_time = 0;
        _ms.err_cnt = 0;
        _ms.tmout = now + 60 * OS_TIMER_SECOND;
        break;

    default:
        return;
    }
    _ms.state = new_state;
}

static void _modem_command_exec(modem_command_e cmd)
{
    switch (cmd)
    {
    case MODEM_COMMAND_ON:
        LOG_DEBUG("ON"); // modem_main_at_cmd
        modem_hw_on(&_ms.m);
        _set_state(MODEM_MAIN_STATE_ON);
        break;

    case MODEM_COMMAND_OFF:
        LOG_DEBUG("OFF");
        modem_hw_off(&_ms.m);
        _set_state(MODEM_MAIN_STATE_OFF);
        break;

    case MODEM_COMMAND_START:
        LOG_DEBUG("START");
        if (! modem_start(&_ms.m))
        {
            LOG_ERROR("start failed");
            _set_state(MODEM_MAIN_STATE_ERROR);
            break;
        }
        _set_state(MODEM_MAIN_STATE_INIT);
        break;

    case MODEM_COMMAND_DATA:
        break;

    case MODEM_COMMAND_HB:
        break;

    default:
        break;
    }
}

static void _state_machine_task(void)
{
    static os_timer_t ts = 0;

    os_timer_t now = os_timer_get();

    if (now < ts)
        return;

    switch (_ms.state)
    {
    case MODEM_MAIN_STATE_OFF:
        if (now < _ms.tmout)
            break;
        
        if (_ms.requested_state == MODEM_MAIN_STATE_OFF)
            return;

        if (_ms.requested_state >= MODEM_MAIN_STATE_INIT)
        {
            _modem_command_exec(MODEM_COMMAND_ON);
        }
        break;

    case MODEM_MAIN_STATE_ON:
        if (now < _ms.tmout)
            break;
        _modem_command_exec(MODEM_COMMAND_START);
        break;

    case MODEM_MAIN_STATE_ERROR:
        if (now < _ms.tmout)
            break;

        _modem_command_exec(MODEM_COMMAND_OFF);
        
        if (_ms.err_cnt >= 3)
        {
            LOG_ERROR("MODEM FAILURE");
            _ms.tmout = now + 300 * OS_TIMER_SECOND;
        }
        break;

    case MODEM_MAIN_STATE_INIT:
        if (_ms.m.flags & MODEM_FLAG_NET_READY)
        {
            LOG_INFO("NET OK");
            _set_state(MODEM_MAIN_STATE_NET_OK);
            break;
        }
        if (! modem_at_ok_cmd(&_ms.m, "AT+CREG?"))
        {
            LOG_ERROR("AT+CREG? failed");
            _set_state(MODEM_MAIN_STATE_ERROR);
            break;
        }
        ts = now + 10 * OS_TIMER_SECOND;

        if (now > _ms.tmout)
        {
            _ms.err_cnt++;
            LOG_ERROR("init failed e=%d", _ms.err_cnt);
            _set_state(MODEM_MAIN_STATE_ERROR);
            break;
        }
        break;

    case MODEM_MAIN_STATE_NET_OK:
        if (now < _ms.tmout)
            break;

        if ((_ms.m.flags & MODEM_FLAG_NET_READY) == 0)
        {   // lost network
            LOG_INFO("NET lost");
            _set_state(MODEM_MAIN_STATE_INIT);
            break;
        }

        if (! modem_check(&_ms.m))
        {
            _set_state(MODEM_MAIN_STATE_ERROR);
            break;
        }

        _ms.tmout = now + 60 * OS_TIMER_SECOND;
        break;

    }
}

static void _status_task(void)
{
    if (_ms.requested_state == _ms.state)
    {
        if (! _modem_error)
            return; 
        // end of error
        LOG_INFO("state OK");
        _modem_error = false;
        _err_wait_time = 0;
        return;
    }
    if (_modem_error)
        return; // already in error state
    
    // state is different than requested
    if (os_timer_get() < _timer)
        return;

    LOG_INFO("state FAIL");
    _modem_error = true;
}

static void _limiters_update(void)
{
    static os_timer_t timer = 0;
    os_timer_t now = os_timer_get();

    if (now < timer)
        return;
    timer = now + 24*60*OS_TIMER_MINUTE;
    _sms_limiter = 0;
}

modem_main_state_e modem_main_state(void)
{
    if (_modem_error)
        return (MODEM_MAIN_STATE_ERROR);

    if (_ms.requested_state == _ms.state)
        return _ms.state;

    return (MODEM_MAIN_STATE_INIT);
}

bool modem_main_lte(void)
{
    return (MODEM.flags & MODEM_FLAG_LTE ? true : false);
}

bool modem_main_sms_incomming(void)
{
    return (MODEM.sms_counter > 0 ? true : false);
}

bool modem_main_sms_read (sms_struct_t *sms)
{
    if (MODEM.sms_counter)
    {   // read only one SMS 
        if ((MODEM.flags & MODEM_FLAG_PIN_READY) == 0)
            return (false);

        OS_PUTTEXT ("\r\nREADING SMS ... ");
        if (modem_sms_read(&MODEM, sms)) // read SMS, allocate memory and delete this SMS
        {   // true = we have valid sms.data and sms.tel_num
            OS_PUTTEXT ("DONE ");
            return (true); 
        }
        OS_PUTTEXT ("FAILED ");
    }
    return (false);
}

bool modem_main_sms_send (sms_struct_t *sms)
{
    bool result;    
    
    if (++_sms_limiter >= _SMS_DAY_LIMIT)
    {
        LOG_ERROR("SMS limiter active");
        return (false);
    }

    result = modem_sms_send_now (&MODEM, sms);
    
    return (result);
}

bool modem_main_at_cmd(buf_t *response, const char *text)
{
    return (modem_at_cmd_get_response(&MODEM, response, text, NULL));
    // return (modem_at_ok_cmd(&_ms.m, text));
}

bool modem_echo(void)
{
    return ((MODEM.flags & MODEM_FLAG_ECHO) ? true : false);
}

void modem_echo_set(bool state)
{
    if (state)
        MODEM.flags |= MODEM_FLAG_ECHO;
    else
        MODEM.flags &= ~MODEM_FLAG_ECHO;

    LOG_DEBUG("ECHO=%s", (MODEM.flags & MODEM_FLAG_ECHO) ? "ON" : "OFF");
}

bool modem_main_data(void)
{
    return((MODEM.flags & MODEM_FLAG_DATA_READY) ? true : false);
}

bool modem_main_data_connect(const ascii *apn)
{
    if (MODEM.flags & MODEM_FLAG_DATA_READY)
        return (true);

    if (_ms.state < MODEM_MAIN_STATE_NET_OK)
        return (false);

    if (! modem_apn_init(&MODEM, apn))
    {
        LOG_ERROR("APN init failed");
        return (false);
    }
    if (! modem_data_connect(&MODEM))
        return (false);

    LOG_INFO("DATA READY");
    return (true);
}

void modem_main_data_disconnect(void)
{
    modem_data_disconnect(&MODEM);
}

bool modem_main_udp_send(udp_packet_t *packet)
{
    if (MODEM.pfunc_udp_send == NULL)
        return (false);
    
    return (MODEM.pfunc_udp_send(&MODEM, packet));
}

void modem_main_jamming(bool state)
{
    // TODO: some reaction to jamming
}

bool modem_main_roaming(void)
{
    return ((MODEM.flags & MODEM_FLAG_ROAMING) ? true : false);
}

bool modem_main_get_time(rtc_t *t)
{
    ascii result[32];
    buf_t buf;

    buf_init(&buf, result, sizeof(result));

    if (modem_at_cmd_get_response(&MODEM, &buf, "AT+CCLK?", "+CCLK: "))
    {   // i.e. '+CCLK: "21/05/03,10:21:44+08" '
        u32 Y,M,D,h,m,s;
        s32 z;
        
        if (sscanf (result+7, "\"%2" SCNu32 "/%2" SCNu32 "/%2" SCNu32 ",%2" SCNu32 ":%2" SCNu32 ":%2" SCNu32 "%" SCNd32,
                        &Y, &M, &D, &h, &m, &s, &z) == 7)
        {
            t->year  = Y;
            t->month = M;
            t->day   = D;
            t->hour  = h;
            t->minute= m;
            t->second= s;
            // the current time is in UTC 
            // NOTE: some other modems have local time instead of UTC
            return (true);
            // we may set time zone (z/4)
        }
        else
        {
            LOG_ERROR("resp: \"%s\"", result);
        }
    }
    return (false);
}

void modem_main_task(void)
{
    if (_ms.command != MODEM_COMMAND_NONE)
    {
        _modem_command_exec(_ms.command);
        _ms.command = MODEM_COMMAND_NONE;
    }
    _state_machine_task();
    _status_task();
    _limiters_update();
    
    if (MODEM.sleep == false)
    {   // reduce modem power consumption
        modem_at_lock(&MODEM);
        MODEM.pfunc_sleep();
        MODEM.sleep = true;
        LOG_DEBUGL(LOG_SELECT_SLEEP,"AT sleep");
        modem_at_unlock(&MODEM);
    }
}

void modem_main_data_rx_process(void)
{
    modem_data_rx_process(&MODEM);
}

void modem_main_uart_rx_task(void)
{
    modem_rx_task(&_ms.m);
}

#else //  (MODEM_ENABLED == 1)

//dummy functions
bool modem_main_init(void)
{
    LOG_INFO("HW OFF");
    return (false);
}

bool modem_main_command(modem_command_e command) {  return (false); }
void modem_main_task(void) { }

#endif // (MODEM_ENABLED != 1)


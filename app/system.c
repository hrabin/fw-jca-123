#include "common.h"
#include "alarm.h"
#include "app.h"
#include "cfg.h"
#include "event.h"
#include "io.h"
#include "log.h"
#include "section.h"
#include "system.h"
#include "tracer.h"

LOG_DEF("SYS");

#define _CFG_FORMAT "S%d"

static section_state_t _state = SECTION_ST_UNKNOWN; 
static os_timer_t _state_time;

u32 system_io_state = 0;
u32 system_int_state = 0;

static system_input_t _sys_inp[SYSTEM_INP_COUNT];

static section_t section;

#define _TICK_SECOND (10)

#define _DELAY_TMOUT (20 * _TICK_SECOND)
#define _DELAY_TMIN  (20 * 10) // [100ms]

static bool _tmin_running = false;
static u32 _tmout_timer = 0;

static void _save_state(void)
{
    buf_def(buf, CFG_ITEM_SIZE);
    buf_append_fmt(&buf, _CFG_FORMAT, _state);
    cfg_write(CFG_ID_STATE, &buf, ACCESS_SYSTEM);
}

static void _clear_inp(void)
{
    int i;
    for (i=0; i<SYSTEM_INP_COUNT; i++)
    {
        _sys_inp[i].tmr = 0;
        _sys_inp[i].cnt = 0;
    }
}

static void _set_state(section_state_t st, event_source_e source)
{
    if (st == _state)
        return;

    switch (st)
    {
    case SECTION_ST_UNSET:
        {
        bool after_alarm = false;
        if (alarm_state() != ALARM_IDLE)
        {
            after_alarm = true;
        }
        app_main_led(APP_LED_IDLE);
        alarm_stop(source);
        event_create(EVENT_ID_UNSET, source);
        _tmout_timer = 0;
        _tmin_running = false;
        _clear_inp();
        // 2x or 3x beep depending on alarm memory
        app_main_siren_beep(0x111, after_alarm ? 12 : 8);
        }
        break;

    case SECTION_ST_SET:
        app_main_led(APP_LED_SET);
        _tmout_timer = _DELAY_TMOUT;
        _clear_inp();
        event_create(EVENT_ID_SET, source);
        app_main_siren_beep(0x01,8);
        break;

    default:
        return;
    }
    _state = st;
    _state_time = os_timer_get();
}

bool system_init(void)
{
    buf_def(buf, CFG_ITEM_SIZE);
    int state;
    int i;

    memset(&_sys_inp, 0, sizeof(_sys_inp));
    for (i=0; i<SYSTEM_INP_COUNT; i++)
    {
        _sys_inp[i].nr = i;
        _sys_inp[i].reaction = ALARM_REACTION_INSTANT;
        _sys_inp[i].postpone = 1 * _TICK_SECOND;
    }
    _sys_inp[IO_INP1].reaction = ALARM_REACTION_DELAY;


    if (! cfg_read(&buf, CFG_ID_STATE , ACCESS_SYSTEM))
        return (false);

    section_init(&section);
    alarm_init();

    if (sscanf(buf_data(&buf), _CFG_FORMAT, &state) < 1)
        state = SECTION_ST_UNSET;

    _state = state;
    return (true);
}

section_state_t system_state(void)
{
    return (_state);
}

static event_source_e _inp_source(unsigned int n)
{
    const event_source_e INP_SOURCE[SYSTEM_INP_COUNT] = {
        EVENT_SOURCE_KEY, EVENT_SOURCE_LOCK, EVENT_SOURCE_LOCK, EVENT_SOURCE_DOOR, EVENT_SOURCE_INPUT1, EVENT_SOURCE_SHOCK
    };
    return (INP_SOURCE[n]);
}

static void _start_alarm(unsigned int n)
{
    event_create(EVENT_ID_ALARM, _inp_source(n));
    alarm_trigger();
}

static bool _input_alarm(system_input_t *inp)
{
    if (++inp->cnt > ALARM_MAX_CNT)
        return (false);
    
    if (inp->postpone)
    {
        if (inp->tmr == 0)
            inp->tmr = inp->postpone;
        return (false);
    }
    return (true);
}

static bool _input_delay_alarm(system_input_t *inp)
{
    if (++inp->cnt > ALARM_MAX_CNT)
        return (false);

    if (inp->tmr == 0)
    {
        inp->tmr = _DELAY_TMIN;
        LOG_INFO("tmin %d start", inp->nr);
        return (true);
    }
    return (false);
}

static void _inp_tick(void)
{
    int i;
    for (i=0; i<SYSTEM_INP_COUNT; i++)
    {
        system_input_t *inp = &_sys_inp[i];
        OS_ASSERT(inp->nr == i, "inp integrity failed");

        if (inp->tmr)
        {
            if (--inp->tmr == 0)
            {
                LOG_DEBUG("tmin %d done", i);
                _start_alarm(i);
            }
        }
    }
}

void system_inp_activated(unsigned int n)
{
    system_input_t *inp;
    OS_ASSERT(n < SYSTEM_INP_COUNT, "inp_act");
    
    inp = &_sys_inp[n];

    switch (n)
    {
    case IO_SHOCK: app_main_led_single(APP_LED_SHOCK); break;
    default:
        break;
    }

    switch (_state)
    {
    case SECTION_ST_SET:
        {
        u32 set_time = os_timer_get() - _state_time;

        switch (inp->reaction)
        {
        case ALARM_REACTION_DELAY:
            if (_tmout_timer)
                break;

            if (_input_delay_alarm(inp))
            {
                _tmin_running = true;
            }
            break;

        case ALARM_REACTION_INSTANT:
            if (set_time < 2*OS_TIMER_SECOND)
                break;

        case ALARM_REACTION_24H:
            if (_input_alarm(inp))
                _start_alarm(n);
            break;
        default:
            break;
        }
        }
        break;

    case SECTION_ST_UNSET:
        switch (inp->reaction)
        {
        case ALARM_REACTION_24H:
            if (_input_alarm(inp))
                _start_alarm(n);
            break;
        default:
            break;
        }

        switch (n)
        {
        case IO_KEY:
            // start tracking
            tracer_start();
            break;

        case IO_SHOCK:
            tracer_shock_start(true);
            break;
        }
        break;

    default:
        break;
    }

}

void system_inp_deactivated(unsigned int n)
{
    switch (n)
    {
    case IO_KEY:
        // stop tracking
        tracer_stop();
        break;

    case IO_SHOCK:
        tracer_shock_start(false);
        break;
    }
}

void system_task(void)
{
    section_task(&section);
/*    switch (_state)
    {
    case SECTION_ST_SET:
        break;
    case SECTION_ST_UNSET:
        break;
    default:
        break;
    }
 */
}

void system_tick(void)
{   // precise 100ms timing
    _inp_tick();

    if (_tmout_timer)
    {
        if (--_tmout_timer == 0)
        {
            LOG_DEBUG("tmout done");
        }
    }
}

void system_set(void)
{
    _set_state(SECTION_ST_SET, EVENT_SOURCE_UNIT);
    _save_state();
}

void system_unset(void)
{
    _set_state(SECTION_ST_UNSET, EVENT_SOURCE_UNIT);
    _save_state();
}

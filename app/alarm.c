#include "common.h"
#include "alarm.h"
#include "app.h"
#include "event.h"
#include "io.h"
#include "log.h"

LOG_DEF("ALARM");

static alarm_state_e _state = ALARM_IDLE; 
static bool _siren_state = OFF;

#define _SIREN_TIME (30 * 10) // [100ms]
static int _siren_tmr = 0;

#define _ALARM_TIME (120 * 10) // [100ms]
static int _alarm_tmr = 0;

#define _ALARMS_MAX 3
int _alarm_cnt = 0;

static void _siren_on(void)
{
    if (_siren_state == ON)
        return;

    _siren_tmr = _SIREN_TIME;
    _siren_state = ON;
    io_set_out(IO_SIREN, _siren_state);
}

static void _siren_off(void)
{
    if (_siren_state == OFF)
        return;

    _siren_state = OFF;
    io_set_out(IO_SIREN, _siren_state);
}

bool alarm_init(void)
{
    _state = ALARM_IDLE;
    _alarm_cnt = 0;
    _alarm_tmr = 0;
    _siren_off();
    return (true);
}

alarm_state_e alarm_state(void)
{
    return (_state);
}

void alarm_tick(void)
{   // 100ms
    if (_siren_tmr)
    {
        if (--_siren_tmr == 0)
        {
            LOG_DEBUG("siren tmout");
            _siren_off();
        }
    }
    if (_alarm_tmr)
    {
        if (--_alarm_tmr == 0)
        {
            LOG_DEBUG("alarm tmout");
            _state = ALARM_TIMEOUT;
        }
    }
}

void alarm_trigger(void)
{  
    if (_state == ALARM_ACTIVE)
        return;

    _alarm_cnt++;

    LOG_WARNING("nr %d", _alarm_cnt);

    if (_alarm_cnt > _ALARMS_MAX)
        return;

    _state = ALARM_ACTIVE;
    _alarm_tmr = _ALARM_TIME;
    _siren_on();
}

void alarm_stop(event_source_e source)
{
    _siren_off();
    if (_state == ALARM_ACTIVE)
    {
        event_create(EVENT_ID_ALARM_CANCEL, source);
    }

    _alarm_tmr = 0;
    _alarm_cnt = 0;
    _state = ALARM_IDLE;
}


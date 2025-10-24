#include "common.h"
#include "system_input.h"
#include "system.h"
#include "log.h"

LOG_DEF("S_INP");

#define _TICK_SECOND (10)

static system_input_t _sys_inp[SYSTEM_INP_COUNT];

void system_input_reset(void)
{
    int i;

    for (i=0; i<SYSTEM_INP_COUNT; i++)
    {
        _sys_inp[i].tmr = 0;
        _sys_inp[i].cnt = 0;
    }
}

void system_input_init(void)
{
    int i;

    memset(&_sys_inp, 0, sizeof(_sys_inp));

    for (i=0; i<SYSTEM_INP_COUNT; i++)
    {
        _sys_inp[i].nr = i;
        _sys_inp[i].reaction = ALARM_REACTION_INSTANT;
        _sys_inp[i].postpone = 1 * _TICK_SECOND;
    }
    _sys_inp[IO_INP1].reaction = ALARM_REACTION_DELAY;

}

event_source_e system_input_source(system_input_id_t n)
{
    const event_source_e INP_SOURCE[SYSTEM_INP_COUNT] = {
        EVENT_SOURCE_KEY, EVENT_SOURCE_LOCK, EVENT_SOURCE_LOCK, EVENT_SOURCE_DOOR, EVENT_SOURCE_INPUT1, EVENT_SOURCE_SHOCK
    };
    OS_ASSERT(n<SYSTEM_INP_COUNT, "system_inp_source()");
    return (INP_SOURCE[n]);
}

system_input_t *system_input_get(system_input_id_t n)
{
    OS_ASSERT(n < SYSTEM_INP_COUNT, "inp_act");

    return (&_sys_inp[n]);
}

bool system_input_alarm(system_input_t *inp)
{
    if (inp->cnt >= ALARM_MAX_CNT)
        return (false);
    
    inp->cnt++;

    if (inp->postpone)
    {
        if (inp->tmr == 0)
            inp->tmr = inp->postpone;
        return (false);
    }
    return (true);
}

void system_input_tick(void)
{
    int n;

    for (n=0; n<SYSTEM_INP_COUNT; n++)
    {
        system_input_t *inp = &_sys_inp[n];
        OS_ASSERT(inp->nr == n, "inp integrity failed");

        if (inp->tmr)
        {
            if (--inp->tmr == 0)
            {
                LOG_DEBUG("tmin %d done", n);
                system_inp_signal(n, inp->reaction);
                // section_signal(&section, inp);
            }
        }
    }
}



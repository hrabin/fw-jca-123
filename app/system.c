#include "common.h"
#include "alarm.h"
#include "app.h"
#include "cfg.h"
#include "io.h"
#include "log.h"
#include "section.h"
#include "system.h"
#include "tracer.h"

LOG_DEF("SYS");

#define _CFG_FORMAT "S%d"

#define _DELAY_TMOUT (20) // [s]
#define _DELAY_TMIN  (10) // [s]


u32 system_io_state = 0;
u32 system_int_state = 0;

static section_t section;

static void _save_state(void)
{
    buf_def(buf, CFG_ITEM_SIZE);
    buf_append_fmt(&buf, _CFG_FORMAT, section.data.state);
    cfg_write(CFG_ID_STATE, &buf, ACCESS_SYSTEM);
}

bool system_init(void)
{
    buf_def(buf, CFG_ITEM_SIZE);
    int state;

    system_input_init();

    if (! cfg_read(&buf, CFG_ID_STATE , ACCESS_SYSTEM))
        return (false);

    section.setup.num    =  0; // ve have only one section here (index 0)
    section.setup.tm_out = _DELAY_TMOUT * OS_TIMER_SECOND;
    section.setup.tm_in  = _DELAY_TMIN * OS_TIMER_SECOND;

    section_init(&section);
    alarm_init();

    if (sscanf(buf_data(&buf), _CFG_FORMAT, &state) < 1)
        state = SECTION_ST_UNSET;

    section.data.state = state;
    return (true);
}

section_state_t system_state(void)
{
    return (section.data.state);
}

void system_inp_activated(unsigned int n)
{
    system_input_t *inp = system_input_get(n);

    switch (n)
    {
    case IO_SHOCK: app_main_led_single(APP_LED_SHOCK); break;
    default:
        break;
    }

    if (section_alarm_enabled(&section, inp->reaction))
    {
        if (system_input_alarm(inp))
            section_signal(&section, inp, inp->reaction);
    }

    if (section.data.state == SECTION_ST_UNSET)
    {
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

void system_inp_signal(unsigned int n, alarm_reaction_e signal)
{
    section_signal(&section, system_input_get(n), signal);
}

void system_set(access_t *access)
{
    if (section_set(&section, access))
    {
        system_input_reset();
        app_main_siren_beep(0x01,8);
   
        _save_state();
    }
}

void system_unset(access_t *access)
{
    if (section_unset(&section, access))
    {
        bool after_alarm = false;
        if (alarm_state() == ALARM_ACTIVE)
        {
            event_create(EVENT_ID_ALARM_CANCEL, access->source);
        }
        if (alarm_state() != ALARM_IDLE)
        {
            after_alarm = true;
        }
        alarm_stop();
        system_input_reset();
        // 2x or 3x beep depending on alarm memory
        app_main_siren_beep(0x111, after_alarm ? 12 : 8);
        _save_state();
    }
}

void system_event(event_id_e e)
{
    switch (e)
    {
    case EVENT_ID_SET:
        app_main_led_single(0xFF, 8);
        break;

    case EVENT_ID_UNSET:
        app_main_led_single(0xF0F, 12);
        break;

    case EVENT_ID_ALARM:
        alarm_trigger();
        break;
    }
}

void system_tick(void)
{   // precise 100ms timing
    system_input_tick();
}

void system_task(void)
{
    section_task(&section);
}


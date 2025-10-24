#include "common.h"
#include "app.h"
#include "event.h"
#include "section.h"
#include "system.h"
#include "log.h"

LOG_DEF("SECTION");


static void _event(section_t *s, event_id_e event, event_source_e source)
{
    // TODO: for multiple section support update using s->setup.num
    event_create(event, source);
}

static void _set_timer(section_t *s, u32 tm)
{
    s->data.timer = os_timer_get() + tm;
}

static void _flag_set(section_t *s, u32 flag)
{
    s->data.flags |= flag;
}

static void _flag_clr(section_t *s, u32 flag)
{
    s->data.flags &= ~flag;
}

static bool _set_state(section_t *s, section_state_t st, event_source_e source)
{
    section_data_t *d = &(s->data);

    if (st == d->state)
        return false;

    switch (st)
    {
    case SECTION_ST_UNSET:
        _flag_clr(s, SECTION_FLAG_TM_IN | SECTION_FLAG_ALARM);
        d->timer = 0;
        _event(s, EVENT_ID_UNSET, source);
        break;

    // case SECTION_ST_SET_PART: // TODO
    case SECTION_ST_SET:
        _set_timer(s, s->setup.tm_out);
        _flag_set(s, SECTION_FLAG_TM_OUT);
        _flag_clr(s, SECTION_FLAG_ALARM);
        _event(s, EVENT_ID_SET, source);
        break;

    default:
        return false;
    }
    d->state = st;
    d->state_time = os_timer_get();
    return true;
}

static void _instant_alarm(section_t *s, system_input_t *inp)
{
    LOG_INFO("ALARM");
    _flag_set(s, SECTION_FLAG_ALARM);
    _event(s, EVENT_ID_ALARM, system_input_source(inp->nr));
}

static void _delay_alarm(section_t *s, system_input_t *inp)
{
    LOG_INFO("delay ALARM");
    if (s->data.timer == 0)
    {
        s->data.alarm_inp = inp;

        _set_timer(s, s->setup.tm_in);
        _flag_set(s, SECTION_FLAG_TM_IN);
    }
}

void section_init(section_t *s)
{
    memset(&(s->data), 0, sizeof(s->data));
    s->data.state = SECTION_ST_UNKNOWN;
}

bool section_alarm_enabled(section_t *s, alarm_reaction_e signal)
{   // return true when current signal may cause alarm
    switch (s->data.state)
    {
    case SECTION_ST_UNSET:
        if (signal == ALARM_REACTION_24H)
            return (true);
        break;

    case SECTION_ST_SET_PART:
    case SECTION_ST_SET:
        if (signal != ALARM_REACTION_NONE)
            return (true);
        break;

    default:
         break;
    }
    return (false);
}

void section_signal(section_t *s, system_input_t *inp, alarm_reaction_e signal)
{
    LOG_INFO("inp %d, signal %d, src %d", inp->nr, signal, system_input_source(inp->nr));

    switch (s->data.state)
    {
    case SECTION_ST_SET_PART:
    case SECTION_ST_SET:
        switch (signal)
        {
        case ALARM_REACTION_DELAY:
            _delay_alarm(s, inp);
            break;

        case ALARM_REACTION_INSTANT:
        case ALARM_REACTION_24H:
            _instant_alarm(s, inp);
            break;

        default:
            break;
        }

        break;

    case SECTION_ST_UNSET:
        switch (signal)
        {
        case ALARM_REACTION_24H:
            _instant_alarm(s, inp);
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }
}

bool section_set(section_t *s, access_t *access)
{
    return (_set_state(s, SECTION_ST_SET, access->source));
}

bool section_unset(section_t *s, access_t *access)
{
    return (_set_state(s, SECTION_ST_UNSET, access->source));
}

void section_task(section_t *s)
{
    section_data_t *d = &(s->data);

    if (d->timer != 0)
    {   // timer is set, something running
        if (os_timer_get() > d->timer)
        {
            d->timer = 0;
            if (d->flags & SECTION_FLAG_TM_OUT)
            {
                LOG_DEBUG("tmout done");
                _flag_clr(s, SECTION_FLAG_TM_OUT);
            }
            if (d->flags & SECTION_FLAG_TM_IN)
            {
                LOG_DEBUG("tmin done");
                OS_ASSERT(d->alarm_inp != NULL, "alarm_inp");
                _instant_alarm(s, d->alarm_inp);
                _flag_clr(s, SECTION_FLAG_TM_IN);
            }
        }
    }
}

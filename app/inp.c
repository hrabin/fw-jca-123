#include "common.h"
#include "inp.h"

#include "hardware.h"
#include "gpio.h"
#include "shock.h"
#include "analog.h"

#include "log.h"
LOG_DEF("INP");

#define INP_FILTER_LIMIT_DEFAULT       (100 * OS_TIMER_MS)
#define INP_FILTER_LIMIT_DEFAULT_DACT  (500 * OS_TIMER_MS)

static volatile bool _inp_enabled = false; // in case power fail we need to disable inp which could not work

static u32 _inp_direction; // 1 == active LOW
static u32 _inp_state;
static u32 _inp_check_needed;
static u32 _inp_bypass;

static os_timer_t _now;

typedef struct {
    os_timer_t timer;
    os_timer_t limit;
    os_timer_t limit_dact;
} inp_filter_t;

static inp_filter_t _inp_filter[INP_SIZE];

static bool _inp_lo_level_active(u8 pin_id)
{
    switch (pin_id)
    {
    case INP_KEY:
        if (HW_KEY_IN)
            return (true);
        break;
    case INP_LOCK:
        if (HW_LOCK_IN)
            return (true);
        break;
    case INP_UNLOCK:
        if (HW_UNLOCK_IN)
            return (true);
        break;
    case INP_DOOR:
        if (HW_DOOR_IN)
            return (true);
        break;
    case INP_INP1:
        if (HW_INP1_IN)
            return (true);
        break;

    case INP_SHOCK:
        if (shock_detected())
            return (true);
        break;
    }
    return (false);
}

static bool _inp_inverted(u8 pin_id)
{
    if (_inp_direction & (1<<pin_id))
        return (true);
    return (false);
}

static bool _inp_power_needed(u8 pin_id)
{
    if (pin_id == INP_SHOCK)
        return (false); // shock detector does not need main power

    return (true);
}

static void _inp_active (u8 pin_id)
{
    if (_inp_state & (1<<pin_id))
    {   // no change, input is active
        _inp_filter[pin_id].timer = _now + _inp_filter[pin_id].limit_dact;
        return;
    }
    if (_now < _inp_filter[pin_id].timer)
        return;

    if (_inp_check_needed & (1<<pin_id))
        return; // previous change not yet registered

    _inp_state |= (1<<pin_id);
    _inp_check_needed |= (1<<pin_id);
    _inp_filter[pin_id].timer = _now + _inp_filter[pin_id].limit;
}

static void _inp_inactive (u8 pin_id)
{
    if ((_inp_state & (1<<pin_id)) == 0)
    {   // no change, input is inactive
        _inp_filter[pin_id].timer = _now + _inp_filter[pin_id].limit;
        return;
    }
    if (_now < _inp_filter[pin_id].timer)
        return;

    if (_inp_check_needed & (1<<pin_id))
        return; // previous change not yet registered

    _inp_state &= ~(1<<pin_id);
    _inp_check_needed |= (1<<pin_id);
    _inp_filter[pin_id].timer = _now + _inp_filter[pin_id].limit_dact;
}

bool inp_init(void)
{
    u16 i;

    HW_INP1_INIT;
    HW_KEY_INIT;
    HW_DOOR_INIT;
    HW_LOCK_INIT;
    HW_UNLOCK_INIT;

    memset(&_inp_filter, 0, sizeof(_inp_filter));
    _inp_direction = (1 << INP_LOCK) | (1 << INP_UNLOCK) | (1 << INP_DOOR) | (1 << INP_INP1);

    _inp_state = 0;
    _inp_bypass = 0;

    // align state to default (inactive)
    for (i=0; i<INP_SIZE; i++)
    {
        _inp_filter[i].limit = INP_FILTER_LIMIT_DEFAULT;
        _inp_filter[i].limit_dact = INP_FILTER_LIMIT_DEFAULT_DACT;
        if (_inp_inverted(i) != _inp_lo_level_active(i))
                _inp_state |= (1<<i);
    }
     _inp_filter[INP_SHOCK].limit = 0; // filtered on detection side
     _inp_filter[INP_SHOCK].limit_dact = 0;
    return (true);
}

const ascii *inp_name(int pin)
{
    switch (pin)
    {
    case INP_KEY:        return ("KEY");
    case INP_LOCK:    return ("LOCK");
    case INP_UNLOCK:  return ("UNLOCK");
    case INP_DOOR:       return ("DOOR");
    case INP_INP1:       return ("INP");
    case INP_SHOCK:      return ("SHOCK");
    case INP_NONE: return ("none");
    default:            return ("unknown");
    }
}

u32 inp_state(void)
{
    // LOG_DEBUG("i=%x, ll=%x, ch=%x", _inp_state, _inp_ll(), _inp_check_needed);
    return (_inp_state);
}

u32 inp_get(void)
{   // read registered state
    u32 state = _inp_state;
    _inp_check_needed = 0;
    return (state);
}

void inp_enable(bool state)
{
    _inp_enabled = state;
}

void inp_set_bypass(u8 inp, bool state)
{
    if (inp >= INP_SIZE)
        return;
    if (state)
        _inp_bypass |= (1<<inp);
    else
        _inp_bypass &= ~(1<<inp);
}

static bool _inp_is_bypass(int inp)
{
    return ((_inp_bypass & (1<<inp)) ? true : false);
}

void inp_task(void)
{
    u16 i;
    bool inp_enabled = _inp_enabled;

    _now = os_timer_get();

    if (inp_enabled)
    {   // detect power loss and disable inputs which cant work
        if (analog_main_mv() < 7000)
            inp_enabled = false;
    }

    for (i=0; i<INP_SIZE; i++)
    {
        if (! inp_enabled)
        {
            if (_inp_power_needed(i))
                continue;
        }

        if (_inp_is_bypass(i))
            continue;

        if (_inp_lo_level_active(i) == _inp_inverted(i))
            _inp_inactive(i);
        else
            _inp_active(i);
    }
}

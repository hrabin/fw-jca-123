#include "common.h"
#include "io.h"

#include "hardware.h"
#include "gpio.h"
#include "shock.h"
#include "analog.h"

#include "log.h"
LOG_DEF("IO");

#define INP_FILTER_LIMIT_DEFAULT       (100 * OS_TIMER_MS)
#define INP_FILTER_LIMIT_DEFAULT_DACT  (500 * OS_TIMER_MS)

volatile bool _io_enabled = false; // in case power fail we need to disable io which could not work

static u32 _inp_direction; // 1 == active LOW
static u32 _inp_state;
static u32 _out_state;
static u32 _inp_check_needed;
static u32 _inp_bypass;

os_timer_t _now;

typedef struct {
    os_timer_t timer;
    os_timer_t limit;
    os_timer_t limit_dact;
} io_filter_t;

static io_filter_t _io_filter[IO_INPUT_SIZE];

bool _inp_lo_level_active(u8 pin_id)
{
    switch (pin_id)
    {
    case IO_KEY:
        if (HW_KEY_IN)
            return (true);
        break;
    case IO_LOCK_IN:
        if (HW_LOCK_IN)
            return (true);
        break;
    case IO_UNLOCK_IN:
        if (HW_UNLOCK_IN)
            return (true);
        break;
    case IO_DOOR:
        if (HW_DOOR_IN)
            return (true);
        break;
    case IO_INP1:
        if (HW_INP1_IN)
            return (true);
        break;

    case IO_SHOCK:
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
    if (pin_id == IO_SHOCK)
        return (false); // shock detector does not need main power

    return (true);
}

static void _inp_active (u8 pin_id)
{
    if (_inp_state & (1<<pin_id))
    {   // no change, input is active
        _io_filter[pin_id].timer = _now + _io_filter[pin_id].limit_dact;
        return;
    }
    if (_now < _io_filter[pin_id].timer)
        return;

    if (_inp_check_needed & (1<<pin_id))
        return; // previous change not yet registered 

    _inp_state |= (1<<pin_id);
    _inp_check_needed |= (1<<pin_id);
    _io_filter[pin_id].timer = _now + _io_filter[pin_id].limit;
}

static void _inp_inactive (u8 pin_id)
{
    if ((_inp_state & (1<<pin_id)) == 0)
    {   // no change, input is inactive
        _io_filter[pin_id].timer = _now + _io_filter[pin_id].limit;
        return;
    }
    if (_now < _io_filter[pin_id].timer)
        return;

    if (_inp_check_needed & (1<<pin_id))
        return; // previous change not yet registered

    _inp_state &= ~(1<<pin_id);
    _inp_check_needed |= (1<<pin_id);
    _io_filter[pin_id].timer = _now + _io_filter[pin_id].limit_dact;
}

bool io_init(void)
{
    int i;

    // inputs
    HW_INP1_INIT;
    HW_KEY_INIT;
    HW_DOOR_INIT;
    HW_LOCK_INIT;
    HW_UNLOCK_INIT;

    // outputs
    HW_SIREN_LOW;
    HW_SIREN_INIT;
    HW_LOCK_OUT_LOW;
    HW_LOCK_OUT_INIT;
    HW_UNLOCK_OUT_LOW;
    HW_UNLOCK_OUT_INIT;
    
    memset(&_io_filter, 0, sizeof(_io_filter));
    _inp_direction = (1 << IO_LOCK_IN) | (1 << IO_UNLOCK_IN) | (1 << IO_DOOR) | (1 << IO_INP1);

    
    _inp_state = 0;
    _out_state = 0;
    _inp_bypass = 0;

    // align state to default (inactive)
    for (i=0; i<IO_INPUT_SIZE; i++)
    {
        _io_filter[i].limit = INP_FILTER_LIMIT_DEFAULT;
        _io_filter[i].limit_dact = INP_FILTER_LIMIT_DEFAULT_DACT;
        if (_inp_inverted(i) != _inp_lo_level_active(i))
                _inp_state |= (1<<i);
    }
     _io_filter[IO_SHOCK].limit = 0; // filtered on detection side
     _io_filter[IO_SHOCK].limit_dact = 0;
    return (true);
}

const ascii *io_inp_name(int pin)
{
    switch (pin)
    {
    case IO_KEY:        return ("KEY");
    case IO_LOCK_IN:    return ("LOCK");
    case IO_UNLOCK_IN:  return ("UNLOCK");
    case IO_DOOR:       return ("DOOR");
    case IO_INP1:       return ("INP");
    case IO_SHOCK:      return ("SHOCK");
    case IO_INPUT_NONE: return ("none");
    default:            return ("unknown");
    }
}

const ascii *io_out_name(int pin)
{
    switch (pin)
    {
    case IO_LOCK_OUT:   return ("LOCK");
    case IO_UNLOCK_OUT: return ("UNLOCK");
    case IO_SIREN:      return ("SIREN");
    default:            return ("unknown");
    }
}

/*static u32 _inp_ll(void)
{
    u32 inp = 0;
    int i;

    for (i=0; i<IO_INPUT_SIZE; i++)
    {
        if (_inp_lo_level_active(i))
            inp |= (1 << i);
    }
}*/

u32 io_inp_state(void)
{
    // LOG_DEBUG("i=%x, ll=%x, ch=%x", _inp_state, _inp_ll(), _inp_check_needed);
    return (_inp_state);
}

u32 io_out_state(void)
{
    return (_out_state);
}

u32 io_get_inp(void)
{   // read registered state
    u32 state = _inp_state;
    _inp_check_needed = 0;
    return (state);
}


void io_set_out(u8 pin, bool state)
{
    if (pin >= IO_OUTPUT_SIZE)
        return;

    // NOTE: lock/unlock is active LOW
    if (state)
    {
        if (_out_state & (1 << pin))
            return;

        switch (pin)
        {
        case IO_LOCK_OUT:   
            if (HW_UNLOCK_IN == 0) 
                return; // blocked
            HW_LOCK_OUT_ON;
            break;

        case IO_UNLOCK_OUT: 
            if (HW_LOCK_IN == 0)
                return; // blocked
            HW_UNLOCK_OUT_ON;
            break;

        case IO_SIREN: HW_SIREN_ON; break;
        default: return;
        }
        _out_state |= (1 << pin);
    }
    else
    {
        if ((_out_state & (1 << pin)) == 0)
            return;

        switch (pin)
        {
        case IO_LOCK_OUT:   HW_LOCK_OUT_OFF; break;
        case IO_UNLOCK_OUT: HW_UNLOCK_OUT_OFF; break;
        case IO_SIREN:      HW_SIREN_OFF; break;
        default: return;
        }
        _out_state &= ~(1 << pin);
    }
}

void io_enable(bool state)
{
    _io_enabled = state;
}

void io_inp_set_bypass(u8 inp, bool state)
{
    if (inp >= IO_INPUT_SIZE)
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

void io_task(void)
{
    int i;
    bool io_enabled = _io_enabled;

    _now = os_timer_get();
    
    if (io_enabled)
    {   // detect power loss and disable inputs which cant work
        if (analog_main_mv() < 7000)
            io_enabled = false;
    }

    for (i=0; i<IO_INPUT_SIZE; i++)
    {
        if (! io_enabled)
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


#include "common.h"
#include "out.h"

#include "hardware.h"
#include "gpio.h"

#include "log.h"
LOG_DEF("OUT");

static u32 _out_state;

bool out_init(void)
{
    HW_SIREN_LOW;
    HW_SIREN_INIT;
    HW_LOCK_OUT_LOW;
    HW_LOCK_OUT_INIT;
    HW_UNLOCK_OUT_LOW;
    HW_UNLOCK_OUT_INIT;

    _out_state = 0;
    return (true);
}

const ascii *out_name(int pin)
{
    switch (pin)
    {
    case OUT_LOCK:   return ("LOCK");
    case OUT_UNLOCK: return ("UNLOCK");
    case OUT_SIREN:      return ("SIREN");
    default:            return ("unknown");
    }
}

u32 out_state(void)
{
    return (_out_state);
}

void out_set(u8 pin, bool state)
{
    if (pin >= OUT_SIZE)
        return;

    // NOTE: lock/unlock is active LOW
    if (state)
    {
        if (_out_state & (1 << pin))
            return;

        switch (pin)
        {
        case OUT_LOCK:
            if (HW_UNLOCK_IN == 0)
                return; // blocked
            HW_LOCK_OUT_ON;
            break;

        case OUT_UNLOCK:
            if (HW_LOCK_IN == 0)
                return; // blocked
            HW_UNLOCK_OUT_ON;
            break;

        case OUT_SIREN: HW_SIREN_ON; break;
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
        case OUT_LOCK:   HW_LOCK_OUT_OFF; break;
        case OUT_UNLOCK: HW_UNLOCK_OUT_OFF; break;
        case OUT_SIREN:      HW_SIREN_OFF; break;
        default: return;
        }
        _out_state &= ~(1 << pin);
    }
}

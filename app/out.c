#include "common.h"
#include "out.h"

#include "hardware.h"
#include "gpio.h"
#include "system.h"

#include "cfg.h"
#include "log.h"
LOG_DEF("OUT");

// Physical output -> system_io_state bit exported to that output.
// Defaults below; CFG_ID_OUT_MAP may override (invalid content -> defaults).
static const u32 _out_table_default[OUT_SIZE] = {
    [OUT_LOCK]   = SYSTEM_IO_LOCK,
    [OUT_UNLOCK] = SYSTEM_IO_UNLOCK,
    [OUT_SIREN]  = SYSTEM_IO_SIREN,
};

static u32 _out_table[OUT_SIZE];
static u32 _out_state;

static bool _out_bit_valid(s32 bit)
{
    return ((bit >= 0) && (bit < 32));
}

void out_table_reinit(void)
{   // (re)load the OUT remap (CFG_ID_OUT_MAP) and re-apply it to the pins.
    // CFG_ID_OUT_MAP = "<lock>,<unlock>,<siren>" system_io_state bit numbers
    s32 lock, unlock, siren;
    buf_def(buf, CFG_ITEM_SIZE);

    memcpy(_out_table, _out_table_default, sizeof(_out_table));

    if (cfg_read(&buf, CFG_ID_OUT_MAP, ACCESS_SYSTEM)
        && (sscanf(buf_data(&buf), "%" SCNd32 ",%" SCNd32 ",%" SCNd32,
                   &lock, &unlock, &siren) == 3)
        && _out_bit_valid(lock) && _out_bit_valid(unlock) && _out_bit_valid(siren))
    {   // valid content overrides the defaults
        _out_table[OUT_LOCK]   = BIT(lock);
        _out_table[OUT_UNLOCK] = BIT(unlock);
        _out_table[OUT_SIREN]  = BIT(siren);
    }

    out_update();
}

bool out_init(void)
{
    HW_SIREN_LOW;
    HW_SIREN_INIT;
    HW_LOCK_OUT_LOW;
    HW_LOCK_OUT_INIT;
    HW_UNLOCK_OUT_LOW;
    HW_UNLOCK_OUT_INIT;

    _out_state = 0;
    out_table_reinit();
    return (true);
}

const ascii *out_name(int pin)
{
    switch (pin)
    {
    case OUT_LOCK:   return ("LOCK");
    case OUT_UNLOCK: return ("UNLOCK");
    case OUT_SIREN:  return ("SIREN");
    default:         return ("unknown");
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
        case OUT_SIREN:  HW_SIREN_OFF; break;
        default: return;
        }
        _out_state &= ~(1 << pin);
    }
}

void out_update(void)
{   // drive physical outputs from system_io_state via out_table
    u8 i;

    for (i=0; i<OUT_SIZE; i++)
    {
        out_set(i, (system_io_state & _out_table[i]) != 0);
    }
}

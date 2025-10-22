#include "platform_setup.h"
#include "hardware.h"
#include "analog.h"
#include "event.h"
#include "io.h"

#include "log.h"

LOG_DEF("POWER");

// 3.8V lithium backup battery
#define V_BATT_CONNECTED  2000 // [mV]
#define V_BATT_EMPTY      2800 // [mV]
#define V_BATT_OK         3200 // [mV]
#define V_BATT_FULL       4000 // [mV]
#define V_BATT_DROP        400 // max drop voltage when testing

// 12V main source 
#define V_VCC_CHARGE_EN  12600 // [mV]
#define V_VCC_EMPTY      11800 // [mV]
#define V_VCC_LOST        8000 // [mV]

#define V_HISTERESIS        50 // [mV]

static bool _init_needed = true;

static bool _charging = false;
static bool _batt_connected = false;
static bool _power_ok = false;
static bool _batt_ok = false;
static bool _test_done = false;

static os_timer_t charging_done_tm = 0;
static os_timer_t bat_connect_tm = 0;

static void _charging_on(void)
{
    if (_charging)
        return;

    if (os_timer_get() < charging_done_tm + 10 * OS_TIMER_SECOND)
        return; // it may be some error (too fast battery voltage drop)

    HW_PWR_BAT_CHARGE_ON;
    _charging = true;
    LOG_DEBUGL(3, "charging ON");
}

static void _charging_off(void)
{
    if (! _charging)
        return;

    HW_PWR_BAT_CHARGE_OFF;
    _charging = false;
    bat_connect_tm = os_timer_get() + 10 * OS_TIMER_SECOND;
    LOG_DEBUGL(3, "charging OFF");
}

static void _bat_cconnect(void)
{
    os_timer_t now = os_timer_get();

    if (now < bat_connect_tm)
        return;

    HW_PWR_BAT_ENA_ON;
    _batt_connected = true;
    bat_connect_tm = now;
    LOG_DEBUGL(3, "BAT connect");
}

static void _bat_discconnect(void)
{
    HW_PWR_BAT_ENA_OFF;
    _batt_connected = false;
    LOG_DEBUGL(3, "BAT disconnected");
}

static void _bat_set_state(bool state)
{
    if (_test_done == false)
    {   // first test after power on
        if (state == false)
            event_create(EVENT_ID_FAULT, EVENT_SOURCE_BATTERY);
        _test_done = true;
    }
    else
    {
        if (_batt_ok == state)
            return; // no change
        event_create(state ? EVENT_ID_FAULT_RECOVERY : EVENT_ID_FAULT, EVENT_SOURCE_BATTERY);
    }
    _batt_ok = state;
}

static void _batt_test(void)
{
#define  BATT_TEST_TM (2 * 24 * 60 * OS_TIMER_MINUTE)

    static os_timer_t tm = 5 * OS_TIMER_MINUTE; // BATT_TEST_TM;
    os_timer_t now = os_timer_get();
    u32 voltage;
    u32 voltage_ref;

    if (! _power_ok)
    {
        tm = now + BATT_TEST_TM;
        return;
    }

    if (now < tm)
        return;

    tm = now + BATT_TEST_TM;

    voltage = analog_batt_mv();
    
    if (voltage < V_BATT_OK)
    {
        _bat_set_state(false);
        return;
    }

    LOG_DEBUG("bat test");

    _charging_off();
    OS_DELAY(100);

    voltage_ref = analog_batt_mv();

    if (voltage_ref < V_BATT_OK)
    {
        _bat_set_state(false);
        LOG_ERROR("bat v=%d", voltage_ref);
        return;
    }

    HW_PWR_BAT_TEST_ON;
    
    for (int i=0; i<10; i++)
    {
        OS_DELAY(20);
        voltage = analog_batt_mv();

        if (voltage < voltage_ref - V_BATT_DROP)
        {
            HW_PWR_BAT_TEST_OFF;
            _bat_set_state(false);
            LOG_ERROR("T%d, d=%d", i, voltage_ref - voltage);
            return;
        }
    }
    HW_PWR_BAT_TEST_OFF;
    LOG_DEBUG("bat test OK, d=%d", voltage_ref - voltage);
    _bat_set_state(true);
}


bool power_init (void)
{
    _init_needed = true;

    HW_PWR_BAT_ENA_OFF;
    HW_PWR_BAT_ENA_INIT;

    HW_PWR_BAT_CHARGE_OFF;
    HW_PWR_BAT_CHARGE_INIT;

    HW_PWR_BAT_TEST_OFF;
    HW_PWR_BAT_TEST_INIT;
    return (true);
}

void power_task (void)
{
#define _TASK_PERIOD (100 * OS_TIMER_MS)
    static os_timer_t tm = 1 * OS_TIMER_SECOND;
    static u32 filter = 0;
    static bool power_status;

    os_timer_t now = os_timer_get();
    u32 voltage_batt;
    u32 voltage_main;

    if (tm > now)
        return;

    tm = now + _TASK_PERIOD;
    
    voltage_batt = analog_batt_mv();
    voltage_main = analog_main_mv();

    if (_init_needed)
    {
        if ((voltage_batt < V_BATT_CONNECTED)
         && (voltage_main < V_VCC_LOST))
        {
            LOG_DEBUG("init delay, u=%d mV", voltage_main);
            tm = now + 10 * OS_TIMER_SECOND;
            return;
        }
        // HW_PWR_BAT_ENA_ON;
        // HW_PWR_BAT_CHARGE_ON;

        _init_needed = false;
        LOG_DEBUG("init done");
        return;
    }

    if (_batt_connected)
    {
        if (_charging)
        {
            if ((voltage_main < V_VCC_CHARGE_EN)
             || (voltage_batt > V_BATT_FULL))
            {   // stop _charging
                LOG_DEBUG("V=%d", voltage_batt);
                if (now < charging_done_tm + (10 + 5) * OS_TIMER_SECOND)
                {
                    LOG_ERROR("BAT charging failed");
                    _bat_discconnect();
                }
                _charging_off();
                charging_done_tm = now;
            }
        }
        else
        {   // not _charging
            if ((voltage_main > V_VCC_CHARGE_EN + V_HISTERESIS)
             && (voltage_batt < V_BATT_FULL - V_HISTERESIS))
            {   // ok, start _charging
                _charging_on();
            }
            else
            {
                if ((voltage_batt < V_BATT_EMPTY)
                  && (! _power_ok))
                {
                    LOG_WARNING("BAT empty");
                    _bat_discconnect();
                }
            }
        }
        _batt_test();
    }
    else
    {
        if ((voltage_batt > V_BATT_CONNECTED + V_HISTERESIS)
          && (_power_ok))
        {
            _bat_cconnect();
            // _batt_ok = true; // TODO: keep false until test ?
        }
    }

    if (_power_ok)
    {
        if (voltage_main < V_VCC_LOST)
        {
            io_enable(false);
            LOG_WARNING("vcc fail");
            _power_ok = false;
        }
    }
    else
    {
        if (voltage_main > V_VCC_LOST + V_HISTERESIS)
        {
            io_enable(true);
            LOG_INFO("vcc restored");
            _power_ok = true;
        }
    }

    // state change report with time filter
    if (power_status == _power_ok)
    {
        filter = 0;
    }
    else
    {
        if (_power_ok)
        {   // power restored, waiting with report
            if (++filter >= 60*10)
            {
                event_create(EVENT_ID_POWER_RECOVERY, EVENT_SOURCE_UNIT);
                power_status = _power_ok;
            }
        }
        else
        {
            if (++filter >= 10)
            {
                 event_create(EVENT_ID_POWER_FAIL, EVENT_SOURCE_UNIT);
                power_status = _power_ok;
            }
        }
    }
}

bool power_batt_ok(void)
{
    if (! _test_done)
    {   // not yet tested, keep as OK if connected
        if (_batt_connected)
            return (true);
    }
    return (_batt_ok);
}


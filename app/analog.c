#include "common.h"
#include "analog.h"
#include "adc.h"
#include "hardware.h"

#include "stm32g4xx_ll_adc.h"

bool analog_init (void)
{
    if (! adc_init())
        return (false);

    return (true);
}


void analog_task(void)
{
#define TASK_PERIOD (5 * OS_TIMER_MS)

    static os_timer_t tm = 0;
    os_timer_t now = os_timer_get();

    if (now < tm)
        return;

    tm = now + TASK_PERIOD;

    adc_task();
}

static u32 _fix_value(s32 value)
{
    // possible negative value due to calibration 
    if (value < 0)
        return (0);

    return (value);
}

u32 analog_main_mv(void)
{
    return (_fix_value(adc_get_value(ADC_CH_MAIN))); // [mv]
}

float analog_main_v(void)
{
    float v = analog_main_mv();
    return (v / 1000);
}

u32 analog_batt_mv(void)
{
    return (_fix_value(adc_get_value(ADC_CH_BATT))); // [mv]
}

float analog_batt_v(void)
{
    float v = analog_batt_mv();
    return (v / 1000);
}

float analog_temp_c(void)
{
    float v = adc_get_value(ADC_CH_TEMP)*4096/TEMPSENSOR_CAL_VREFANALOG;
    v /= 10; // mam to ulozene x10 kvuli presnosti
    v = (80.f*(v-*TEMPSENSOR_CAL1_ADDR)/(*TEMPSENSOR_CAL2_ADDR-*TEMPSENSOR_CAL1_ADDR))+30;
    return (v);
}

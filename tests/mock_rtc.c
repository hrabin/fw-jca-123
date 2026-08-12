// Shared host mock for rtc_get_time() — deterministic timestamps.
#include "rtc.h"

void rtc_get_time(rtc_t *time)
{
    // fixed current time for deterministic test output
    time->second = 56;
    time->minute = 34;
    time->hour   = 12;
    time->day    = 12;
    time->month  = 8;
    time->year   = 26;
}

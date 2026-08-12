// Stub for host-side testing — rtc_t is just a time struct.
#ifndef RTC_TYPE_H
#define RTC_TYPE_H

#include "type.h"

typedef struct {
	u8 second;
	u8 minute;
	u8 hour;
	u8 day;
	u8 month;
	u8 year;
} __attribute__((packed)) rtc_t;

#endif

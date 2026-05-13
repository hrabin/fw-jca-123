#ifndef POWER_H
#define	POWER_H

#include "type.h"

typedef enum {
	POWER_OK   = 0,
	POWER_LOW  = 1,
	POWER_FAIL = 2
} power_status_e;

bool power_init(void);
bool power_status(void);
bool power_bat_status(void);
void power_task(void);
bool power_batt_ok(void);

#endif // POWER_H


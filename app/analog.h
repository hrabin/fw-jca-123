
#ifndef ANALOG_H
#define ANALOG_H

#include "type.h"

bool analog_init (void);
void analog_task(void);

u32 analog_main_mv(void);
float analog_main_v(void);

u32 analog_batt_mv(void);
float analog_batt_v(void);

float analog_temp_c(void);


#endif // ! ANALOG_H


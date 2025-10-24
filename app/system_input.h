#ifndef SYSTEM_INPUT_H
#define SYSTEM_INPUT_H

#include "type.h"
#include "event.h"
#include "alarm.h"
#include "io.h"

#define SYSTEM_INP_COUNT (IO_INPUT_SIZE)

typedef struct {
    u8 nr; // input number 
    alarm_reaction_e reaction;
    u8 postpone;
    u8 tmr;
    u8 cnt; // count number of alarms
} system_input_t;

typedef u8 system_input_id_t;

void system_input_reset(void);
void system_input_init(void);
event_source_e system_input_source(system_input_id_t n);
system_input_t *system_input_get(system_input_id_t n);
bool system_input_alarm(system_input_t *inp);
void system_input_tick(void);

#endif // ! SYSTEM_INPUT_H


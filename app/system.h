#ifndef SYSTEM_H
#define SYSTEM_H

#include "type.h"
#include "io.h"
#include "alarm.h"
#include "section.h"

#define SYSTEM_INP_COUNT (IO_INPUT_SIZE)

typedef struct {
    u8 nr; // input number 
    alarm_reaction_e reaction:8;
    u8 postpone;
    u8 tmr;
    u8 cnt; // count number of alarms
} system_input_t;

#define SYSTEM_IO_INP1    BIT(0)
#define SYSTEM_IO_INP2    BIT(1)
#define SYSTEM_IO_DOOR    BIT(2)
#define SYSTEM_IO_KEY     BIT(3)
#define SYSTEM_IO_PANIC   BIT(4)
#define SYSTEM_IO_TRACING BIT(15)
#define SYSTEM_IO_TRACK   BIT(16)
extern u32 system_io_state;

#define SYSTEM_INT_POWER_FAIL BIT(0)
#define SYSTEM_INT_BATT_LOW   BIT(1)

extern u32 system_int_state;

bool system_init(void);
void system_inp_activated(unsigned int n);
void system_inp_deactivated(unsigned int n);
void system_task(void);
void system_tick(void);

section_state_t system_state(void);
void system_set(void);
void system_unset(void);

#endif // ! SYSTEM_H


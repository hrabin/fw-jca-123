#ifndef SYSTEM_H
#define SYSTEM_H

#include "type.h"
#include "alarm.h"
#include "system_input.h"
#include "out.h"
#include "section.h"

#define SYSTEM_INP_COUNT (INP_SIZE)
#define SYSTEM_OUT_COUNT (OUT_SIZE)

#define SYSTEM_IO_INP1    BIT(0)
#define SYSTEM_IO_INP2    BIT(1)
#define SYSTEM_IO_DOOR    BIT(2)
#define SYSTEM_IO_KEY     BIT(3)
#define SYSTEM_IO_PANIC   BIT(4)
#define SYSTEM_IO_SHOCK   BIT(5)
#define SYSTEM_IO_TRACING BIT(15)
#define SYSTEM_IO_TRACK   BIT(16)
#define SYSTEM_IO_LOCK    BIT(17)
#define SYSTEM_IO_UNLOCK  BIT(18)
#define SYSTEM_IO_SIREN   BIT(19)

extern u32 system_io_state;

#define SYSTEM_INT_POWER_FAIL BIT(0)
#define SYSTEM_INT_BATT_LOW   BIT(1)
#define SYSTEM_INT_BATT_FAIL  BIT(2)
extern u32 system_int_state;

bool system_init(void);
void system_inp_activated(unsigned int n);
void system_inp_deactivated(unsigned int n);
void system_inp_signal(unsigned int n, alarm_reaction_e signal);
void system_io_activate(u32 bit);
void system_io_deactivate(u32 bit);
void system_int_state_update(u32 bit, bool status);
section_state_t system_state(void);
void system_set(access_t *access);
void system_unset(access_t *access);
void system_event(event_id_e e);

void system_tick(void);
void system_task(void);

#endif // ! SYSTEM_H


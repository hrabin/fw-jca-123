#ifndef SECTION_H
#define SECTION_H

#include "type.h"
#include "access.h"
#include "system_input.h"
#include "alarm.h"


typedef enum {
    SECTION_ST_UNKNOWN  = 0,  // error state
    SECTION_ST_UNSET    = 1,  // 
    SECTION_ST_SET_PART = 2,  // partially set, some inputs inactive
    SECTION_ST_SET      = 3,  // 
    SECTION_ST_SERVICE  = 4,  // 

    SECTION_ST_SIZE
} section_state_t;

typedef struct {
    u16            num; // this section number 
    u16            tm_out;
    u16            tm_in;
} section_setup_t;

#define SECTION_FLAG_ERROR  BIT(0)
#define SECTION_FLAG_TM_OUT BIT(1)
#define SECTION_FLAG_TM_IN  BIT(2)
#define SECTION_FLAG_ALARM  BIT(3)

typedef struct {
    os_timer_t        state_time;
    os_timer_t        timer;
    section_state_t   state;
    u32 flags;
    system_input_t    *alarm_inp;
} section_data_t;

// internal section state structure
typedef struct {
    section_data_t data;
    section_setup_t setup;
} section_t;

void section_init(section_t *s);
bool section_alarm_enabled(section_t *s, alarm_reaction_e signal);
void section_signal(section_t *s, system_input_t *inp, alarm_reaction_e signal);
bool section_set(section_t *s, access_t *access);
bool section_unset(section_t *s, access_t *access);
void section_task(section_t *s);

#endif // ! SECTION_H


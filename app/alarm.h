#ifndef ALARM_H
#define ALARM_H

#include "type.h"
#include "event.h"

enum {
    ALARM_IDLE     = 0,  // 
    ALARM_ACTIVE   = 1,  // 
    ALARM_TIMEOUT  = 2,  // 
};
typedef u8 alarm_state_e;

enum {
    ALARM_REACTION_NONE    = 0,  // 
    ALARM_REACTION_DELAY   = 1,  // 
    ALARM_REACTION_INSTANT = 2,  // 
    ALARM_REACTION_24H     = 3,  // 
};
typedef u8 alarm_reaction_e;


#define ALARM_MAX_CNT (3)
/*typedef struct {
    alarm_reaction_e reaction;
    u8 cnt; // count number of alarms
} alarm_input_t;
*/

bool alarm_init(void);
void alarm_tick(void);

alarm_state_e alarm_state(void);
void alarm_trigger(void);
void alarm_stop(event_source_e source);

#endif // ! ALARM_H


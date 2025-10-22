#ifndef SECTION_H
#define SECTION_H

#include "type.h"


typedef enum {
    SECTION_ST_UNKNOWN  = 0,  // error state
    SECTION_ST_UNSET    = 1,  // 
    SECTION_ST_SET_PART = 2,  // partially set, some inputs inactive
    SECTION_ST_SET      = 3,  // 
} section_state_t;

typedef struct {
	u16            tm_out;
	u16            tm_in;
} section_setup_t;

#define SECTION_FLAG_ERROR  BIT(0)
#define SECTION_FLAG_TM_OUT BIT(1)
#define SECTION_FLAG_TM_IN  BIT(2)


// internal section state structure
typedef struct {
    section_state_t state;
    u32             flags;
    section_setup_t setup;
} section_t;

void section_init(section_t *s);
bool section_set(section_t *s);
bool section_unset(section_t *s);
void section_task(section_t *s);

#endif // ! SECTION_H


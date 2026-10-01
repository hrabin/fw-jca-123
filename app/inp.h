#ifndef INP_H
#define INP_H

#include "type.h"

typedef enum {
    INP_KEY,
    INP_LOCK,
    INP_UNLOCK,
    INP_DOOR,
    INP_INP1,
    INP_SHOCK,
    INP_SIZE // size limit
#define INP_NONE INP_SIZE
} inp_id_t;

#define    INP_KEY_MASK    (1 << INP_KEY)
#define    INP_LOCK_MASK   (1 << INP_LOCK)
#define    INP_UNLOCK_MASK (1 << INP_UNLOCK)
#define    INP_DOOR_MASK   (1 << INP_DOOR)
#define    INP_INP1_MASK   (1 << INP_INP1)
#define    INP_SHOCK_MASK  (1 << INP_SHOCK)

bool inp_init(void);
const ascii *inp_name(int pin);
u32 inp_state(void);
u32 inp_get(void);
void inp_enable(bool state);
void inp_set_bypass(u8 inp, bool state);
void inp_task(void);

#endif // ! INP_H

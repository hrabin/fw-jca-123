#ifndef OUT_H
#define OUT_H

#include "type.h"

typedef enum {
    OUT_LOCK,
    OUT_UNLOCK,
    OUT_SIREN,
    OUT_SIZE // size limit
} out_id_t;

bool out_init(void);
const ascii *out_name(int pin);
u32 out_state(void);
void out_set(u8 pin, bool state);

#endif // ! OUT_H

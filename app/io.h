#ifndef IO_H
#define IO_H

#include "type.h"

typedef enum { 
    IO_KEY,
    IO_LOCK_IN,
    IO_UNLOCK_IN,
    IO_DOOR,
    IO_INP1,
    IO_SHOCK,
    IO_INPUT_SIZE // size limit
#define IO_INPUT_NONE IO_INPUT_SIZE
} io_input_id_t;

#define    IO_KEY_MASK       (1 << IO_KEY)
#define    IO_LOCK_IN_MASK   (1 << IO_LOCK_IN)
#define    IO_UNLOCK_IN_MASK (1 << IO_UNLOCK_IN)
#define    IO_DOOR_MASK      (1 << IO_DOOR)
#define    IO_INP1_MASK      (1 << IO_INP1)
#define    IO_SHOCK_MASK     (1 << IO_SHOCK)

typedef enum {
    IO_LOCK_OUT,
    IO_UNLOCK_OUT,
    IO_SIREN,
    IO_OUTPUT_SIZE // size limit
} io_output_id_t;

bool io_init(void);
const ascii *io_inp_name(int pin);
const ascii *io_out_name(int pin);
u32 io_inp_state(void);
u32 io_out_state(void);
u32 io_get_inp(void);
void io_set_out(u8 pin, bool state);
void io_enable(bool state);
void io_inp_set_bypass(u8 inp, bool state);
void io_task(void);

#endif // ! IO_H


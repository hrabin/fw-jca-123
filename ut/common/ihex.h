#ifndef IHEX_H
#define	IHEX_H

#include "common.h"

typedef bool (*ihex_parse_callback_t) (u32 addr, u8 *data, int len);

void ihex_init(ihex_parse_callback_t parser);
void ihex_reset(void);
void ihex_flush(u32 addr, u8 *data, int len);
bool ihex_parse (char *src);

#endif // ! IHEX_H


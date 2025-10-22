#ifndef CFG_H
#define CFG_H

#include "type.h"
#include "buf.h"

#define CFG_ITEM_SIZE 62

typedef u16 cfg_id_t;

#include "cfg_table.h"

bool cfg_init(void);
bool cfg_write(cfg_id_t id, buf_t *src, access_auth_t auth);
bool cfg_read(buf_t *dest, cfg_id_t id, access_auth_t auth);
s32 cfg_read_nparam(cfg_id_t id, int n);

#endif // ! CFG_H

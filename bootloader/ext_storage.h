#ifndef EXT_STORAGE_H
#define	EXT_STORAGE_H

#include "common.h"
#include "container.h"

#define	EXT_STORAGE_SECTOR_SIZE (512)

bool ext_storage_init (void);

bool ext_storage_check_fw (container_hdr_t *container);

bool ext_storage_read_fw (u8 *dest, int len, u32 offset);

bool ext_storage_write_data(u32 addr, u8 *src, u32 len);

void ext_storage_flush_cache(void);

#endif // ! EXT_STORAGE_H


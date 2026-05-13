#ifndef STORAGE_H
#define	STORAGE_H

#include "common.h"

#define	STORAGE_SECTOR_SIZE (512)

#define STORAGE_CFG_SPACE          (128*1024) // cfg.c
#define STORAGE_GPS_RECORDS_SPACE (1024*1024) // gps_buffer.c (32768 x 32B)
#define STORAGE_EVENT_MEM_SPACE    (512*1024) // event_memory.c

bool storage_init (void);

bool storage_write_fw(u32 offset, u8 *src, u32 len);

bool storage_write_cfg(u32 offset, u8 *src, u32 len);
bool storage_read_cfg(u8 *dest, u32 offset, u32 len);

bool storage_save_gps_data(u32 offset, u8 *src, u32 len);
bool storage_get_gps_data(u8 *dest, u32 offset, u32 len);

bool storage_write_event(u32 offset, u8 *src, u32 len);
bool storage_read_event(u8 *dest, u32 offset, u32 len);

void storage_flush_cache(void);
void storage_maintenance(void);

#endif // ! STORAGE_H


#ifndef STORAGE_H
#define	STORAGE_H

#include "common.h"

#define	STORAGE_SECTOR_SIZE (512)

#define STORAGE_GPS_RECORD_SIZE (32)
#define STORAGE_GPS_RECORDS   32768 //  must be 2^N

bool storage_init (void);

bool storage_write_fw(u32 offset, u8 *src, u32 len);

bool storage_write_cfg(u32 offset, u8 *src, u32 len);
bool storage_read_cfg(u8 *dest, u32 offset, u32 len);

bool storage_save_gps_data(u32 offset, u8 *src, u32 len);
bool storage_get_gps_data(u8 *dest, u32 offset, u32 len);


void storage_flush_cache(void);
void storage_maintenance(void);

#endif // ! STORAGE_H


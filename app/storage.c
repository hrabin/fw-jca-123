#include "common.h"

#include "storage.h"
#include "flash_lib.h"

#include "log.h"

LOG_DEF("STOR");

#define SECTOR_SIZE  STORAGE_SECTOR_SIZE

#define	_CFG_ADDR (0)
#define	_CFG_SIZE (128*1024)

#define	_GPS_ADDR (_CFG_ADDR + _CFG_SIZE)
#define	_GPS_SIZE (STORAGE_GPS_RECORDS * STORAGE_GPS_RECORD_SIZE)

#define _LAST_ADDR (_GPS_ADDR + _GPS_SIZE)


#define	_FW_MAX_SIZE (256*1024UL)
#define	_FW_ADDR (4096*1024UL - _FW_MAX_SIZE)
#define	_FW_START_SECTOR (_FW_ADDR/SECTOR_SIZE)

#if _LAST_ADDR > _FW_ADDR
  #error "size configuration mismatch"
#endif // _LAST_ADDR > _FW_ADDR

static bool _storage_ready = false;

#if defined HW_FLASH_SPI_MUTEX
	HW_FLASH_SPI_MUTEX_DEF;
#endif // defined HW_FLASH_SPI_MUTEX

static void _storage_lock(void)
{
#if defined HW_FLASH_SPI_MUTEX
	OS_MUTEX_LOCK(HW_FLASH_SPI_MUTEX);
#endif // defined HW_FLASH_SPI_MUTEX
}

static void _storage_unlock(void)
{
#if defined HW_FLASH_SPI_MUTEX
	OS_MUTEX_UNLOCK(HW_FLASH_SPI_MUTEX);
#endif // defined HW_FLASH_SPI_MUTEX
}

bool storage_init (void)
{
	bool result;

	result = flash_init();

	_storage_ready = result;
	return (result);
}

bool storage_write_fw(u32 offset, u8 *src, u32 len)
{
	bool result;
	if (! _storage_ready)
		return (false);

	if ((offset + len) > _FW_MAX_SIZE)
		return (false);

	_storage_lock();
	result = flash_write_data (_FW_ADDR + offset, src, len);
	_storage_unlock();
	return (result);
}

bool storage_write_cfg(u32 offset, u8 *src, u32 len)
{
	bool result;
	if (! _storage_ready)
		return (false);

	if ((offset + len) > _CFG_SIZE)
		return (false);

	_storage_lock();
	result = flash_write_data (_CFG_ADDR + offset, src, len);
	_storage_unlock();
	return (result);
}

bool storage_read_cfg(u8 *dest, u32 offset, u32 len)
{
	bool result;
	if (! _storage_ready)
		return (false);

	if ((offset + len) > _CFG_SIZE)
		return (false);

	_storage_lock();
	result = flash_read_data(dest, _CFG_ADDR + offset, len);
	_storage_unlock();
	return (result);
}

bool storage_save_gps_data(u32 offset, u8 *src, u32 len)
{
	bool result;

	if ((offset + len) > _GPS_SIZE)
		return (false);

	_storage_lock();
	result = flash_write_data (_GPS_ADDR + offset, src, len);
	_storage_unlock();
	return (result);
}

bool storage_get_gps_data(u8 *dest, u32 offset, u32 len)
{
	bool result;

	if ((offset + len) > _GPS_SIZE)
		return (false);

	_storage_lock();
	result = flash_read_data(dest, _GPS_ADDR + offset, len);
	_storage_unlock();
	return (result);
}

void storage_flush_cache(void)
{
	flash_flush_cache();
}

void storage_maintenance(void)
{
	flash_maintenace_task();
}



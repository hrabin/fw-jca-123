#include "common.h"

// #include "aes.h"
#include "ext_storage.h"
#include "wd.h"
#include "app_info.h"
#include "container.h"
#include "chsum.h"

#include "flash_lib.h"

#include "log.h"

LOG_DEF("EXT");

#define SECTOR_SIZE  EXT_STORAGE_SECTOR_SIZE

#define	_FW_MAX_SIZE (256*1024UL)
#define	_FW_ADDR (4096*1024UL - _FW_MAX_SIZE)
#define	_FW_START_SECTOR (_FW_ADDR/SECTOR_SIZE)

static bool _storage_ready = false;

static u8 buf[SECTOR_SIZE];

/*
static aes_context aes_ctx;
static u8 aes_iv[16];
const u8 aes_firmware_key[16]="Togimevemorocusr";
*/
#warning "TODO: add AES support" // see ut/app_container/main.c


static container_hdr_t *fw_container = NULL;

static bool _sector_read (u8 *dest, u32 sector)
{
	return (flash_read_data(dest, sector * SECTOR_SIZE, SECTOR_SIZE)); 
}

bool ext_storage_init (void)
{
	bool result;

	OS_PRINTF("INIT FLASH device ... ");
	result = flash_init();

	if (result)
		OS_PRINTF("OK" NL);
	else
		OS_PRINTF("ERROR" NL);
	
	_storage_ready = result;
	OS_PRINTF("FLASH FW space 0x%lx to 0x%lx" NL, _FW_ADDR, _FW_ADDR+_FW_MAX_SIZE);
	return (result);
}

bool ext_storage_check_fw (container_hdr_t *container)
{
	u32 fw_chsum = CHSUM32_START_VALUE;
    u32 s;
	int l;

	if (! _storage_ready)
		return (false);

	wd_feed();

	s = _FW_START_SECTOR;

	LOG_INFO("CHECK FIRMWARE" NL);

// 	memset( aes_iv , 0, 16 );
// 	aes_setkey_dec(&aes_ctx, aes_firmware_key, 128);

	if (! _sector_read(buf, s))
	{
		LOG_ERROR("cant read sector %ld", s);
		return (false);
	}
	
	if (! container_valid(buf, SECTOR_SIZE))
	{
		LOG_ERROR("bad container");
		LOG_DUMP("D", buf, 64);
		return (false);
	}
	memcpy((u8 *)container, buf, sizeof(container_hdr_t));

	if (! container_compatible(container))
	{
		LOG_ERROR("incompatible FW");
		return (false);
	}

	if ((container->data_size < APP_MIN_SIZE) || (container->data_size > APP_MAX_SIZE))
	{
		LOG_ERROR("FW size: %ld", container->data_size);
		return (false);
	}

	OS_PRINTF("EXT FW size: %ld" NL, container->data_size);

	l = container->data_size;
	while (l>0)
	{
		s++;
		if (! _sector_read(buf, s))
		{
			LOG_ERROR("cant read sector %ld", s);
			return (false);
		}
		fw_chsum = chsum32(buf, l>SECTOR_SIZE ? SECTOR_SIZE : l, fw_chsum);
		l -= SECTOR_SIZE;
	}
	if (fw_chsum == container->data_chsum)
	{
		fw_container = container;
		return (true);
	}

	LOG_ERROR("FW chsum failed (%lx != %lx)", fw_chsum, container->data_chsum);
	return (false);
}

bool ext_storage_read_fw (u8 *dest, int len, u32 offset)
{
	u32 s;

	if ((len != SECTOR_SIZE) || (offset % SECTOR_SIZE))
	{
		LOG_ERROR("sector align failed");
		return (false);
	}
	ASSERT(fw_container != NULL, "fw_container == NULL");

	offset += SECTOR_SIZE; // first sector is container
	s = ((_FW_ADDR+offset)/SECTOR_SIZE);
	
	if (! _sector_read(buf, s))
		return (false);

/*	if (fw_container->aes)
	{
		memset( aes_iv , 0, 16 );
		aes_crypt_cbc( &aes_ctx, AES_DECRYPT, SECTOR_SIZE, aes_iv, buf, dest);
	}
	else */
	{
		memcpy(dest, buf, SECTOR_SIZE);
	}

	return (true);
}

bool ext_storage_write_data(u32 addr, u8 *src, u32 len)
{
	if (! _storage_ready)
		return (false);

	if ((addr < _FW_ADDR) || (addr+len > _FW_ADDR+_FW_MAX_SIZE))
	{
		LOG_ERROR("Out of allowed FLASH space");
		return (false);
	}

	return (flash_write_data (addr, src, len));
}

void ext_storage_flush_cache(void)
{
	flash_flush_cache();
}

#include "os.h"
#include "gps.h"
#include "gps_buffer.h"
#include "storage.h"
#include "log.h"
#include "log_select.h"

LOG_DEF("gpsbuf");

#define	BUF_STORE_LIMIT STORAGE_GPS_RECORDS

#if ((BUF_STORE_LIMIT & (BUF_STORE_LIMIT-1)) != 0)
  #error "BUF_STORE_LIMIT must be a power of 2."
#endif

#define	SAVE_GRID_SIZE STORAGE_GPS_RECORD_SIZE

#define	PTR_INVALID 0xFFFF
volatile u16 buf_read_ptr = 0;
volatile u16 buf_save_ptr = 0;
volatile u16 buf_last_undelivered = 0;


#define	BUF_INC_PTR(x) {if((x)++>=BUF_STORE_LIMIT)x=0;}

bool gps_buf_load (gps_buf_t *buf, u16 mem_id);


bool gps_buf_init (u16 *track, u32 *info)
{
	u16 i;
	u16 ptr;
	gps_buf_t *buf;

	OS_ASSERT(SAVE_GRID_SIZE>=sizeof(gps_buf_t), "SAVE_GRID_SIZE<sizeof(gps_buf_t)");

	*track = 0;
	*info  = 0;
	
	if ((buf = (gps_buf_t *)OS_MEM_ALLOC (sizeof(gps_buf_t))) == NULL)
		return (false);

	// search for last valid record 
	buf_last_undelivered = PTR_INVALID;
	buf_save_ptr         = PTR_INVALID;
	
	if (gps_buf_load(buf, 0))
	{	// there is some first record, lets find the last one
		buf_save_ptr = 0;

		i=(BUF_STORE_LIMIT>>1);
		ptr=i;
		*track = buf->track;

		// search for the last one record 
		while (i)
		{
			i>>=1;
			if ((gps_buf_load(buf, ptr))
			 && (buf->track >= *track))
			{
				*track = buf->track;
				buf_save_ptr = ptr;
				*info = buf->info;
				ptr+=i;
				if ((buf->flags & GPS_FLAG_DELIVERED) == 0)
					buf_last_undelivered = buf_save_ptr; // something undelivered
			}
			else
			{
				ptr-=i;
			}
		}
	
		// now search for last undelivered record 
		if (buf_last_undelivered != PTR_INVALID)
		{	// yes, there is something undelivered
			i=(BUF_STORE_LIMIT>>1);
			// halving the space
			ptr = (buf_save_ptr - i) & (BUF_STORE_LIMIT-1) ; // 

			while (i)
			{
				i>>=1;
				if ((gps_buf_load(buf, ptr))
				 && ((buf->flags & GPS_FLAG_DELIVERED) == 0)) // also undelivered
				{	// so further
					buf_last_undelivered = ptr;
					ptr-=i;
				}
				else
				{	// get closer to first one
					ptr+=i;
				}
				ptr&=(BUF_STORE_LIMIT-1);
			}
		}
	}

	if (buf_save_ptr == PTR_INVALID) // no any stored point ?
		buf_save_ptr = 0;
	else
		BUF_INC_PTR(buf_save_ptr);

	if (buf_last_undelivered == PTR_INVALID) // no any undelivered ?
		buf_last_undelivered = buf_save_ptr;

	buf_read_ptr = buf_last_undelivered;

	LOG_DEBUGL(LOG_SELECT_GPS_BUF, "init: rd=%d, wr=%d, t=%d", buf_read_ptr, buf_save_ptr, *track);

	OS_MEM_FREE(buf);
	return (true);
}

bool gps_buf_hard_erase (void)
{
	gps_buf_t *buf;
	u32 mem_id;

	LOG_INFO("erasing all");
	if ((buf = (gps_buf_t *)OS_MEM_ALLOC (sizeof(gps_buf_t))) == NULL)
		return (false);
	
	memset ((u8 *)buf, 0xFF,sizeof(gps_buf_t));
	for (mem_id=0; mem_id<BUF_STORE_LIMIT; mem_id++)
	{
		if (storage_save_gps_data (mem_id*SAVE_GRID_SIZE, (u8 *)buf, sizeof(gps_buf_t)) == false)
		{
			LOG_ERROR("FAILED at %" PRIu32 " !", mem_id);
			break;
		}
		// if ((mem_id&0xFF) == 0xFF)
		// 	OS_PRINTF(".");

	}
	
	buf_save_ptr = 0;
	buf_last_undelivered = buf_save_ptr;
	buf_read_ptr = buf_last_undelivered;
	return (true);
}

u8 gps_buf_checksum (gps_buf_t *buf)
{	// simple checksum to detect consistency
	u8 i;
	u8 chsum=0;

	for (i=0; i<(sizeof(gps_buf_t)-1); i++)
	{
		chsum += (((u8 *)buf)[i] ^ i) + BUF_VERSION;
	}
	return (chsum);
}

bool gps_buf_save (u16 mem_id, gps_buf_t *buf)
{
	LOG_DEBUGL(LOG_SELECT_GPS_BUF, "save %d", mem_id);
	buf->chsum = gps_buf_checksum(buf);
	if (storage_save_gps_data(mem_id*SAVE_GRID_SIZE, (u8 *)buf, sizeof(gps_buf_t)) == false)
		return (false);

	return (true);
}

bool gps_buf_load (gps_buf_t *buf, u16 mem_id)
{
 	LOG_DEBUGL(LOG_SELECT_GPS_BUF ,"load %d", mem_id);
	if (storage_get_gps_data ((u8 *)buf, mem_id*SAVE_GRID_SIZE, sizeof(gps_buf_t)) == false)
		return (false);
	if (buf->chsum != gps_buf_checksum(buf))
	{
		return (false);
	}
	return (true);
}

bool gps_buf_store_position (gps_stamp_t *pos, u16 track, u16 point, u8 flags, u32 info)
{
	gps_buf_t *buf;

	if ((buf = (gps_buf_t *)OS_MEM_ALLOC (sizeof(gps_buf_t))) == NULL)
		return (false);
// 	memset ((u8 *)buf, 0, sizeof(gps_buf_t));
	buf->point = point;
	buf->track = track;
	memcpy ((u8 *)&buf->position, (u8 *)pos, sizeof(gps_stamp_t));
	buf->flags = flags;
	buf->info  = info;

	gps_buf_save (buf_save_ptr, buf);

	BUF_INC_PTR (buf_save_ptr);
	OS_MEM_FREE (buf);
	return (true);
}

s16 gps_buf_read_next (gps_stamp_t *pos, u16 *track, u16 *point, u8 *flags, u32 *info)
{
	u16 result;
	gps_buf_t *buf;

	if (buf_save_ptr == buf_read_ptr)
		return (-1);

	if ((buf = (gps_buf_t *)OS_MEM_ALLOC (sizeof(gps_buf_t))) == NULL)
		return (-1);

	result = buf_read_ptr;
	gps_buf_load (buf, buf_read_ptr);
	memcpy ((u8 *)pos, (u8 *)&buf->position, sizeof(gps_stamp_t));
	*track = buf->track;
	*point = buf->point;
	*flags = buf->flags;
	if (info != NULL)
		*info = buf->info;

	BUF_INC_PTR(buf_read_ptr);

	OS_MEM_FREE (buf);
	return (result);
}

bool gps_buf_delivered_all (void)
{
	gps_buf_t *buf;

	if ((buf = (gps_buf_t *)OS_MEM_ALLOC (sizeof(gps_buf_t))) == NULL)
		return (false);
	
	// oznacit za dorucene vsechny dosud prectene zaznamy
	while (buf_last_undelivered != buf_read_ptr)
	{
		if (gps_buf_load (buf, buf_last_undelivered))
		{
			// OS_PRINTF ("[BD%d]", buf_last_undelivered);
			buf->flags |= GPS_FLAG_DELIVERED;
			gps_buf_save (buf_last_undelivered, buf);
		}
		BUF_INC_PTR(buf_last_undelivered);
	}
	OS_MEM_FREE (buf);
	return (true);
}

bool gps_buf_empty (void)
{
	if (buf_read_ptr == buf_save_ptr)
		return (true);
	return (false);
}

void gps_buf_test (void)
{
	os_timer_t timer;
	u16 i;
	gps_buf_t *buf;

	if ((buf = (gps_buf_t *)OS_MEM_ALLOC(sizeof(gps_buf_t))) == NULL)
		return;

	OS_PRINTF ("gps_buf_test()" NL);
	timer = os_timer_get();

	for (i=0; i<5000; i++)
	{
		gps_buf_load(buf, i);
	}

	timer = os_timer_get()-timer;
	OS_PRINTF ("TIME: %" PRId32 NL, (u32)timer);
	OS_MEM_FREE(buf);
}

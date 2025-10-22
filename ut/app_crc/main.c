/*
 * Author: jarda https://github.com/hrabin
 */

#include "common.h"
#include "app.h"
#include "ihex.h"

static u8 app_info_page[APP_INFO_PAGE_SIZE];
app_info_t *app_info = (app_info_t *)app_info_page;

static u32 crc = 0xFFFFFFFF;
static u32 app_size = 0;
static u32 info_size = 0;

u32 _app_crc_calc(u8 *data, int len) 
{
	int i;
	u32 b, mask;

	while (len--) 
	{
		b = *data++;

		crc = crc ^ b;
		for (i = 7; i >= 0; i--) 
		{   // Do eight times.
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}
	return ~crc;
}

static bool _flush_data(u32 addr, u8 *data, int len)
{
	if ((addr >= APP_INFO_PAGE_ADDR) && ((addr+len)<(APP_INFO_PAGE_ADDR+APP_INFO_PAGE_SIZE)))
	{	// skip info page
		return (true);
	}

	// flush all other data
	ihex_flush(addr, data, len);
	return (true);
}

static bool _compute_crc(u32 addr, u8 *data, int len)
{
	static u32 dest_addr = 0;

	// OS_PRINTF("0x%lx, l=%d" NL, addr, len);

	if ((addr >= APP_INFO_PAGE_ADDR) && ((addr+len)<(APP_INFO_PAGE_ADDR+APP_INFO_PAGE_SIZE)))
	{
		u32 a = addr - APP_INFO_PAGE_ADDR;
		// OS_PRINTF("INFO PAGE a=%x" NL, a);

		memcpy(app_info_page+a, data, len);

		if ((a+len) > info_size)
			info_size = a+len;

		return (true);
	}

	if ((addr < APP_START_ADDR) || ((addr+len) > (APP_START_ADDR + APP_MAX_SIZE)))
	{
		OS_FATAL("data out of range");
		return (false);
	}

	if ((app_info->device_id == 0xFFFF) || (info_size==0))
	{
		OS_FATAL("data without info page");
		return (false);
	}

	if (dest_addr == 0)
	{
		// OS_PRINTF("data begin !" NL);
	}
	else
	{
		if (addr != dest_addr)
		{
			OS_WARNING("data GAP %dB (0x%x .. 0x%x)", addr-dest_addr, dest_addr, addr);
			if (addr < dest_addr)
			{
				OS_FATAL("cant skip back");
			}
		}
		while (addr > dest_addr)
		{
			_app_crc_calc((u8 *)"\xFF", 1);
			dest_addr++;
		}
	}

	_app_crc_calc(data, len);

	dest_addr = addr+len;
	// app_size += len;
	app_size = dest_addr - APP_START_ADDR;

	return (true);
}

static void _rx_feed(char ch)
{
	static int line = 0;
    static char ihex_buf[256];
    static u32 rx_len = 0;

	if (ch == ':')
	{
		ihex_buf[0] = ch;
		rx_len=1;
		return;
	}

	if (rx_len >= sizeof(ihex_buf) || rx_len == 0)
		return;

	if ((ch == '\r') || (ch == '\n'))
		ch = '\0';

	ihex_buf[rx_len] = ch;

	if (ch == '\0')
	{
		if (rx_len>0)
		{
			line++;
			if (! ihex_parse(ihex_buf))
			{
				OS_ERROR("ihex line: %d" NL, line);
				// exit -1;
			}
		}
	
		rx_len = 0;
		return; 
	}
	rx_len++;
}

int main(int argc, char *argv[])
{
#define	MAX_FILE_SIZE (5 * 1024 * 1024)
	u8 data[MAX_FILE_SIZE];
	int i;
	int size=0;

	OS_PRINTFE("# Loading iHEX file ... ");
	
	while ((i = getchar()) != EOF)
	{	// read from stdin
		data[size] = i;
		size++;
		
		if (size >= MAX_FILE_SIZE)
		{
			OS_FATAL("size overflow");
		}
	}
	OS_PRINTFE("%dB DONE" NL, size);

	memset(app_info_page, 0xff, sizeof(app_info_page));
	
	ihex_init(_compute_crc);
	for (i=0; i<size; i++)
	{
		_rx_feed(data[i]);
	}

	app_info->size = app_size;
	app_info->crc = ~crc;

	OS_PRINTFE("# ------------------ " NL);
	OS_PRINTFE("# size           = %d" NL, app_info->size);
	OS_PRINTFE("# crc            = 0x%x" NL, app_info->crc);
	OS_PRINTFE("# device_id      = %d" NL, app_info->device_id);
	OS_PRINTFE("# hw_version_min = %d" NL, app_info->hw_version_min);
	OS_PRINTFE("# hw_version_max = %d" NL, app_info->hw_version_max);
	OS_PRINTFE("# ------------------ " NL);

	ihex_flush(APP_INFO_PAGE_ADDR, app_info_page, info_size);
	
	ihex_init(_flush_data);
	for (i=0; i<size; i++)
	{
		_rx_feed(data[i]);
	}
	OS_PRINTF(":00000001FF" NL);
	return (0);
}


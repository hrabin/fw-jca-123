/*
 * Author: jarda https://github.com/hrabin
 */

#include "common.h"
#include "aes.h"
#include "app.h"
#include "cfg.h"
#include "container.h"
#include "ihex.h"

const u8 aes_firmware_key[16+1]="Togimevemorocusr";

static u32 app_size = 0;
static u32 app_chsum;

static u8 fw_data[APP_MAX_SIZE];

#define CHSUM32_START_VALUE (0x6b13ff7d)

#define	 DEST_REMAP_ADDRESS (0x3c0000) // FLASH target address where to put FW

u32 chsum32 (u8 *data, u32 len, u32 start_value)
{
	int msb;

	while (len--)
	{
		msb = (start_value >> 31) & 1;
		start_value = (start_value<<1) + msb;
		start_value += *data;
		data++;
	}
	return (start_value);
}

static bool _load_bin(u32 addr, u8 *data, int len)
{
	static u32 dest_addr = 0;
	u32 a;

	if ((addr < APP_INFO_PAGE_ADDR) || ((addr+len) > (APP_START_ADDR + APP_MAX_SIZE)))
	{
		OS_FATAL("data out of range");
		return (false);
	}
	a = addr - APP_INFO_PAGE_ADDR;

	if (a != dest_addr)
	{
		OS_WARNING("data GAP %dB (0x%x .. 0x%x)", a-dest_addr, dest_addr, a);
		if (a < dest_addr)
		{
			OS_FATAL("cant skip back");
		}
	}
	while (a > dest_addr)
	{
		dest_addr++;
	}
	memcpy(fw_data + a, data, len);

	dest_addr = a+len;
	app_size = dest_addr;

	return (true);
}

static void _rx_feed(char ch)
{
    static char ihex_buf[256];
    static u32 rx_len = 0;

	if (ch == ':')
	{
		ihex_buf[0] = ch;
		rx_len=1;
		return;
	}

	if ((rx_len >= sizeof(ihex_buf)) || (rx_len == 0))
		return;

	if ((ch == '\r') || (ch == '\n'))
		ch = '\0';

	ihex_buf[rx_len++] = ch;

	if (ch == '\0')
	{
		ihex_parse(ihex_buf);
		rx_len = 0;
	}
}

void usage (ascii *filename)
{
	ascii *p = strrchr(filename, '/');
    
	if (p == NULL)
	p = filename;

	if (p[0] == '/')
		p++;

	OS_PRINTF( "\nusage: %s [options]\n", filename);
	
	cfg_print_help();

	OS_PRINTF( "\n");
}

int main(int argc, char *argv[])
{
	container_hdr_t container;
	app_info_t app_info;
	int ch;
	u32 i;
	
	if (! configure (argc, argv))
	{
		usage(argv[0]);
		exit (EXIT_FAILURE);
	}

	bzero(&container, sizeof(container));
	memset(fw_data, 0xFF, sizeof(fw_data));

	ihex_init(_load_bin);

	while ((ch = getchar()) != EOF)
	{	// get data from stdin
		_rx_feed(ch);
	}
	memcpy(&app_info, fw_data, sizeof(app_info));
	
	if (app_info.size + APP_INFO_PAGE_SIZE != app_size)
	{
		OS_FATAL("app size mismatch");
	}

	OS_PRINTFE("# app_info.size = %u" NL, app_info.size);
	OS_PRINTFE("# app_info.device_id = %u" NL, app_info.device_id);

	if (cfg.encrypt)
	{	// AES128 encryption
	#define	AES_PAGE_SIZE (512)

		aes_context aes_ctx;
		u8 aes_iv[16];
		u8 *p;

		// length paddng to 16
		app_size = (app_size+15) & ~15;

		aes_setkey_enc(&aes_ctx, aes_firmware_key, 128);

		p = fw_data;

		for (i=0; i<app_size; i+=AES_PAGE_SIZE)
		{
			int l = app_size - i;
			l = l > AES_PAGE_SIZE ? AES_PAGE_SIZE : l;

			memset( aes_iv , 0, 16 );
			aes_crypt_cbc( &aes_ctx, AES_ENCRYPT, l, aes_iv, p, p);
			p+=AES_PAGE_SIZE;
			// OS_PRINTFE("ENC l=%d", l);
		}
	
	}

	app_chsum = chsum32(fw_data, app_size, CHSUM32_START_VALUE);

	OS_PRINTFE("# DATA chsum=0x%x" NL, app_chsum);
	OS_PRINTFE("# DATA size=%d" NL, app_size);

	container.data_size = app_size;
	container.data_chsum = app_chsum;
	container.device_id = app_info.device_id;
	container.hw_version_min = app_info.hw_version_min;
	container.hw_version_max = app_info.hw_version_max;
	strncpy((ascii *)container.fw_name, cfg.fw_name, CONTAINER_FW_NAME_LEN);
	container.aes = cfg.encrypt;
	container.hdr_chsum = chsum32((u8 *)&container, sizeof(container)-4, CHSUM32_START_VALUE);

	if (cfg.hex)
	{
		ihex_flush(DEST_REMAP_ADDRESS, (u8 *)&container, sizeof(container));
		ihex_flush(DEST_REMAP_ADDRESS + sizeof(container), fw_data, app_size);
		OS_PRINTF(":00000001FF" NL);
	}
	else
	{	// binary output
		char *p;

		p = (char *)&container;
		for (i=0; i<sizeof(container); i++)
		{
			putchar(*p++);
		}
		p = (char *)fw_data;
		for (i=0; i<app_size; i++)
		{
			putchar(*p++);
		}
	}

	return (0);
}


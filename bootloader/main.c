/*
 * Author: jarda https://github.com/hrabin
 */

#include "common.h"
#include "main.h"

#include "app_info.h"
#include "buf.h"
#include "container.h"
#include "ext_storage.h"
#include "gpreg.h"
#include "hardware.h"
#include "hw_info.h"
#include "ihex.h"
#include "int_flash.h"
#include "reset.h"
#include "time.h"
#include "tty.h"
#include "wd.h"
#include "sys.h"
#include "log.h"

LOG_DEF("BL");

#if (MAIN_DEBUG == 1) 
  #warning "MAIN_DEBUG directive active"
#endif // MAIN_DEBUG

static u8 reset_type = RESET_POWER_ON;

#define BL_TIMEOUT (15 * 60 * 5)
static u32 bl_timeout = 0;

static void _tty_rx_parser(char *data)
{	// RX iHEX FW file
	if (data[0] == ':')
	{
		if (ihex_parse(data))
		{
			OS_PRINTF("OK" NL);
			bl_timeout = 0;
		}
		else
		{
			OS_PRINTF("ERROR" NL);
		}
	
		OS_FLUSH();
		return; 
	}

	switch (data[0])
	{
    case 'B':
        OS_PRINTF("go to BL" NL);
        GPREG_WRITE(GPREG_BOOT, GPREG_BOOT_STAY_IN_BOOT);
        GPREG_WRITE(GPREG_WDID, GPREG_WDID_REBOOT_RQ);
		wd_reset();
        break;

	case 'F': 
		ext_storage_flush_cache();
		OS_PRINTF("FLASH" NL);
		GPREG_WRITE(GPREG_BOOT, GPREG_BOOT_FLASH_RQ);
   		GPREG_WRITE(GPREG_WDID, GPREG_WDID_REBOOT_RQ);
		wd_reset();
		break;

	case 'T': 
		OS_PRINTF("TEST OK" NL);
		break;

	case 'R':
		ext_storage_flush_cache();
		OS_PRINTF("RESET" NL);
		wd_reset();
		break;

	case 'W':
		OS_PRINTF("WDR TEST" NL);
		while (1)
			;
	}
}

static void _bl_task_delay(u32 delay)
{
	utime_t time;
	delay*=TIMER_MS;
	time=timer_get_time();

	while ((utime_t)(timer_get_time()-time)<delay)
	{
        tty_rx_task();
	}
}

static void _bl_task(void)
{
	GPREG_WRITE(GPREG_BOOT, 0)

    while (1)
    {
        HW_LED_ON;
        _bl_task_delay(100);
        HW_LED_OFF;
        _bl_task_delay(100);

        wd_feed();

		if (++bl_timeout >= BL_TIMEOUT)
			break;
    }

    OS_PRINTF("TIMEOUT REBOOT");
	wd_reset();
}

void main_fatal_error(char *msg)
{
    OS_PRINTF("FATAL ERROR");

    if (msg != NULL)
    {
        OS_PRINTF(": %s", msg);
    }
    OS_PRINTF(NL);

	_bl_task();
}

static bool main_flash_app(void)
{
	container_hdr_t container;
	u32 buf[EXT_STORAGE_SECTOR_SIZE/4];
	u32 dest, i;

    if (! ext_storage_init())
    {
		LOG_ERROR("ext storage HW init failed !");
        return (false);
    }
    if (! ext_storage_check_fw(&container))
    {
		LOG_ERROR("ext storage firmware not found !");
        return (false);
    }
	OS_PRINTF("ext storage firmware OK" NL);

	dest = APP_INFO_PAGE_ADDR;
	for (i=0; i<container.data_size; i+=EXT_STORAGE_SECTOR_SIZE)
	{
		ext_storage_read_fw((u8 *)buf, sizeof(buf), i);

		int_flash_write(dest, buf, EXT_STORAGE_SECTOR_SIZE);
		dest += EXT_STORAGE_SECTOR_SIZE;
	}
	OS_PRINTF(NL);
    return (true);
}

static bool main_app_ok(void)
{   // check application CRC
	u8 *app_ptr;
	u32 crc;

	// LOG_DUMP("APP", (u8 *)&APP_INFO, sizeof(APP_INFO));
	if ((APP_INFO.size > APP_MAX_SIZE) || (APP_INFO.size < APP_MIN_SIZE))
	{
		LOG_ERROR("FW size mismatch");
		return (false);
	}
	
	app_ptr = (u8 *)APP_START_ADDR;
	crc = app_crc(app_ptr, APP_INFO.size);

	if (crc == APP_INFO.crc)
	{
		return (true);
	}

    LOG_ERROR("APP CHSUM FAILED (0x%lX != 0x%lX)" NL, crc, APP_INFO.crc);
    return (false);
}

typedef unsigned long (*pfunc_void)	(void);

static void _jump_to_app(void)
{	
	pfunc_void app;
	app=(pfunc_void)(*((u32*)(APP_START_ADDR+4)));
	__set_MSP(*(__IO uint32_t*) APP_START_ADDR);

	app();
}

int main (void) 
{
    bool app_ok = false;
    bool flash_rq = false;

	reset_type = reset_get_type();

    sys_init();
    sys_clock_config();
	sys_run();
    timer2_free_run();

    wd_init();
    wd_run();

    gpio_init();

    HW_LED_INIT;
    HW_LED_ON;

    tty_init(_tty_rx_parser);

    OS_PRINTF(NL);
    OS_PRINTF("BL START" NL);
    HW_LED_OFF;

    OS_PRINTF("# PLATFORM: " OS_PLATFORM_NAME NL);
    OS_PRINTF("# BL VERSION: 0.0.1" NL);
    OS_PRINTF("# BL BUILD DATE: " __DATE__ NL);

    OS_PRINTF("# DEVICE ID: %d" NL, HW_INFO.device_id);
    OS_PRINTF("# HW: %s" NL, HW_INFO.hw_name);
    OS_PRINTF("# SN: 0x%" PRIx32 NL, HW_INFO.addr);


    OS_PRINTF("# RESET TYPE: ");
    switch (reset_type)
    {
    case RESET_POWER_ON: OS_PRINTF("POWER_ON"); break;
    case RESET_USER_RQ:  OS_PRINTF("USER_RQ");  break;
    case RESET_WDT:      OS_PRINTF("WDT");      break;
    case RESET_BOD:      OS_PRINTF("BOD");      break;
    default:             OS_PRINTF("UNKNOWN");  break;
    }
    OS_PRINTF(NL);
    OS_PRINTF(NL);

    app_ok = main_app_ok();

    if (reset_type == RESET_USER_RQ)
    {
        OS_PRINTF("GPREG_BOOT = %lx" NL, GPREG_BOOT);
        if (GPREG_BOOT == GPREG_BOOT_FLASH_RQ)
        {
            OS_PRINTF("FLASH RQ CMD" NL);
            flash_rq = true;
        }
		else if (GPREG_BOOT == GPREG_BOOT_STAY_IN_BOOT)
		{
            OS_PRINTF("BOOT STAY" NL);
   			ext_storage_init();
			_bl_task();
		}
		GPREG_WRITE(GPREG_BOOT, 0)
    }

    if ((flash_rq) || (app_ok == false))
    {
        OS_PRINTF("FLASH REQUEST" NL);
        wd_feed();

        if (! main_flash_app())
        {
            LOG_ERROR("FLASH FAILED");
        }
        else
        {
            OS_PRINTF("FLASH SUCCESSFUL" NL);
        }
	    app_ok = main_app_ok();
    }

	GPREG_WRITE(GPREG_BOOTLOOP, GPREG_BOOTLOOP + 1);

	if (GPREG_BOOTLOOP > 5)
	{
        OS_PRINTF("BOOT LOOP" NL);
		GPREG_WRITE(GPREG_BOOTLOOP, 0);
   		ext_storage_init();
		_bl_task();
	}
	if (app_ok)
	{
        OS_DELAY(50);
    	OS_PRINTF("BL RUN APP" NL);
    	OS_PRINTF(NL);
        OS_DELAY(50);
 		_jump_to_app();
	}

    main_fatal_error("NO FIRMWARE AVAILABLE"); // endless loop

    // should never continue here
    while (1)
        ;
}


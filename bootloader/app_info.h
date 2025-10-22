#ifndef APP_INFO_H
#define	APP_INFO_H

#include "type.h"

#define	BL_SIZE (32*KB)
#define	APP_INFO_PAGE_SIZE 256

#define	APP_MAX_SIZE (512*KB - BL_SIZE - APP_INFO_PAGE_SIZE)
#define	APP_MIN_SIZE (1*KB)

#define	FLASH_START_ADDR (0x8000000)
#define	APP_INFO_PAGE_ADDR (FLASH_START_ADDR + BL_SIZE)
#define	APP_START_ADDR     (FLASH_START_ADDR + BL_SIZE + APP_INFO_PAGE_SIZE)

#if BOOTLOADER == 0
  // this is not bootloader project but application
  // define vectors offset
  #define APP_VTOR_ADDR APP_START_ADDR
#endif // 


typedef struct {

	u32  size;           // 
	u32  crc;            // 
	u16  device_id;      // 
	u16  hw_version_min; // 
	u16  hw_version_max; // 
	u16  res; // padding to 16B

} app_info_t;

extern const app_info_t APP_INFO;

u32 app_crc(u8 *data, int len);

#endif // ! APP_INFO_H


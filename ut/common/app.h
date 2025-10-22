#ifndef APP_H
#define	APP_H

#define	BL_SIZE (32*KB)
#define	APP_INFO_PAGE_SIZE 256

#define	APP_MAX_SIZE (512*KB - BL_SIZE - APP_INFO_PAGE_SIZE)
#define	APP_MIN_SIZE (1*KB)

#define	FLASH_START_ADDR (0x8000000)
#define	APP_INFO_PAGE_ADDR (FLASH_START_ADDR + BL_SIZE)
#define	APP_START_ADDR     (FLASH_START_ADDR + BL_SIZE + APP_INFO_PAGE_SIZE)


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

#endif // ! APP_H


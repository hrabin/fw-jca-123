#ifndef APP_INFO_H
#define APP_INFO_H

#include "type.h"

#define BL_SIZE (32*KB)

/* Reserved page between the bootloader and the application.
   
   APP_START_ADDR doubles as SCB->VTOR, and a Cortex-M requires the vector
   table base to be aligned to the table size rounded up to a power of two.
   This part has 118 exception vectors (472 bytes) -> 512 bytes.

   The APP_START_ADDR must be 512B aligned */

#define APP_INFO_PAGE_SIZE 512

#define APP_MAX_SIZE (512*KB - BL_SIZE - APP_INFO_PAGE_SIZE)
#define APP_MIN_SIZE (1*KB)

#define FLASH_START_ADDR (0x8000000)
#define APP_INFO_PAGE_ADDR (FLASH_START_ADDR + BL_SIZE)
#define APP_START_ADDR     (FLASH_START_ADDR + BL_SIZE + APP_INFO_PAGE_SIZE)

#if BOOTLOADER == 0
  // this is not bootloader project but application
  // define vectors offset
  #define APP_VTOR_ADDR APP_START_ADDR
#else
  // bootloader: its vector table is at the start of flash.  Set VTOR to the
  // real address instead of relying on the 0x00000000 boot-memory alias.
  #define APP_VTOR_ADDR FLASH_START_ADDR
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


#ifndef HARDWARE_H
#define HARDWARE_H

#include "platform_setup.h"

#ifndef HW_NAME

  // default HW name
  #define	HW_NAME "JCA12301"

#endif // not defined HW_NAME

#define HW_VERSION_CUR 1
#define HW_DEVICE_ID 1


// #include "pcb_nucleo.h"
// #include "pcb_stlinkv2.h"
// #include "pcb_stm32_devel_board.h"
#include "pcb_jca123-01.h"

#endif // ! HARDWARE_H


#ifndef _GPS_IO_H_
#define _GPS_IO_H_

#include	"type.h"

void gps_io_port_init(u32 baudrate);
void gps_io_init (void);
void gps_io_pwr (u8 state);
void gps_io_reset (u8 state);
bool gps_tx_char (u8 c);
bool gps_rx_char (u8 *c);


#endif	// _GPS_IO_H_

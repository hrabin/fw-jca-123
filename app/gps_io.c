#include	"os.h"
#include	"uart.h"
#include	"hardware.h"
#include 	"gps_io.h"

#if !defined(HW_GNSS_ON_INIT) && !defined(HW_GNSS_RST_INIT) 
  #warning "missing GNSS HW"
#endif

void gps_io_port_init(u32 baudrate)
{
#ifdef HW_GNSS_UART_INIT
	HW_GNSS_UART_INIT(baudrate);
#endif	// HW_GNSS_UART_INIT
}

void gps_io_init(void)
{
#ifdef HW_GNSS_ON_INIT
	HW_GNSS_ON_LOW;
	HW_GNSS_ON_INIT;
#endif	// HW_GNSS_ON_INIT
#ifdef HW_GNSS_RST_INIT
	HW_GNSS_RST_HI;
	HW_GNSS_RST_INIT;
#endif	// HW_GNSS_RST_INIT

	gps_io_port_init(9600);
}

void gps_io_pwr(u8 state)
{
	if (state == ON)
	{
#ifdef HW_GNSS_PWR_ON
    	HW_GNSS_PWR_ON;
		HW_GNSS_UART_WAKEUP;
#endif // HW_GNSS_PWR_ON
	} 
   	else
	{
#ifdef HW_GNSS_PWR_OFF
		HW_GNSS_UART_SLEEP;
    	HW_GNSS_PWR_OFF;
#endif // HW_GNSS_PWR_OFF
	}
}

void gps_io_reset(u8 state)
{	// in this HW we dont have RESET pin connecte, use power for reset
	if (state == ON)
	{
#ifdef HW_GNSS_PWR_OFF
    	HW_GNSS_PWR_OFF;
#endif // HW_GNSS_PWR_OFF
	} 
	else
	{
#ifdef HW_GNSS_PWR_ON
    	HW_GNSS_PWR_ON;
#endif // HW_GNSS_PWR_ON
	}
}

bool gps_tx_char (u8 c)
{
#ifdef HW_GNSS_UART_PUTCHAR
	if (HW_GNSS_UART_PUTCHAR(c) == 0)
		return (true);
#endif	// HW_GNSS_UART_PUTCHAR
	return (false);
}

bool gps_rx_char(u8 *c)
{
#ifdef HW_GNSS_UART_GETCHAR
    s16 result;

	if ((result = HW_GNSS_UART_GETCHAR()) >= 0)
	{
		*c = result & 0xFF;
		return (true);
	}
#endif	// HW_GNSS_UART_GETCHAR
    return (false);
}

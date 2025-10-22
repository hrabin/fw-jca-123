#ifndef PCB_H
#define PCB_H

#include "gpio.h"

// STM32G474VE - 512kiB FLASH, 128kiB RAM
// STM32G473RCT - 256kiB FLASH, 128kiB RAM
// internal FLASH parameters, default is dual bank mode !
#define FLASH_START_ADDR (0x8000000)
#define FLASH_SIZE 0x80000 // 2x256kiB (DBANK==1 dual bank on/off)
#define FLASH_PAGE_BITS (11)
#define FLASH_PAGE_SIZE (1<<FLASH_PAGE_BITS) // 2ki

#define FLASH_BANK_PAGE_BITS (7) // 
#define FLASH_BANK_PAGE_MASK ((1<<FLASH_BANK_PAGE_BITS)-1) // == 127

// LED_OUT   PC1 (pin9)
#define HW_LED_BIT                  (15)
#define HW_LED_PORT                 GPIOB
#define HW_LED_INIT                 GPIO_MODE(HW_LED_PORT,    HW_LED_BIT, GPIO_MODE_OUTPUT)
#define HW_LED_ON                   GPIO_BIT_SET(HW_LED_PORT, HW_LED_BIT)
#define HW_LED_OFF                  GPIO_BIT_CLR(HW_LED_PORT, HW_LED_BIT)

// POWER drive 
// HW_PWR_BAT_ENA (PA3) - connect battery for using
#define HW_PWR_BAT_ENA_BIT           (3)
#define HW_PWR_BAT_ENA_PORT          GPIOA
#define HW_PWR_BAT_ENA_INIT          GPIO_MODE(HW_PWR_BAT_ENA_PORT,HW_PWR_BAT_ENA_BIT, GPIO_MODE_OUTPUT)
#define HW_PWR_BAT_ENA_ON            GPIO_BIT_SET(HW_PWR_BAT_ENA_PORT,HW_PWR_BAT_ENA_BIT)
#define HW_PWR_BAT_ENA_OFF           GPIO_BIT_CLR(HW_PWR_BAT_ENA_PORT,HW_PWR_BAT_ENA_BIT)

// HW_PWR_BAT_CHARGE (PC0) - enable charging
#define HW_PWR_BAT_CHARGE_BIT       (0)
#define HW_PWR_BAT_CHARGE_PORT      GPIOC
#define HW_PWR_BAT_CHARGE_INIT      GPIO_MODE(HW_PWR_BAT_CHARGE_PORT,HW_PWR_BAT_CHARGE_BIT, GPIO_MODE_OUTPUT)
#define HW_PWR_BAT_CHARGE_ON        GPIO_BIT_SET(HW_PWR_BAT_CHARGE_PORT,HW_PWR_BAT_CHARGE_BIT)
#define HW_PWR_BAT_CHARGE_OFF       GPIO_BIT_CLR(HW_PWR_BAT_CHARGE_PORT,HW_PWR_BAT_CHARGE_BIT)

// HW_PWR_BAT_TEST (PA4) - connect discharge resistor to battery 
#define HW_PWR_BAT_TEST_BIT         (4)
#define HW_PWR_BAT_TEST_PORT        GPIOA
#define HW_PWR_BAT_TEST_INIT        GPIO_MODE(HW_PWR_BAT_TEST_PORT,HW_PWR_BAT_TEST_BIT, GPIO_MODE_OUTPUT)
#define HW_PWR_BAT_TEST_ON          GPIO_BIT_SET(HW_PWR_BAT_TEST_PORT,HW_PWR_BAT_TEST_BIT)
#define HW_PWR_BAT_TEST_OFF         GPIO_BIT_CLR(HW_PWR_BAT_TEST_PORT,HW_PWR_BAT_TEST_BIT)

// POWER meas
#define HW_ADC_MAIN_MEAS_CHNL       1
#define HW_ADC_MAIN_MEAS_MULT       (33353)
#define HW_ADC_BAT_MEAS_CHNL        3
#define HW_ADC_BAT_MEAS_MULT        2010
#define HW_ADC_V4_MEAS_CHNL         2
#define HW_ADC_V4_MEAS_MULT         2000

// MODEM 
#define UART3_ON                    1  // (PB10, PB11)
#define UART3_TX_BUF_DEPTH          512
#define UART3_RX_BUF_DEPTH          2048

// HW_LTE_PWRKEY (PD2)
#define HW_MODEM_PWRKEY_BIT         (2)
#define HW_MODEM_PWRKEY_PORT        GPIOD
#define HW_MODEM_PWRKEY_INIT      { GPIO_MODE(HW_MODEM_PWRKEY_PORT,HW_MODEM_PWRKEY_BIT, GPIO_MODE_OUTPUT) \
                                      GPIO_OUTPUT_TYPE(HW_MODEM_PWRKEY_PORT,HW_MODEM_PWRKEY_BIT,GPIO_OUTPUT_TYPE_OPENDRAIN);}
#define HW_MODEM_PWRKEY_HI          GPIO_BIT_SET(HW_MODEM_PWRKEY_PORT,HW_MODEM_PWRKEY_BIT)
#define HW_MODEM_PWRKEY_LOW         GPIO_BIT_CLR(HW_MODEM_PWRKEY_PORT,HW_MODEM_PWRKEY_BIT)

// HW_LTE_OE (PB12) - level shifter enable
#define HW_MODEM_OE_BIT             (12)
#define HW_MODEM_OE_PORT            GPIOB
#define HW_MODEM_OE_INIT            GPIO_MODE(HW_MODEM_OE_PORT,HW_MODEM_OE_BIT, GPIO_MODE_OUTPUT)
#define HW_MODEM_OE_HI              GPIO_BIT_SET(HW_MODEM_OE_PORT,HW_MODEM_OE_BIT)
#define HW_MODEM_OE_LOW             GPIO_BIT_CLR(HW_MODEM_OE_PORT,HW_MODEM_OE_BIT)

// HW_LTE_PON_TRIG (PB6)
#define HW_MODEM_PON_TRIG_BIT       (6)
#define HW_MODEM_PON_TRIG_PORT      GPIOB
#define HW_MODEM_PON_TRIG_INIT      GPIO_MODE(HW_MODEM_PON_TRIG_PORT,HW_MODEM_PON_TRIG_BIT, GPIO_MODE_OUTPUT)
#define HW_MODEM_PON_TRIG_HI        GPIO_BIT_SET(HW_MODEM_PON_TRIG_PORT,HW_MODEM_PON_TRIG_BIT)
#define HW_MODEM_PON_TRIG_LOW       GPIO_BIT_CLR(HW_MODEM_PON_TRIG_PORT,HW_MODEM_PON_TRIG_BIT)

// HW_LTE_DTR (PB7)
#define HW_MODEM_DTR_BIT            (7)
#define HW_MODEM_DTR_PORT           GPIOB
#define HW_MODEM_DTR_INIT           GPIO_MODE(HW_MODEM_DTR_PORT,HW_MODEM_DTR_BIT, GPIO_MODE_OUTPUT)
#define HW_MODEM_DTR_HI             GPIO_BIT_SET(HW_MODEM_DTR_PORT,HW_MODEM_DTR_BIT)
#define HW_MODEM_DTR_LOW            GPIO_BIT_CLR(HW_MODEM_DTR_PORT,HW_MODEM_DTR_BIT)

#define HW_MODEM_UART_GETCHAR()     uart3_getchar()
#define HW_MODEM_UART_PUTCHAR(ch)   uart3_putchar(ch)
#define HW_MODEM_UART_INIT(baud)    uart3_init(baud)
#define HW_MODEM_UART_ON            RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN
#define HW_MODEM_UART_OFF           RCC->APB1ENR1 &= ~RCC_APB1ENR1_USART3EN

// BLE (expansion connector)
#define UART4_ON                    1  // (PC10, PC11)
#define UART4_TX_BUF_DEPTH          128
#define UART4_RX_BUF_DEPTH          512
#define HW_BLE_UART_GETCHAR()       uart4_getchar()
#define HW_BLE_UART_PUTCHAR(ch)     uart4_putchar(ch)
#define HW_BLE_UART_INIT(baud)      uart4_init(baud)
#define HW_BLE_RST_BIT              (8)
#define HW_BLE_RST_PORT             GPIOC
#define HW_BLE_RST_INIT             GPIO_MODE(HW_BLE_RST_PORT, HW_BLE_RST_BIT, GPIO_MODE_OUTPUT)    
#define HW_BLE_RST_LOW              GPIO_BIT_CLR(HW_BLE_RST_PORT, HW_BLE_RST_BIT)
#define HW_BLE_RST_HI               GPIO_BIT_SET(HW_BLE_RST_PORT, HW_BLE_RST_BIT)
#define HW_BLE_PWRC_BIT             (9)
#define HW_BLE_PWRC_PORT            GPIOC
#define HW_BLE_PWRC_INIT            GPIO_MODE(HW_BLE_PWRC_PORT, HW_BLE_PWRC_BIT, GPIO_MODE_OUTPUT)    
#define HW_BLE_PWRC_LOW             GPIO_BIT_CLR(HW_BLE_PWRC_PORT, HW_BLE_PWRC_BIT)
#define HW_BLE_PWRC_HI              GPIO_BIT_SET(HW_BLE_PWRC_PORT, HW_BLE_PWRC_BIT)

// **** FLASH ****
#define SPI1_ON                     1
#define HW_FLASH_CS_BIT             (0)
#define HW_FLASH_CS_PORT            GPIOF
#define HW_FLASH_CS_INIT            GPIO_MODE(HW_FLASH_CS_PORT, HW_FLASH_CS_BIT, GPIO_MODE_OUTPUT)    
#define HW_FLASH_CS_LOW             GPIO_BIT_CLR(HW_FLASH_CS_PORT, HW_FLASH_CS_BIT)
#define HW_FLASH_CS_HI              GPIO_BIT_SET(HW_FLASH_CS_PORT, HW_FLASH_CS_BIT) 

#define HW_FLASH_SPI_INIT()         spi1_init()
#define HW_FLASH_SPI_TRANSFER(x)    spi1_transfer(x)
#define HW_FLASH_SPI_MUTEX_DEF      extern os_mutex_t spi1_mutex
#define HW_FLASH_SPI_MUTEX          spi1_mutex

// **** INPUTS ****
// HW_INP1_IN (PB1)
#define HW_INP1_BIT                 1
#define HW_INP1_PORT                GPIOB
#define HW_INP1_INIT                GPIO_MODE(HW_INP1_PORT, HW_INP1_BIT, GPIO_MODE_INPUT)
#define HW_INP1_IN                  GPIO_IN(HW_INP1_PORT, HW_INP1_BIT)

// HW_KEY_KEY (PB0)
#define HW_KEY_BIT                  0
#define HW_KEY_PORT                 GPIOB
#define HW_KEY_INIT                 GPIO_MODE(HW_KEY_PORT, HW_KEY_BIT, GPIO_MODE_INPUT)
#define HW_KEY_IN                   GPIO_IN(HW_KEY_PORT, HW_KEY_BIT)

// HW_DOOR_IN (PB2)
#define HW_DOOR_BIT                 2
#define HW_DOOR_PORT                GPIOB
#define HW_DOOR_INIT                GPIO_MODE(HW_DOOR_PORT, HW_DOOR_BIT, GPIO_MODE_INPUT)
#define HW_DOOR_IN                  GPIO_IN(HW_DOOR_PORT, HW_DOOR_BIT)

// HW_LOCK_IN (PC2)
#define HW_LOCK_BIT                 2
#define HW_LOCK_PORT                GPIOC
#define HW_LOCK_INIT                GPIO_MODE(HW_LOCK_PORT, HW_LOCK_BIT, GPIO_MODE_INPUT)
#define HW_LOCK_IN                  GPIO_IN(HW_LOCK_PORT, HW_LOCK_BIT)

// HW_UNLOCK_IN (PC3)
#define HW_UNLOCK_BIT               3
#define HW_UNLOCK_PORT              GPIOC
#define HW_UNLOCK_INIT              GPIO_MODE(HW_UNLOCK_PORT, HW_UNLOCK_BIT, GPIO_MODE_INPUT)
#define HW_UNLOCK_IN                GPIO_IN(HW_UNLOCK_PORT, HW_UNLOCK_BIT)

// **** OUTPUTS ****

// HW_LOCK_OUT (PA12)
// NOTE: lock/unlock is inverted because BM2LB150FJ pulls down when IN goes up
#define HW_LOCK_OUT_BIT             (12)
#define HW_LOCK_OUT_PORT            GPIOA
#define HW_LOCK_OUT_INIT            GPIO_MODE(HW_LOCK_OUT_PORT, HW_LOCK_OUT_BIT, GPIO_MODE_OUTPUT)    
#define HW_LOCK_OUT_LOW             GPIO_BIT_CLR(HW_LOCK_OUT_PORT, HW_LOCK_OUT_BIT)
#define HW_LOCK_OUT_HI              GPIO_BIT_SET(HW_LOCK_OUT_PORT, HW_LOCK_OUT_BIT)
#define HW_LOCK_OUT_ON              HW_LOCK_OUT_HI
#define HW_LOCK_OUT_OFF             HW_LOCK_OUT_LOW

// HW_UNLOCK_OUT (PA11)
#define HW_UNLOCK_OUT_BIT           (11)
#define HW_UNLOCK_OUT_PORT          GPIOA
#define HW_UNLOCK_OUT_INIT          GPIO_MODE(HW_UNLOCK_OUT_PORT, HW_UNLOCK_OUT_BIT, GPIO_MODE_OUTPUT)    
#define HW_UNLOCK_OUT_LOW           GPIO_BIT_CLR(HW_UNLOCK_OUT_PORT, HW_UNLOCK_OUT_BIT)
#define HW_UNLOCK_OUT_HI            GPIO_BIT_SET(HW_UNLOCK_OUT_PORT, HW_UNLOCK_OUT_BIT)
#define HW_UNLOCK_OUT_ON            HW_UNLOCK_OUT_HI
#define HW_UNLOCK_OUT_OFF           HW_UNLOCK_OUT_LOW

// HW_SIREN_ON (PA10)
#define HW_SIREN_BIT                (10)
#define HW_SIREN_PORT               GPIOA
#define HW_SIREN_INIT               GPIO_MODE(HW_SIREN_PORT, HW_SIREN_BIT, GPIO_MODE_OUTPUT)    
#define HW_SIREN_LOW                GPIO_BIT_CLR(HW_SIREN_PORT, HW_SIREN_BIT)
#define HW_SIREN_HI                 GPIO_BIT_SET(HW_SIREN_PORT, HW_SIREN_BIT)
#define HW_SIREN_ON         HW_SIREN_HI
#define HW_SIREN_OFF        HW_SIREN_LOW

// HW_CAN_SHDN (PB14)
#define HW_CAN_SHDN_BIT             (14)
#define HW_CAN_SHDN_PORT            GPIOB
#define HW_CAN_SHDN_INIT            GPIO_MODE(HW_CAN_SHDN_PORT, HW_CAN_SHDN_BIT, GPIO_MODE_OUTPUT)    
#define HW_CAN_SHDN_LOW             GPIO_BIT_CLR(HW_CAN_SHDN_PORT, HW_CAN_SHDN_BIT)
#define HW_CAN_SHDN_HI              GPIO_BIT_SET(HW_CAN_SHDN_PORT, HW_CAN_SHDN_BIT)

// 
#define HW_ACCL_CS_BIT              (13)
#define HW_ACCL_CS_PORT             GPIOB
#define HW_ACCL_CS_INIT             GPIO_MODE(HW_ACCL_CS_PORT, HW_ACCL_CS_BIT, GPIO_MODE_OUTPUT)    
#define HW_ACCL_CS_LOW              GPIO_BIT_CLR(HW_ACCL_CS_PORT, HW_ACCL_CS_BIT)
#define HW_ACCL_CS_HI               GPIO_BIT_SET(HW_ACCL_CS_PORT, HW_ACCL_CS_BIT) 


// NOTE: we use the same SPI for FLASH memory !
#define HW_ACCL_SPI_INIT()
#define HW_ACCL_SPI_TRANSFER(x)    spi1_transfer(x)
#define HW_ACCL_SPI_MUTEX_DEF      extern os_mutex_t spi1_mutex
#define HW_ACCL_SPI_MUTEX          spi1_mutex

// Debug interface (command console)
#define HW_CONSOLE_ON_UART
#define UART1_ON 1
#define HW_TTY_UART_GETCHAR() uart1_getchar()
#define HW_TTY_UART_PUTCHAR(ch) uart1_putchar(ch)
#define HW_TTY_UART_INIT(baud) uart1_init(baud)
#define UART1_TX_BUF_DEPTH 2048

// GNSS uart je USART2 GNSS_TX PD5, GNSS_RX PD6
#define UART2_ON 1
#define UART2_TX_BUF_DEPTH 128
#define UART2_RX_BUF_DEPTH 512

#define HW_GNSS_UART_INIT(baud)  uart2_init(baud)
#define HW_GNSS_UART_GETCHAR()   uart2_getchar()
#define HW_GNSS_UART_PUTCHAR(ch) uart2_putchar(ch)
#define HW_GNSS_UART_WAKEUP      uart2_wakeup()
#define HW_GNSS_UART_SLEEP       uart2_sleep()

// GNSS_ON PD7
#define HW_GNSS_ON_BIT    (4)
#define HW_GNSS_ON_PORT   GPIOB
#define HW_GNSS_ON_INIT   GPIO_MODE(HW_GNSS_ON_PORT, HW_GNSS_ON_BIT, GPIO_MODE_OUTPUT)
#define HW_GNSS_ON_LOW    GPIO_BIT_CLR(HW_GNSS_ON_PORT, HW_GNSS_ON_BIT)
#define HW_GNSS_ON_UP     GPIO_BIT_SET(HW_GNSS_ON_PORT, HW_GNSS_ON_BIT)
#define HW_GNSS_PWR_ON    HW_GNSS_ON_UP
#define HW_GNSS_PWR_OFF   HW_GNSS_ON_LOW

// GSM
#define UART3_ON 1

/*
//RADIO
#define SPI3_ON  1

#define HW_RF_CS_BIT   (9)
#define HW_RF_CS_PORT  GPIOC
#define HW_RF_CS_INIT  GPIO_MODE(HW_RF_CS_PORT, HW_RF_CS_BIT, GPIO_MODE_OUTPUT)    
#define HW_RF_CS_LO    GPIO_BIT_CLR(HW_RF_CS_PORT, HW_RF_CS_BIT)
#define HW_RF_CS_HI    GPIO_BIT_SET(HW_RF_CS_PORT, HW_RF_CS_BIT) 

#define HW_RF_SDN_BIT   (8)
#define HW_RF_SDN_PORT  GPIOC
#define HW_RF_SDN_INIT  GPIO_MODE(HW_RF_SDN_PORT, HW_RF_SDN_BIT, GPIO_MODE_OUTPUT)    
#define HW_RF_SDN_LO    GPIO_BIT_CLR(HW_RF_SDN_PORT, HW_RF_SDN_BIT)
#define HW_RF_SDN_HI    GPIO_BIT_SET(HW_RF_SDN_PORT, HW_RF_SDN_BIT) 

//  dummy defines
#define BAT_ON_INIT
#define BAT_ON_ON
#define BAT_ON_OFF

*/

#endif // PCB_H


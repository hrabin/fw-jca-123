# About

GSM GPS car alarm system JCA-123 bootloader and application.

Designed for HW https://github.com/hrabin/hw-jca-123.git

Currently work in progress, but basic functionality works:

 * Traccar communication
 * SMS commands and responses
 * FW OTA upgrade - TBD

# STM32 based firmware

Minimal requirements:

 * Linux compatible OS
 * arm-none-eabi-gcc

For MCU programming you will need ST-Link HW.

# Project structure

Current setup for CPU STM32G4xx.

```
├── app                 # application main directory
│   └── build           # application build directory
├── bootloader          # bootloader main directory
│   └── build           # bootloader build directory
├── doc                 # device documentation
├── hw                  # hardware definitions
├── lib                 # additional libraries
│   └── pdu             # library for PDU manipulation
├── sdk                 # submodule (fw-stm32-sdk.git)
│   ├── common          # universal libraries
│   ├── doc             # MCU documentation
│   ├── drv_*           # drivers
│   ├── hal             # HW abstraction layer (universal)
│   ├── freertos        # 
│   └── stm32           # STM32 related source files 
│       ├── CMSIS       # 
│       │   ├── device  # MCU definitions 
│       │   ├── inc     # stm32 core definitions
│       │   ├── linker  # linker files
│       │   └── src     # stm32 core sources
│       └── STM32*      # submodules (STMicroelectronics)
└── ut
    ├── app_container   # tool for build FW container
    ├── app_crc         # tool for FW crc calculation
    ├── common          # common for tools 
    └── upload          # tool for FW uplad using serial port

```

# Build

## Prepare

Tools installation i.e. Debian based OS :

```
$ apt install gcc-arm-none-eabi
```

Flashing tools install:

```
$ apt install stlink-tools
```

Update submodules
```
$ git submodule update --init --recursive
```

## Application

```bash
$ cd ./app
$ make
$ make flash
```

## Bootloader

```bash
$ cd ./bootloader
$ make HW_ADDR=0x100001
$ make flash
```

Note: For successful programming you need to flash app first and then bootloader.

# Function

TBD

## Command help
```
AT: send AT-command
AUTH: authorize channel AUTH="pass"
BLE: send cmd to BLE module
CFG: CFG=<n>[,"value"]
DL: set debug level[,select]
ECHO: get/set modem echo
GPS: get GPS coordinates
HELP: Show this help text
IO: get IO state
LOCK: lock pulse
UNLOCK: unlock pulse
NET: Get network info
REBOOT: REBOOT=<n>
RTC: get RTC time
SET: switch to set
STATUS: Get status info
UNSET: switch to unset
UPDATE: request to update FW from server
VER: Request product information
```


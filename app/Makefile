include version.mk

TARGET = $(DEVICE_NAME)-$(SW_VERSION_NAME)
BOOT = 0
DEBUG = 0

OPT = -Os
# -Os == size optimalization, -Og for debugging

DIR_ROOT  ?= .

DIR_HW     := ../hw
DIR_MODEM  := ../modem
DIR_LIB    := ../lib
DIR_SDK    := ../sdk
DIR_UTILS  := ../ut
DIR_RTOS   := $(DIR_SDK)/freertos

# tools setup
DIR_CRC       := $(DIR_UTILS)/app_crc
CRC = $(DIR_CRC)/app_crc.bin
DIR_CONTAINER := $(DIR_UTILS)/app_container
CONTAINER = $(DIR_CONTAINER)/ac.bin

include  $(DIR_SDK)/sdk_stm32g473.mk

BUILD_DIR = build

MAKEFLAGS += "-j $(shell nproc)"

C_SOURCES +=  \
  $(DIR_ROOT)/alarm.c \
  $(DIR_ROOT)/analog.c \
  $(DIR_ROOT)/app.c \
  $(DIR_ROOT)/app_info.c \
  $(DIR_ROOT)/ble.c \
  $(DIR_ROOT)/can.c \
  $(DIR_ROOT)/cfg.c \
  $(DIR_ROOT)/cfg_table.c \
  $(DIR_ROOT)/cmd.c \
  $(DIR_ROOT)/event.c \
  $(DIR_ROOT)/event_buf.c \
  $(DIR_ROOT)/gps.c \
  $(DIR_ROOT)/gps_buffer.c \
  $(DIR_ROOT)/gps_io.c \
  $(DIR_ROOT)/io.c \
  $(DIR_ROOT)/main.c \
  $(DIR_ROOT)/net.c \
  $(DIR_ROOT)/power.c \
  $(DIR_ROOT)/section.c \
  $(DIR_ROOT)/server.c \
  $(DIR_ROOT)/sms_processing.c \
  $(DIR_ROOT)/storage.c \
  $(DIR_ROOT)/system.c \
  $(DIR_ROOT)/text.c \
  $(DIR_ROOT)/tracer.c \
  $(DIR_ROOT)/tracer_h02.c \
  $(DIR_ROOT)/update.c \
  \
  $(DIR_HAL)/hw_info.c \
  $(DIR_HAL)/led.c \
  $(DIR_HAL)/log.c \
  $(DIR_HAL)/os.c \
  $(DIR_HAL)/shock.c \
  $(DIR_HAL)/tty.c \
  $(DIR_HAL)/wdog.c \
  \
  $(DIR_DRV)/adc.c \
  $(DIR_DRV)/flash_spi_universal.c \
  $(DIR_DRV)/gpio.c \
  $(DIR_DRV)/int_flash.c \
  $(DIR_DRV)/irq.c \
  $(DIR_DRV)/lis2dh12.c \
  $(DIR_DRV)/rtc.c \
  $(DIR_DRV)/spi.c \
  $(DIR_DRV)/sys.c \
  $(DIR_DRV)/uart.c \
  $(DIR_DRV)/wd.c \
  \
  $(DIR_COMMON)/buf.c \
  $(DIR_COMMON)/chsum.c \
  $(DIR_COMMON)/parse.c \
  $(DIR_COMMON)/text_lib.c \
  $(DIR_COMMON)/util.c \
  \
  $(DIR_RTOS)/source/croutine.c \
  $(DIR_RTOS)/source/event_groups.c \
  $(DIR_RTOS)/source/list.c \
  $(DIR_RTOS)/source/queue.c \
  $(DIR_RTOS)/source/stream_buffer.c \
  $(DIR_RTOS)/source/tasks.c \
  $(DIR_RTOS)/source/timers.c \
  $(DIR_RTOS)/source/CMSIS_RTOS_V2/cmsis_os2.c \
  $(DIR_RTOS)/source/portable/MemMang/heap_4.c \
  $(DIR_RTOS)/source/portable/GCC/ARM_CM4F/port.c \
  \
  $(DIR_MODEM)/modem.c \
  $(DIR_MODEM)/modem_at.c \
  $(DIR_MODEM)/modem_bg95.c \
  $(DIR_MODEM)/modem_main.c \
  $(DIR_MODEM)/modem_sms.c \
  $(DIR_MODEM)/multi_sms.c \
  $(DIR_MODEM)/udp.c \
  \
  $(DIR_LIB)/pdu/pdu.c \


C_DEFS += $(VERSION_DEFS) \
-DMAIN_DEBUG=$(DEBUG) \
-DBOOTLOADER=0 \
-DLOG_ENABLE=1 -DLOG_LOCK=1 

# C includes
C_INCLUDES +=  \
-I$(DIR_ROOT) \
-I$(DIR_HW) \
-I$(DIR_LIB) \
-I$(DIR_MODEM) \
-I$(DIR_RTOS)/source/include \
-I$(DIR_RTOS)/source/CMSIS_RTOS_V2 \
-I$(DIR_RTOS)/source/portable/GCC/ARM_CM4F \
-I$(DIR_RTOS) \

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

# add printf float support
LDFLAGS += -u _printf_float

#######################################

# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))


# Compilation rule for C sources
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

# Compilation rule for ASM sources
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

# Linking rule
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

# HEX generation rule
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR) $(CRC)
	$(HEX) $< $@
	-echo "Compute HEX CRC"
	$(CRC) < $(BUILD_DIR)/$(TARGET).hex > $(BUILD_DIR)/$(TARGET)_crc.hex
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir $@		

# tools build rules
$(CRC):
	@echo "Building CRC tool..."
	$(MAKE) -C $(DIR_CRC)

$(CONTAINER):
	@echo "Building container tool..."
	$(MAKE) -C $(DIR_CONTAINER)

#######################################

.PHONY: clean
clean:
	-rm -fR $(BUILD_DIR)

.PHONY: flash
flash:
	#-st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x8000000
	-st-flash --format ihex --reset write $(BUILD_DIR)/$(TARGET)_crc.hex

.PHONY: fullflash
fullflash:  flash
	-st-flash --format ihex --reset write boot.hex

.PHONY: deploy
deploy: $(BUILD_DIR)/$(TARGET).hex | $(CONTAINER) 
	-cat $(BUILD_DIR)/$(TARGET)_crc.hex | $(CONTAINER) -n "$(SW_VERSION_NAME)" -e0 -x > $(BUILD_DIR)/$(TARGET)_container.hex


.PHONY: erase
erase:
	-st-flash erase
  
#######################################
# -include $(wildcard $(BUILD_DIR)/*.d)


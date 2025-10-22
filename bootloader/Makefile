TARGET = bootloader
BOOT = 1

# HW_ADDR ?= 0x100001

DEBUG = 0
OPT = -Os
# -Os == size optimalization, -Og for debugging

DIR_ROOT  ?= .

DIR_HW     := ../hw
DIR_SDK    := ../sdk

include  $(DIR_SDK)/sdk_stm32g473.mk

BUILD_DIR = build

MAKEFLAGS += "-j $(shell nproc)"

C_SOURCES +=  \
  $(DIR_ROOT)/main.c \
  $(DIR_ROOT)/ext_storage.c \
  $(DIR_ROOT)/container.c \
  $(DIR_ROOT)/ihex.c \
  $(DIR_ROOT)/app_info.c \
  \
  $(DIR_HAL)/led.c \
  $(DIR_HAL)/log.c \
  $(DIR_HAL)/hw_info.c \
  \
  $(DIR_DRV)/flash_spi_universal.c \
  $(DIR_DRV)/gpio.c \
  $(DIR_DRV)/int_flash.c \
  $(DIR_DRV)/irq.c \
  $(DIR_DRV)/reset.c \
  $(DIR_DRV)/spi.c \
  $(DIR_DRV)/sys.c \
  $(DIR_DRV)/timer.c \
  $(DIR_DRV)/tty.c \
  $(DIR_DRV)/uart.c \
  $(DIR_DRV)/wd.c \
  \
  $(DIR_COMMON)/buf.c \
  $(DIR_COMMON)/chsum.c \
  $(DIR_COMMON)/util.c \


C_DEFS +=  \
-DHW_ADDR=$(HW_ADDR) \
-DMAIN_DEBUG=$(DEBUG) \
-DBOOTLOADER=1 \
-DLOG_ENABLE=1

# C includes
C_INCLUDES +=  \
-I$(DIR_ROOT) \
-I$(DIR_HW)

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin


#######################################

# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir $@		

#######################################

.PHONY: clean
clean:
	-rm -fR $(BUILD_DIR)

.PHONY: flash
flash:
	#-st-flash --reset write $(BUILD_DIR)/$(TARGET).bin 0x8000000
	-st-flash --format ihex --reset write $(BUILD_DIR)/$(TARGET).hex


.PHONY: erase
erase:
	-st-flash erase
  
#######################################
# -include $(wildcard $(BUILD_DIR)/*.d)


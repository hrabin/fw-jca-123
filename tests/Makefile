# Unit tests for fw-jca-123 — build and run on host (x86) machine.
#
# Usage:
#   make          build the test runner
#   make test     build + run
#   make clean    remove build artifacts
#   make add      create a new test_xxx.c skeleton

CC         = gcc
CFLAGS     = -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough -Os
BUILD_DIR  = build
TARGET     = $(BUILD_DIR)/test_runner

# Include paths: stubs first (override embedded headers), then real SDK
INCLUDES   = -I./stubs \
             -I../sdk/common \
             -I../sdk/drv_g4 \
             -I../lib/nmea \
             -I../modem \
             -I../net

# Source files under test (from the main project)
SRC_UNDER_TEST = ../lib/nmea/nmea.c \
                 ../sdk/common/parse.c \
                 ../sdk/common/buf.c \
                 ../sdk/common/util.c \
                 ../sdk/common/text_lib.c \
                 ../modem/modem.c \
                 ../modem/modem_at.c \
                 ../modem/multi_sms.c

# Test source files (auto-discovered: test_*.c)
TEST_SRCS      = $(wildcard test_*.c) test_main.c

ALL_SRCS       = $(TEST_SRCS) $(SRC_UNDER_TEST)
OBJECTS        = $(addprefix $(BUILD_DIR)/,$(notdir $(ALL_SRCS:.c=.o)))

# Let pattern rules find sources across directories
vpath %.c $(sort $(dir $(ALL_SRCS)))

# ---- build ----

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR):
	mkdir -p $@

# ---- phony targets ----

.PHONY: test
test: $(TARGET)
	@./$(TARGET)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: add
add:
	@read -p "Test name (without test_ prefix): " name; \
	echo '#include "test.h"'  > "test_$$name.c"; \
	echo '' >> "test_$$name.c"; \
	echo 'TEST('"$$name"')' >> "test_$$name.c"; \
	echo '{' >> "test_$$name.c"; \
	echo '    // TODO: write test' >> "test_$$name.c"; \
	echo '    ASSERT(0);  // placeholder — remove when test is written' >> "test_$$name.c"; \
	echo '}' >> "test_$$name.c"; \
	echo "Created test_$$name.c"

# ================================
# Project Configuration
# ================================
TARGET        = blinky
LINKER        = ../../libs/linkers/stm32g071rb_application.ld
SRC_DIR       = src
INC_DIR       = inc
BUILD_DIR     = build
LIBRARY_BUILD_DIR   = ../../libs/stm32g071rb/build
LIBRARY_SRC_DIR   = ../../libs/stm32g071rb/src
LIBRARY_INC_DIR   = ../../libs/stm32g071rb/inc
LIBRARY       = $(LIBRARY_BUILD_DIR)/libstm32g071rb.a
STARTUP       = $(LIBRARY_SRC_DIR)/startup_stm32g071rb.c
SYSCALLS      = $(LIBRARY_SRC_DIR)/syscalls.c

# ================================
# Toolchain
# ================================
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy

# ================================
# Compilation Flags
# ================================
MCUFLAGS     = -mcpu=cortex-m0plus -mthumb
CORE_CFLAGS  = -nostdlib -nostartfiles
DEBUGFLAGS   = -g -Wall -Wpedantic -Werror -fstack-usage
INCLUDES     = -I$(INC_DIR) -I$(LIBRARY_BUILD_DIR) -I$(LIBRARY_INC_DIR)
SPECS        = -specs=nosys.specs -specs=nano.specs
CFLAGS       = $(MCUFLAGS) $(CORE_CFLAGS) $(DEBUGFLAGS) $(INCLUDES) $(SPECS)
LDFLAGS      = -T $(LINKER)

# ================================
# Source Files and Objects
# ================================
SRCS = $(wildcard $(SRC_DIR)/*.c) $(STARTUP) $(SYSCALLS)
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SRCS)))

# ================================
# Build Rules
# ================================
all: $(BUILD_DIR) $(TARGET).elf $(TARGET).bin

# Create firmware build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile firmware source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo Compiling $<...

$(BUILD_DIR)/%.o: ../../libs/stm32g071rb/src/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo Compiling $<...

# Link everything
$(TARGET).elf: $(OBJS) $(LIBRARY)
	@$(CC) $(MCUFLAGS) $(DEBUGFLAGS) $(OBJS) $(LIBRARY) $(LDFLAGS) -o $@
	@echo Linking complete!

# Convert to binary and hex
$(TARGET).bin: $(TARGET).elf
	@$(OBJCOPY) -O binary $< $(TARGET).bin
	@$(OBJCOPY) -O ihex $< $(TARGET).hex
	@echo Success!
	@echo Created: $(TARGET).bin, $(TARGET).hex, $(TARGET).elf

# Size reporting
size: $(TARGET).elf
	@arm-none-eabi-size $<

# Clean all
clean:
	@rm -rf $(BUILD_DIR) $(TARGET).elf $(TARGET).bin $(TARGET).hex
	@echo Cleaned up build files.

# Mark phony targets
.PHONY: all clean size

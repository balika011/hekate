ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

TARGET := rcm_bootrom
BUILD := build
OUTPUT := output
SOURCEDIR = bootloader
VPATH = $(dir $(wildcard ./$(SOURCEDIR)/*/)) $(dir $(wildcard ./$(SOURCEDIR)/*/*/))

OBJS = $(addprefix $(BUILD)/$(TARGET)/, \
	start.o \
	bootrom.o \
	se.o \
	sdmmc.o \
	sdmmc_driver.o \
	util.o \
	gpio.o \
	max7762x.o \
	clock.o \
	i2c.o \
)

ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork
CUSTOMDEFINES := #-DDEBUG
CFLAGS = $(ARCH) -O2 -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-inline -std=gnu11 -Wall $(CUSTOMDEFINES)
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections

MODULEDIRS := $(wildcard modules/*)

.PHONY: all clean $(MODULEDIRS)

all: $(TARGET).bin
	@echo -n "Payload size is "
	@printf "0x%x\n" `wc -c < $(OUTPUT)/$(TARGET).bin`
	@echo "Max size is 0x1ED58 Bytes."

clean:
	@rm -rf $(OBJS)
	@rm -rf $(BUILD)
	@rm -rf $(OUTPUT)

$(MODULEDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(TARGET).bin: $(BUILD)/$(TARGET)/$(TARGET).elf $(MODULEDIRS)
	$(OBJCOPY) -S -O binary $< $(OUTPUT)/$@
	@printf ICTC$(BLVERSION_MAJOR)$(BLVERSION_MINOR) >> $(OUTPUT)/$@

$(BUILD)/$(TARGET)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -T $(SOURCEDIR)/link.ld $^ -o $@

$(BUILD)/$(TARGET)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET)/%.o: %.S
	@mkdir -p "$(BUILD)"
	@mkdir -p "$(BUILD)/$(TARGET)"
	@mkdir -p "$(OUTPUT)"
	$(CC) $(CFLAGS) -c $< -o $@

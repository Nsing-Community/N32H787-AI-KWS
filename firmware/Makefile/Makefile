# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

######################################
# target
######################################
# Recipe shell. build.cmd passes SH_BIN (the bundled BusyBox shim or a
# Git-for-Windows sh.exe); plain `make` from Git Bash / MSYS2 keeps /bin/sh.
# SHELL itself is special in GNU make and is never read from the environment,
# so the indirection through SH_BIN is what makes it overridable.
SH_BIN ?= /bin/sh
SHELL := $(SH_BIN)

TARGET = n32h787_kws_demo
OPENOCD ?= openocd
OPENOCD_CONFIG := ../openocd/n32h7x_cmsisdap.tcl

# Standalone build: the application is the only image in Flash, placed at
# the flash base (0x15000000).  The board does not have a USB-MSC bootloader
# in the 0x15000000..0x15005FFF window, so the boot ROM vectors land on this
# image directly.
APP_FLASH_BASE := 0x15000000

# Connect conservatively, then run the validated NSLink bulk transfer rate.
# Measured knees on this probe: 2000 kHz is slower, 6000 kHz and 8000 kHz are
# identical, so 6000 is the point of diminishing returns.
CONNECT_KHZ ?= 1000
ADAPTER_KHZ ?= 6000

######################################
# building variables
######################################
# debug build
ifeq ($(release), y)
DEBUG = 0
else
DEBUG = 1
endif
# optimization
OPT = -O2

#######################################
# Build path
#######################################
BUILD_DIR = build

#####################################
# chip platform info
#####################################
TARGET_STARTUP  := n32h78x_cm7
## NTFx CODE START Define ##
DEFS += -DCORE_CM7
# Required by CMSIS cache APIs; the vendor device header omits them.
DEFS += -D__DCACHE_PRESENT=1U -D__ICACHE_PRESENT=1U
DEFS += -DN32H78x
DEFS += -DUSE_STDPERIPH_DRIVER
# Application memory partition: 256 KiB ITCM + 512 KiB DTCM + 256 KiB AXI SRAM2.
DEFS += -DUSING_TCM
# The prebuilt TensorFlow Lite Micro library was built with this reduced
# TfLiteTensor/TfLiteNode ABI.  Every translation unit including TFLM headers
# must use the identical ABI.
DEFS += -DTF_LITE_STATIC_MEMORY
## NTFx CODE END Define ##

######################################
#TOOLS CHAIN
######################################
# The gcc path can be defined by 3 methods:
# 1.Define it in make command via GCC_PATH variable (> make GCC_PATH="D:\\...\\bin\\")
# 2.Added it to the PATH environment variable
# 3.Define it in makefile ,such as :
#GCC_PATH = 'D:\...\bin\'
ifdef GCC_PATH
CROSS_COMPILE = $(GCC_PATH)arm-none-eabi-
else
CROSS_COMPILE = arm-none-eabi-
endif

######################################
# C sources
######################################
## NTFx CODE START C sources ##
C_DIRS += ../USER/src
C_DIRS += ../Driver/CMSIS/core
C_DIRS += ../Driver/CMSIS/device
C_DIRS += ../Driver/n32h7xx_std_periph_driver/src
## NTFx CODE END C sources ##

# TensorFlow Lite Micro keyword-spotting sources (compiled as C++).
CPP_DIRS += ../USER/src

PYTHON ?= python3

SRC_OBJS_DIRS += $(foreach DIR, $(C_DIRS), $(wildcard $(DIR)/*.c))
# The vendor package contains two mutually exclusive XSPI driver revisions
# that export the same XSPI_* symbols. N32H760 uses the standard driver.
C_SOURCES = $(filter-out \
    ../Driver/n32h7xx_std_periph_driver/src/n32h7xx_xspi_v2.c, \
    $(SRC_OBJS_DIRS))
# kws_model_data.cc is generated from the .tflite by the rule further down, so
# it does not exist when the wildcard runs on a fresh clone and would never be
# compiled -- the model would be silently absent from the image.  Naming it
# explicitly fixes that, and $(sort) collapses it back into the wildcard's own
# hit on later builds: a duplicate here would put the object on the link line
# twice, which the linker reports as a multiple definition and, because each
# copy contributes its own .kws_model input section, as a model twice the size
# of the region meant to hold it.
CPP_SOURCES = $(sort $(foreach DIR, $(CPP_DIRS), $(wildcard $(DIR)/*.cc)) \
    ../USER/src/kws_model_data.cc)

######################################
# ASM sources
######################################
ASM_SOURCES =../Driver/CMSIS/device/startup/startup_$(TARGET_STARTUP)_gcc.s

######################################
# C includes
######################################
## NTFx CODE START C includes ##
C_INCS += ../USER/inc
C_INCS += ../Driver/CMSIS/core
C_INCS += ../Driver/CMSIS/device
C_INCS += ../Driver/n32h7xx_std_periph_driver/inc
## NTFx CODE END C includes ##
TFLM_INCLUDES = -I../USER/tflm_sdk \
                -I../USER/tflm_sdk/third_party/flatbuffers/include \
                -I../USER/tflm_sdk/third_party/gemmlowp \
                -I../USER/tflm_sdk/third_party/ruy

INCS_OBJS_DIR = $(foreach DIR2, $(C_INCS), $(wildcard $(DIR2)/*.h))
INCS_OBJS_PATH = $(sort $(dir $(INCS_OBJS_DIR)))
C_INCLUDES = $(addprefix -I,$(INCS_OBJS_PATH))
C_INCLUDES += $(TFLM_INCLUDES)

######################################
# Compile & Link flags
######################################
# cpu
CPU = -mcpu=cortex-m7
FPU = -mfpu=fpv5-d16
FLOAT-ABI = -mfloat-abi=hard
# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

#CFLAGS
CFLAGS += $(MCU) -Wall
CFLAGS += $(OPT)
CFLAGS += -ffunction-sections -fdata-sections
#DEBUG
ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif
# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"
CXXFLAGS = $(CFLAGS) -std=gnu++17 -fno-exceptions -fno-rtti
#LFLAGS
LFLAGS += $(MCU)
LFLAGS += -Wl,--gc-sections
LFLAGS += --specs=nosys.specs
LFLAGS += -Xlinker -Map=$(BUILD_DIR)/$(TARGET).map
# link script
LDSCRIPT = LinkFile/n32h787_kws_demo_standalone.ld

######################################
# Targets
######################################
.PHONY: all app flash probe clean rebuild reflash
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
app: $(BUILD_DIR)/$(TARGET).bin

# Build and program the application at APP_FLASH_BASE, leaving the bootloader
# sectors untouched.
flash: $(BUILD_DIR)/$(TARGET).bin
	$(OPENOCD) -d1 -c 'gdb_port disabled' -f $(OPENOCD_CONFIG) -c 'adapter speed $(CONNECT_KHZ); n32h7x_flash {$(abspath $(BUILD_DIR)/$(TARGET).bin)} $(ADAPTER_KHZ) $(APP_FLASH_BASE); shutdown'

# No-write connection check. Run this first when programming gets stuck.
probe:
	$(OPENOCD) -d1 -c 'gdb_port disabled' -f $(OPENOCD_CONFIG) -c 'adapter speed $(CONNECT_KHZ); n32h7x_connect; poll; shutdown'

######################################
# Objects
######################################
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of C++ objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(CPP_SOURCES:.cc=.o)))
vpath %.cc $(sort $(dir $(CPP_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	"$(CROSS_COMPILE)gcc" $(CFLAGS) $(DEFS) $(C_INCLUDES) -c -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.cc Makefile | $(BUILD_DIR)
	"$(CROSS_COMPILE)g++" $(CXXFLAGS) $(DEFS) $(C_INCLUDES) -c -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cc=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	"$(CROSS_COMPILE)gcc" $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT) ../USER/tflm_sdk/lib/libtensorflow-microlite.a
	"$(CROSS_COMPILE)g++" $(OBJECTS) ../USER/tflm_sdk/lib/libtensorflow-microlite.a $(LFLAGS) -T$(LDSCRIPT) -o $@
	"$(CROSS_COMPILE)size" $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	"$(CROSS_COMPILE)objcopy" -O binary -S $< $@

$(BUILD_DIR)/main.o $(BUILD_DIR)/system_n32h7xx.o: DEFS += \
	-DAPP_FLASH_BASE=$(APP_FLASH_BASE) -DVECT_TAB_BASE=$(APP_FLASH_BASE)

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	"$(CROSS_COMPILE)objcopy" -O ihex -S $< $@

$(BUILD_DIR):
	@mkdir -p $@

# The DS-CNN-S model is ~95 KiB, which would be a ~570 KiB source file if it
# were written out by hand, so it is generated from the .tflite instead.  It
# lands in USER/src, where the CPP_SOURCES wildcard above picks it up.  This
# rule deliberately sits after `all:` -- make takes the first rule in the file
# as its default goal.
KWS_MODEL = ../../models/DS_CNN_S.tflite

../USER/src/kws_model_data.cc: $(KWS_MODEL) ../../tools/embed_tflite.py ../USER/inc/kws_model_data.h
	$(PYTHON) ../../tools/embed_tflite.py $< $@ --symbol g_kws_model_data \
	    --header kws_model_data.h --section .kws_model

#######################################
# clean up
#######################################
clean:
	rm -rf $(BUILD_DIR)

# Convenience aliases used via build.cmd rebuild / reflash.
rebuild: clean all
reflash: clean flash

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***

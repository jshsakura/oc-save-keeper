#---------------------------------------------------------------------------------
# oc-save-keeper - Safe save backup and sync for Nintendo Switch
#---------------------------------------------------------------------------------
.SUFFIXES:
.NOTPARALLEL: test switch-build all

HOST_CXX ?= g++
HOST_JSON_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null)
HOST_JSON_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)
HOST_CXXFLAGS := -std=c++20 -Wall -Wextra -O0 -g -I$(CURDIR)/include -I$(CURDIR) $(HOST_JSON_CFLAGS)
HOST_LDFLAGS := $(HOST_JSON_LIBS)
TEST_BUILD := build-host
TEST_BIN := $(TEST_BUILD)/unit-tests
TEST_SOURCES := $(wildcard $(CURDIR)/tests/*.cpp)
TEST_HEADERS := $(shell if [ -d "$(TOPDIR)/include" ] && [ -d "$(TOPDIR)/tests" ]; then find $(TOPDIR)/include $(TOPDIR)/tests \( -name '*.hpp' -o -name '*.h' \); fi)

TOPDIR ?= $(CURDIR)
ifneq ($(strip $(DEVKITPRO)),)
include $(DEVKITPRO)/libnx/switch_rules
endif

TARGET		:=	oc-save-keeper
BUILD		:=	build
SOURCES		:=	source source/core source/ui source/network source/utils source/fs source/zip source/ui/saves
DATA		:=	data
INCLUDES	:=	include
APP_TITLE   :=  OC Save Keeper
APP_AUTHOR  :=  OpenCourse
APP_VERSION :=  0.1.0
ROMFS	    :=	romfs
APP_ICON	:=	icon.jpg
NROFLAGS    :=  --icon=$(TOPDIR)/$(APP_ICON) --nacp=$(TOPDIR)/$(TARGET).nacp --romfsdir=$(TOPDIR)/$(ROMFS)

ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS	:=	$(INCLUDE) -D__SWITCH__ `sdl2-config --cflags` `curl-config --cflags` -g -Wall -O2 -ffunction-sections -I$(PORTLIBS)/include/freetype2 $(ARCH)
CXXFLAGS:= $(CFLAGS) -fno-rtti -fno-exceptions -std=c++20
ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS	:=	`sdl2-config --libs` -lSDL2_ttf -lfreetype -lharfbuzz `curl-config --libs` -lSDL2_image -lwebp -lpng -ljpeg -lz -lminizip -ljson-c -lnx -lbz2

LIBDIRS	:= $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif
export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 		:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export BUILD_EXEFS_SRC := $(TOPDIR)/$(EXEFS_SRC)

.PHONY: $(BUILD) clean all test switch-build
all: $(BUILD)
switch-build: $(BUILD)
ifeq ($(strip $(DEVKITPRO)),)
$(BUILD):
	@echo "Please set DEVKITPRO in your environment"
	@exit 1
else
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
endif
test: $(TEST_BIN)
	@$(TEST_BIN)

$(TEST_BIN): $(TEST_SOURCES) $(TEST_HEADERS)
	@[ -d $(TEST_BUILD) ] || mkdir -p $(TEST_BUILD)
	@$(HOST_CXX) $(HOST_CXXFLAGS) $(TEST_SOURCES) -o $(TEST_BIN) $(HOST_LDFLAGS)

clean:
	@rm -fr $(BUILD) $(TEST_BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf
else
.PHONY:	all
DEPENDS	:=	$(OFILES:.o=.d)
all	:	$(OUTPUT).nacp $(OUTPUT).nro
$(OUTPUT).nro	:	$(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf	:	$(OFILES)
%.bin.o	:	%.bin
	@$(bin2o)
-include $(DEPENDS)
endif

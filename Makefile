#---------------------------------------------------------------------------------
# oc-save-keeper - Safe save backup and sync for Nintendo Switch
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET		:=	oc-save-keeper
BUILD		:=	build
SOURCES		:=	source source/core source/ui source/network source/utils source/fs source/zip
DATA		:=	data
INCLUDES	:=	include
APP_TITLE   :=  oc-save-keeper
APP_AUTHOR  :=  OpenCourse
APP_VERSION :=  1.0.0
ROMFS	    :=	romfs
ICON		:=	icon.jpg

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

.PHONY: $(BUILD) clean all
all: $(BUILD)
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf
else
.PHONY:	all
DEPENDS	:=	$(OFILES:.o=.d)
all	:	$(OUTPUT).nro
$(OUTPUT).nro	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)
%.bin.o	:	%.bin
	@$(bin2o)
-include $(DEPENDS)
endif

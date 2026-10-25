TARGET = openbarnyard_psp_probe
OBJS = Source/main.o Source/ToshiSmoke.o Platform_PSP.o THPTimer_PSP.o
VPATH = ../../Toshi/Source/Platform/PSP

INCDIR = ../../Toshi/Source
CFLAGS = -O2 -G0 -Wall -Wextra -DTOSHI_SKU_PSP
CXXFLAGS = $(CFLAGS) -std=gnu++20 -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgum -lpspgu -lpspge -lpspdisplay -lpspctrl -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = OpenBarnyard PSP Probe
PSP_FW_VERSION = 660

ifndef PSPDEV
$(error PSPDEV is not set; install PSPDEV and export PSPDEV before building)
endif

PSPSDK := $(PSPDEV)/psp/sdk
include $(PSPSDK)/lib/build.mak

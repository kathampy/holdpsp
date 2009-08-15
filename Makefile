TARGET = hold
OBJS = import.o main.o sysconhk.o exports.o

INCDIR = ../include
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -fno-pic
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC=1
USE_KERNEL_LIBS=1

PSP_FW_VERSION = 500

LIBDIR = ../lib
LIBS = -lpsppower_driver -lpspsystemctrl_kernel
LDFLAGS = -nostdlib -nodefaultlibs

#EXTRA_TARGETS = EBOOT.PBP
#PSP_EBOOT_TITLE = Hold
#PSP_EBOOT_ICON="icon0.png"
#PSP_EBOOT_PIC1="pic1.png"
#PSP_EBOOT_SND0="snd0.at3"

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

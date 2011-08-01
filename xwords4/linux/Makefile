# -*- mode: makefile; compile-command: "make -j3 MEMDEBUG=TRUE"; -*-
# Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights
# reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

ifeq ($(MEMDEBUG),TRUE)
DEFINES = -DMEM_DEBUG -DDEBUG -DENABLE_LOGGING -DNUMBER_KEY_AS_INDEX 
DEFINES += -DCOMMS_CHECKSUM
CFLAGS += -g $(GPROFFLAG) -Wall -Wunused-parameter -Wcast-align -Werror -O0
CFLAGS += -DDEBUG_TS
PLATFORM = obj_linux_memdbg
else
DEFINES =
PLATFORM = obj_linux_rel
# Not shipping this!  Always build with symbols etc
CFLAGS += -g $(GPROFFLAG) -Wall -Wunused-parameter -Wcast-align -Werror -O0
#CFLAGS += -Os -Werror -Wunused
endif

# CC = llvm-gcc

DO_CURSES = -DPLATFORM_NCURSES
ifdef CURSES_SMALL_SCREEN
DO_CURSES += -DCURSES_SMALL_SCREEN
endif
DO_GTK = -DPLATFORM_GTK
# DO_GTK += -DUSE_CAIRO

# uncomment for standalone build
# STANDALONE = -DXWFEATURE_STANDALONE_ONLY

UNICODE ?= -DXWFEATURE_UNICODE
ifdef UNICODE
	CFLAGS += -std=gnu99
endif

SVN_REV ?= "$(shell ../scripts/gitversion.sh)"
SVNDEF = -D'SVN_REV=$(SVN_REV)'

ifdef CURSES_ONLY
DO_GTK =
# := avoids recursion
PLATFORM := $(PLATFORM)_curses
endif
ifdef GTK_ONLY
DO_CURSES =
PLATFORM := $(PLATFORM)_gtk
endif
ifdef LIB_NO_UI
DO_CURSES =
DO_GTK =
endif

DEFINES += $(DO_CURSES) $(DO_GTK) $(SVNDEF)

ifdef LIB_NO_UI
TARGET=$(PLATFORM)/libxwords.so.0
POINTER_SUPPORT = -DPOINTER_SUPPORT
else
TARGET=$(PLATFORM)/xwords
endif

include ../common/config.mk

DEFINES += -DPLATFORM_LINUX -DKEY_SUPPORT -DKEYBOARD_NAV -DNODE_CAN_4 \
	-DSCROLL_DRAG_THRESHHOLD=1 -DNUM_SAVED_ENGINE_MOVES=32
# DEFINES += -DSTUBBED_DICT
ifdef DO_GTK
DEFINES += -DXWFEATURE_SEARCHLIMIT
endif
DEFINES += -DFEATURE_TRAY_EDIT
#DEFINES += -DXWFEATURE_RAISETILE
#DEFINES += -DDRAW_WITH_PRIMITIVES
DEFINES += -DXWFEATURE_CROSSHAIRS
DEFINES += -DXWFEATURE_CHAT
DEFINES += -DDISABLE_TILE_SEL
DEFINES += -DSET_GAMESEED
DEFINES += -DTEXT_MODEL

ifdef CURSES_CELL_HT
DEFINES += -DCURSES_CELL_HT=$(CURSES_CELL_HT)
endif
ifdef CURSES_CELL_WIDTH
DEFINES += -DCURSES_CELL_WIDTH=$(CURSES_CELL_WIDTH)
endif
DEFINES += $(UNICODE)

# Networking-related features.  Only set these if STANDALONE is not set
ifeq ($(STANDALONE),)

# Bluetooth support
ifndef NO_BLUETOOTH
BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_L2CAP
endif
#BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_RFCOMM
# DEFINES += -DXWFEATURE_IR
DEFINES += ${BLUETOOTH}
DEFINES += -DXWFEATURE_RELAY
DEFINES += -DXWFEATURE_SMS

# Robot can be made to think, to simulate for relay mostly
DEFINES += -DXWFEATURE_SLOW_ROBOT

# Support device-to-device connection via UDP, e.g. using wifi on a
# LAN or where the host/server isn't behind a firewall.
# DEFINES += -DXWFEATURE_IP_DIRECT

# RELAY_HEARTBEAT means relay (must be compiled with same -D) works
# with comms on heartbeat.  Works only with relay.  COMMS_HEARTBEAT
# should work on any comms transport (even IR, but user experience
# will be very bad!).  Is particularly useful with BT.  Is not used
# for relay.
# DEFINES += -DCOMMS_HEARTBEAT
endif
DEFINES += $(STANDALONE)

# Let users pick the tiles going into their trays
#DEFINES += -DFEATURE_TRAY_EDIT
DEFINES += -DDONT_ABORT_ENGINE

DEFINES += -DPERIMETER_FOCUS

#-DDEBUG -DEIGHT_TILES

#GPROFFLAG = -pg

# INCLUDES += -I/usr/lib/glib/include
INCLUDES += ${EXTRAINCS}

ifdef DO_GTK
GTK_OBJS = \
	$(PLATFORM)/gtkmain.o \
	$(PLATFORM)/gtkdraw.o \
	$(PLATFORM)/gtkask.o \
	$(PLATFORM)/gtkletterask.o \
	$(PLATFORM)/gtkpasswdask.o \
	$(PLATFORM)/gtknewgame.o \
	$(PLATFORM)/gtkconnsdlg.o \
	$(PLATFORM)/gtkutils.o \
	$(PLATFORM)/gtkntilesask.o \
	$(PLATFORM)/gtkchat.o
endif
ifdef DO_CURSES
CURSES_OBJS = \
	$(PLATFORM)/cursesmain.o \
	$(PLATFORM)/cursesdraw.o \
	$(PLATFORM)/cursesask.o \
	$(PLATFORM)/cursesdlgutil.o \
	$(PLATFORM)/cursesletterask.o
endif
ifndef LIB_NO_UI
MAIN_OBJS = $(PLATFORM)/linuxmain.o
endif


OBJ = \
	$(PLATFORM)/filestream.o \
	$(PLATFORM)/linuxbt.o \
	$(PLATFORM)/linuxudp.o \
	$(PLATFORM)/linuxsms.o \
	$(PLATFORM)/linuxdict.o \
	$(PLATFORM)/linuxutl.o \
	$(CURSES_OBJS) $(GTK_OBJS) $(MAIN_OBJS)

LIBS = -lm -lmcheck $(GPROFFLAG)
ifdef BLUETOOTH
LIBS += -lbluetooth
endif

ifneq (,$(findstring DPLATFORM_GTK,$(DEFINES)))
	LIBS += `pkg-config --libs gtk+-2.0`
	CFLAGS += `pkg-config --cflags gtk+-2.0`
#   CFLAGS += -DGDK_DISABLE_DEPRECATED
	POINTER_SUPPORT = -DPOINTER_SUPPORT
endif

CFLAGS += $(POINTER_SUPPORT)

ifneq (,$(findstring DPLATFORM_NCURSES,$(DEFINES)))
	LIBS += $(OE_LIBDIR) -lncursesw
endif

# provides an all: target
include ../common/rules.mk

all: $(TARGET)

help:
	@echo "make [MEMDEBUG=TRUE] [CURSES_ONLY=TRUE] [GTK_ONLY=TRUE]"

#test:
#	$(MAKE) test1 DEFINES="$(DEFINES) -FOOBAR"

#test1:
#	echo $(findstring FOO,$(DEFINES))
#	echo $(DEFINES)

curses:
	$(MAKE) CURSES_ONLY=TRUE

gtk:
	$(MAKE) GTK_ONLY=TRUE

memdebug:
	$(MAKE) MEMDEBUG=TRUE

gprof:
	$(MAKE) GPROFFLAG=-pg MEMDEBUG=TRUE


$(PLATFORM)/xwords: $(COMMONOBJ) $(OBJ) *.h Makefile
	mkdir -p $(PLATFORM)
	$(CC) $(CFLAGS) $(DEFINES) $(COMMONOBJ) $(OBJ) $(LIBS) -o $@

$(PLATFORM)/libxwords.so.0: $(COMMONOBJ) $(OBJ) *.h Makefile
	mkdir -p $(PLATFORM)
	$(CC) $(CFLAGS) $(DEFINES) $(COMMONOBJ) $(OBJ) -shared -o $@  -Wl,-soname,libxwords.so.0

$(PLATFORM)/%.o: %.c
	mkdir -p $(PLATFORM)
	$(CC) -c $(INCLUDES) $(DEFINES) -DPLATFORM=$(PLATFORM) $(CFLAGS) $< -o $@

clean:
	rm -rf $(PLATFORM)/*.o $(TARGET) $(DESTDIR)/usr/local/bin/xwords
	cd ../common && $(MAKE) PLATFORM=$(PLATFORM) $@

install: $(TARGET)
	cp $< $(DESTDIR)/usr/local/bin

tarball:
	tar cvfz xwords_$(shell svnversion ..).tgz \
		../linux/Makefile ../linux/*.c ../linux/*.h \
		../relay/*.h \
		../common/*.c ../common/*.h ../common/rules.mk ../common/config.mk
	md5sum xwords_$(shell svnversion ..).tgz > xwords_$(shell svnversion ..).tgz.md5

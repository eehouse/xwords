# -*- mode: makefile; compile-command: "make MEMDEBUG=TRUE"; -*-
# Copyright 2002-2007 by Eric House (xwords@eehouse.org).  All rights
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
DEFINES = -DMEM_DEBUG -DDEBUG 
CFLAGS += -g $(GPROFFLAG) -Wall -Wunused-parameter -Wcast-align -Werror
CFLAGS += -DDEBUG_TS
PLATFORM = obj_linux_memdbg
else
DEFINES =
PLATFORM = obj_linux_rel
CFLAGS += -Os -Werror
endif

DO_CURSES = -DPLATFORM_NCURSES
ifdef CURSES_SMALL_SCREEN
DO_CURSES += -DCURSES_SMALL_SCREEN
endif
DO_GTK = -DPLATFORM_GTK

SVN_REV ?= "$(shell svnversion -n .)"
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

DEFINES += $(DO_CURSES) $(DO_GTK) $(SVNDEF)

TARGET=$(PLATFORM)/xwords

include ../common/config.mk

DEFINES += -DPLATFORM_LINUX -DKEY_SUPPORT -DKEYBOARD_NAV -DNODE_CAN_4
# DEFINES += -DSTUBBED_DICT
ifdef DO_GTK
DEFINES += -DXWFEATURE_SEARCHLIMIT
endif
DEFINES += -DFEATURE_TRAY_EDIT
#DEFINES += -DDRAW_WITH_PRIMITIVES

ifdef CURSES_CELL_HT
DEFINES += -DCURSES_CELL_HT=$(CURSES_CELL_HT)
endif
ifdef CURSES_CELL_WIDTH
DEFINES += -DCURSES_CELL_WIDTH=$(CURSES_CELL_WIDTH)
endif

# Bluetooth support
ifndef NO_BLUETOOTH
BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_L2CAP
endif
#BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_RFCOMM
# DEFINES += -DXWFEATURE_IR
DEFINES += ${BLUETOOTH}
DEFINES += -DXWFEATURE_RELAY

# Support device-to-device connection via UDP, e.g. using wifi on a
# LAN or where the host/server isn't behind a firewall.
DEFINES += -DXWFEATURE_IP_DIRECT

# Choose one of these.  RELAY_HEARTBEAT means relay (must be compiled
# with same -D) works with comms on heartbeat.  Works only with relay.
# COMMS_HEARTBEAT should work on any comms transport (even IR, but
# user experience will be very bad!).  Is particularly useful with BT.
# DEFINES += -DRELAY_HEARTBEAT
DEFINES += -DCOMMS_HEARTBEAT

# Let users pick the tiles going into their trays
#DEFINES += -DFEATURE_TRAY_EDIT
DEFINES += -DDONT_ABORT_ENGINE

DEFINES += -DPERIMETER_FOCUS

#-DDEBUG -DEIGHT_TILES

#GPROFFLAG = -pg

# INCLUDES += -I/usr/lib/glib/include
INCLUDES += ${EXTRAINCS}

OBJ =   $(PLATFORM)/linuxmain.o \
	$(PLATFORM)/linuxdict.o \
	$(PLATFORM)/gtkmain.o \
	$(PLATFORM)/gtkdraw.o \
	$(PLATFORM)/gtkask.o \
	$(PLATFORM)/gtkletterask.o \
	$(PLATFORM)/gtkpasswdask.o \
	$(PLATFORM)/gtknewgame.o \
	$(PLATFORM)/gtkntilesask.o \
	$(PLATFORM)/cursesmain.o \
	$(PLATFORM)/cursesdraw.o \
	$(PLATFORM)/cursesask.o \
	$(PLATFORM)/cursesdlgutil.o \
	$(PLATFORM)/cursesletterask.o \
	$(PLATFORM)/filestream.o \
	$(PLATFORM)/linuxbt.o \
	$(PLATFORM)/linuxudp.o \

#	$(PLATFORM)/linuxcommpipe.o \

LIBS = -lm -lmcheck $(GPROFFLAG)
ifdef BLUETOOTH
LIBS += -lbluetooth
endif

ifneq (,$(findstring DPLATFORM_GTK,$(DEFINES)))
	LIBS += `pkg-config --libs gtk+-2.0`
	CFLAGS += `pkg-config --cflags gtk+-2.0` -DPOINTER_SUPPORT \
		-DGDK_DISABLE_DEPRECATED
endif

ifneq (,$(findstring DPLATFORM_NCURSES,$(DEFINES)))
	LIBS += $(OE_LIBDIR) -lncurses 
endif

# provides an all: target
include ../common/rules.mk

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


$(TARGET): $(COMMONOBJ) $(OBJ) *.h Makefile
	mkdir -p $(PLATFORM)
	$(CC) $(CFLAGS) $(DEFINES) $(COMMONOBJ) $(OBJ) $(LIBS) -o $@

$(PLATFORM)/%.o: %.c
	mkdir -p $(PLATFORM)
	$(CC) -c $(INCLUDES) $(DEFINES) -DPLATFORM=$(PLATFORM) $(CFLAGS) $< -o $@

clean:
	rm -rf $(PLATFORM)/*.o $(TARGET) $(DESTDIR)/usr/local/bin/xwords
	cd ../common && $(MAKE) PLATFORM=$(PLATFORM) $@

install: $(TARGET)
	cp $< $(DESTDIR)/usr/local/bin

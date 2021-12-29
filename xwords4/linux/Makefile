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

BUILD_DIR ?= .

ifeq ($(MEMDEBUG),TRUE)
DEFINES = -DMEM_DEBUG -DDEBUG -DENABLE_LOGGING -DNUMBER_KEY_AS_INDEX 
DEFINES += -DCOMMS_CHECKSUM
DEFINES += -DLOG_COMMS_MSGNOS
DEFINES += -DDI_DEBUG
CFLAGS += -g $(GPROFFLAG) -Wall -Wunused-parameter -Wcast-align -Werror -O0
DEFINES += -DDEBUG_HASHING
CFLAGS += -DDEBUG_TS -rdynamic 
PLATFORM = obj_linux_memdbg
else
DEFINES =
PLATFORM = obj_linux_rel
# Not shipping this!  Always build with symbols etc
CFLAGS += -g $(GPROFFLAG) -Wall -Wunused-parameter -Wcast-align -Werror -O0
#CFLAGS += -Os -Werror -Wunused
endif

# Getting ready for gtk 3.x
CFLAGS += -DGTK_DISABLE_SINGLE_INCLUDES
CFLAGS += -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
CFLAGS += -DGSEAL_ENABLE

BUILD_PLAT_DIR = $(BUILD_DIR)/$(PLATFORM)

# CC = llvm-gcc
ifneq ($(shell which ccache),)
    CC := ccache $(CC)
    CXX := ccache $(CXX)
endif

DO_CURSES = -DPLATFORM_NCURSES
DO_CURSES += -DUSE_GLIBLOOP
ifdef CURSES_SMALL_SCREEN
DO_CURSES += -DCURSES_SMALL_SCREEN
endif
DO_GTK = -DPLATFORM_GTK
DO_GTK += -DXWFEATURE_BOARDWORDS
DO_GTK += -DUSE_CAIRO

# uncomment for standalone build
# STANDALONE = -DXWFEATURE_STANDALONE_ONLY

UNICODE ?= -DXWFEATURE_UNICODE
ifdef UNICODE
	CFLAGS += -std=gnu99
endif

SVN_REV ?= "$(shell ../scripts/gitversion.sh)"
SVNDEF = -D'SVN_REV=$(SVN_REV)'
USE_SQLITE = 1

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
TARGET=$(BUILD_PLAT_DIR)/libxwords.so.0
POINTER_SUPPORT = -DPOINTER_SUPPORT
else
TARGET=$(BUILD_PLAT_DIR)/xwords
endif

export BUILD_PLAT_DIR
include ../common/config.mk

DEFINES += -DPLATFORM_LINUX -DKEY_SUPPORT -DKEYBOARD_NAV \
	-DSCROLL_DRAG_THRESHHOLD=1 -DNUM_SAVED_ENGINE_MOVES=32
# DEFINES += -DSTUBBED_DICT
ifdef DO_GTK
DEFINES += -DXWFEATURE_SEARCHLIMIT
endif
DEFINES += -DFEATURE_TRAY_EDIT
DEFINES += -DXWFEATURE_RAISETILE
#DEFINES += -DDRAW_WITH_PRIMITIVES
DEFINES += -DXWFEATURE_CROSSHAIRS
DEFINES += -DXWFEATURE_CHAT
DEFINES += -DDISABLE_TILE_SEL
# DEFINES += -DSET_GAMESEED
DEFINES += -DTEXT_MODEL
DEFINES += -DXWFEATURE_WALKDICT
DEFINES += -DXWFEATURE_TESTPATSTR
DEFINES += -DXWFEATURE_WALKDICT_FILTER
DEFINES += -DXWFEATURE_DICTSANITY
DEFINES += -DHASH_STREAM 
DEFINES += -DRELAY_NAME_DEFAULT="\"localhost\""
DEFINES += -DXWFEATURE_TURNCHANGENOTIFY
#DEFINES += -DXWFEATURE_SCOREONEPASS
### Enable zero or one of these two ###
#DEFINES += -DXWFEATURE_TRAYUNDO_ALL
DEFINES += -DXWFEATURE_TRAYUNDO_ONE
DEFINES += -DXWFEATURE_BONUSALL
# DEFINES += -DXWFEATURE_BONUSALLHINT
DEFINES += -DXWFEATURE_HILITECELL
# allow change dict inside running game
DEFINES += -DXWFEATURE_CHANGEDICT
DEFINES += -DXWFEATURE_DEVID
DEFINES += -DXWFEATURE_COMMSACK
#DEFINES += -DXWFEATURE_ACTIVERECT
DEFINES += -DXWFEATURE_WIDER_COLS
DEFINES += -DCOMMS_XPORT_FLAGSPROC
DEFINES += -DINITIAL_CLIENT_VERS=3
DEFINES += -DCOMMON_LAYOUT
DEFINES += -DNATIVE_NLI
DEFINES += -DNLI_VERSION=1
# DEFINES += -DRELAY_VIA_HTTP

# MAX_ROWS controls STREAM_VERS_BIGBOARD and with it move hashing
DEFINES += -DMAX_ROWS=32

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
BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_L2CAP -DXW_BT_UUID=\"7be0d084-ff89-4d6d-9c78-594773a6f963\"

endif
#BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_RFCOMM
# DEFINES += -DXWFEATURE_IR
DEFINES += ${BLUETOOTH}
DEFINES += -DXWFEATURE_RELAY
DEFINES += -DXWFEATURE_SMS -DXWFEATURE_BASE64
DEFINES += -DXWFEATURE_DIRECTIP

# Robot can be made to think, to simulate for relay mostly
DEFINES += -DXWFEATURE_SLOW_ROBOT -DXWFEATURE_ROBOTPHONIES

DEFINES += -DXWFEATURE_DEVICE
DEFINES += -DXWFEATURE_KNOWNPLAYERS

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
	$(BUILD_PLAT_DIR)/gtkmain.o \
	$(BUILD_PLAT_DIR)/gtkboard.o \
	$(BUILD_PLAT_DIR)/gtkdraw.o \
	$(BUILD_PLAT_DIR)/gtkask.o \
	$(BUILD_PLAT_DIR)/gtkletterask.o \
	$(BUILD_PLAT_DIR)/gtkpasswdask.o \
	$(BUILD_PLAT_DIR)/gtknewgame.o \
	$(BUILD_PLAT_DIR)/gtkconnsdlg.o \
	$(BUILD_PLAT_DIR)/gtkinvit.o \
	$(BUILD_PLAT_DIR)/gtkutils.o \
	$(BUILD_PLAT_DIR)/gtkntilesask.o \
	$(BUILD_PLAT_DIR)/gtkaskdict.o \
	$(BUILD_PLAT_DIR)/gtkchat.o \
	$(BUILD_PLAT_DIR)/gtkkpdlg.o \

endif
ifdef DO_CURSES
CURSES_OBJS = \
	$(BUILD_PLAT_DIR)/cursesmain.o \
	$(BUILD_PLAT_DIR)/cursesboard.o \
	$(BUILD_PLAT_DIR)/cursesmenu.o \
	$(BUILD_PLAT_DIR)/curgamlistwin.o \
	$(BUILD_PLAT_DIR)/cursesdraw.o \
	$(BUILD_PLAT_DIR)/cursesask.o \
	$(BUILD_PLAT_DIR)/cursesdlgutil.o \
	$(BUILD_PLAT_DIR)/cursesletterask.o
endif
ifndef LIB_NO_UI
MAIN_OBJS = $(BUILD_PLAT_DIR)/linuxmain.o \
	$(BUILD_PLAT_DIR)/gsrcwrap.o
endif

OBJ = \
	$(BUILD_PLAT_DIR)/filestream.o \
	$(BUILD_PLAT_DIR)/linuxbt.o \
	$(BUILD_PLAT_DIR)/uuidhack.o \
	$(BUILD_PLAT_DIR)/linuxudp.o \
	$(BUILD_PLAT_DIR)/linuxsms.o \
	$(BUILD_PLAT_DIR)/linuxdict.o \
	$(BUILD_PLAT_DIR)/linuxutl.o \
	$(BUILD_PLAT_DIR)/gamesdb.o \
	$(BUILD_PLAT_DIR)/relaycon.o \
	$(BUILD_PLAT_DIR)/mqttcon.o \
	$(BUILD_PLAT_DIR)/lindutil.o \
	$(CURSES_OBJS) $(GTK_OBJS) $(MAIN_OBJS)

LIBS = -lm -lpthread -luuid -lcurl -ljson-c $(GPROFFLAG)
ifdef USE_SQLITE
LIBS += -lsqlite3
LIBS += -lmosquitto
DEFINES += -DUSE_SQLITE
endif
# Turn this off for now.  I apparently have a memory problem, but it
# doesn't make the app unusable for testing while crash on boot does.
# LIBS += -lmcheck

ifdef BLUETOOTH
LIBS += -lbluetooth
endif

ifneq (,$(findstring DPLATFORM_GTK,$(DEFINES)))
	LIBS += `pkg-config --libs gtk+-3.0`
	CFLAGS += `pkg-config --cflags gtk+-3.0`
	CFLAGS += -DGDK_DISABLE_DEPRECATED
	POINTER_SUPPORT = -DPOINTER_SUPPORT
endif

CFLAGS += `pkg-config --cflags glib-2.0`
LIBS += `pkg-config --libs glib-2.0`

CFLAGS += $(POINTER_SUPPORT)
CFLAGS += -DDROP_BITMAPS

ifneq (,$(findstring DPLATFORM_NCURSES,$(DEFINES)))
	LIBS += $(OE_LIBDIR) -lncursesw
endif

# provides an all: target
include ../common/rules.mk

REQUIRED_DEBS = gcc libgtk-3-dev \
		libbluetooth-dev \
		libncurses5-dev \
		libncursesw5-dev \
		uuid-dev \
		libsqlite3-dev \
		libcurl4-openssl-dev \
		libjson-c-dev \
		libgtk-3-dev \
		python3-requests \
		libmosquitto-dev \

.PHONY: debcheck debs_install

debs_install:
	sudo apt-get install $(REQUIRED_DEBS)

debcheck:
	@if which dpkg; then \
		for DEB in $(REQUIRED_DEBS); do \
			if ! dpkg -l $$DEB >/dev/null 2>&1; then \
				echo "$$DEB not installed"; \
				echo "try running 'make debs_install'"; \
				break; \
			fi \
		done; \
	fi

all: debcheck $(TARGET)

dawg2dict: $(TARGET)
	ln -sf $< $@

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


$(BUILD_PLAT_DIR)/xwords: $(COMMONOBJ) $(OBJ) *.h Makefile debcheck
	mkdir -p $(BUILD_PLAT_DIR)
	$(CC) $(CFLAGS) $(DEFINES) $(COMMONOBJ) $(OBJ) $(LIBS) -o $@

$(BUILD_PLAT_DIR)/libxwords.so.0: $(COMMONOBJ) $(OBJ) *.h Makefile
	mkdir -p $(BUILD_PLAT_DIR)
	$(CC) $(CFLAGS) $(DEFINES) $(COMMONOBJ) $(OBJ) -shared -o $@  \
		-Wl,-soname,libxwords.so.0

$(BUILD_PLAT_DIR)/%.o: %.c debcheck
	mkdir -p $(BUILD_PLAT_DIR)
	$(CC) -c $(INCLUDES) $(DEFINES) -DPLATFORM=$(PLATFORM) $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_PLAT_DIR)/*.o $(TARGET) $(DESTDIR)/usr/local/bin/xwords
	rm -f $(COMMONOBJ)

install: $(TARGET)
	cp $< $(DESTDIR)/usr/local/bin

tarball:
	tar cvfz xwords_$(shell svnversion ..).tgz \
		../linux/Makefile ../linux/*.c ../linux/*.h \
		../relay/*.h \
		../common/*.c ../common/*.h ../common/rules.mk ../common/config.mk
	md5sum xwords_$(shell svnversion ..).tgz > xwords_$(shell svnversion ..).tgz.md5

# -*- mode: makefile -*-
# Copyright 2002 by Eric House (fixin@peak.org).  All rights reserved.
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

PLATFORM=linux
TARGET=$(PLATFORM)/xwords
CC = gcc

include ../common/config.mk

DEFINES = -DPLATFORM_LINUX -DKEY_SUPPORT -DKEYBOARD_NAV -DNODE_CAN_4
DEFINES += -DSTUBBED_DICT
# build with GTK
DEFINES += -DPLATFORM_GTK
# build with ncurses (-u flag chooses over gtk if both present)
DEFINES += -DPLATFORM_NCURSES
#DEFINES += -DDRAW_WITH_PRIMITIVES
DEFINES += -DBEYOND_IR

# Let users pick the tiles going into their trays
#DEFINES += -DFEATURE_TRAY_EDIT

#-DDEBUG -DEIGHT_TILES

#GPROFFLAG = -pg

# INCLUDES += -I/usr/lib/glib/include
INCLUDES += -I$(HOME)/usr/local/include/ -I/home/ehouse/usr/local/lib/glib/include -I$(HOME)/usr/local/pilot/include

OBJ =   $(PLATFORM)/linuxmain.o \
	$(PLATFORM)/linuxdict.o \
	$(PLATFORM)/gtkmain.o \
	$(PLATFORM)/gtkdraw.o \
	$(PLATFORM)/gtkask.o \
	$(PLATFORM)/gtkletterask.o \
	$(PLATFORM)/gtkpasswdask.o \
	$(PLATFORM)/gtknewgame.o \
	$(PLATFORM)/cursesmain.o \
	$(PLATFORM)/cursesdraw.o \
	$(PLATFORM)/cursesask.o \
	$(PLATFORM)/cursesdlgutil.o \
	$(PLATFORM)/cursesletterask.o \
	$(PLATFORM)/filestream.o \

#	$(PLATFORM)/linuxcommpipe.o \

LIBS = -lm -L $(HOME)/usr/local/pilot/lib $(GPROFFLAG)
# -lprc 
# -pg
CFLAGS = -g -Wall -O2 $(GPROFFLAG)

ifneq (,$(findstring DPLATFORM_GTK,$(DEFINES)))
# 	LIBS += `pkg-config --libs gtk+-2.0`
# 	CFLAGS += `pkg-config --cflags gtk+-2.0` -DPOINTER_SUPPORT
	LIBS += $$(gtk-config --libs) 
	CFLAGS += `gtk-config --cflags` -DPOINTER_SUPPORT
endif

ifneq (,$(findstring DPLATFORM_NCURSES,$(DEFINES)))
	LIBS += -lncurses
endif

ifeq (,$(findstring -DCLIENT_ONLY,$(DEFINES)))
# 	LIBS += -lprc
endif

# provides an all: target
include ../common/rules.mk

help:
	@echo -n "make debug | curses_debug | gtk_debug | "
	@echo "curses_client_debug | gtk_client_debug"

#test:
#	$(MAKE) test1 DEFINES="$(DEFINES) -FOOBAR"

#test1:
#	echo $(findstring FOO,$(DEFINES))
#	echo $(DEFINES)

curses_client_debug:
	$(MAKE) $(TARGET) DEFINES="$(DEFINES) -DDEBUG -DPLATFORM_NCURSES -DCLIENT_ONLY"

curses_debug:
	$(MAKE) $(TARGET) DEFINES="$(DEFINES) -DDEBUG -DPLATFORM_NCURSES"

gtk_client_debug:
	$(MAKE) $(TARGET) DEFINES="$(DEFINES) -DDEBUG -DPLATFORM_GTK -DCLIENT_ONLY"

gtk_debug:
	$(MAKE) $(TARGET) DEFINES="$(DEFINES) -DDEBUG -DPLATFORM_GTK"

debug:
	$(MAKE) $(TARGET) DEFINES="$(DEFINES) -DDEBUG -DPLATFORM_NCURSES -DPLATFORM_GTK"

memdebug:
	$(MAKE) $(TARGET) DEFINES="$(DEFINES) -DDEBUG -DPLATFORM_NCURSES -DPLATFORM_GTK -DMEM_DEBUG"

gprof:
	$(MAKE) $(TARGET) GPROFFLAG=-pg DEFINES="$(DEFINES) -DPLATFORM_NCURSES -DPLATFORM_GTK"

$(TARGET): $(COMMONOBJ) $(OBJ) *.h Makefile
	@ls -d $(PLATFORM)  >/dev/null || mkdir $(PLATFORM)
	$(CC) $(CFLAGS) $(DEFINES) $(COMMONOBJ) $(OBJ) $(LIBS) -o $@

$(PLATFORM)/%.o: %.c
	@ls -d $(PLATFORM) >/dev/null || mkdir $(PLATFORM)
	$(CC) -c $(INCLUDES) $(DEFINES) -DPLATFORM=$(PLATFORM) $(CFLAGS) $< -o $@

clean:
	rm -rf $(PLATFORM)/*.o $(TARGET)
	cd ../common && $(MAKE) PLATFORM=$(PLATFORM) $@

# -*- mode: makefile -*-

# Copyright 2002 by Eric House
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

COMMON_INCS = -I ../common -I../relay
INCLUDES += $(COMMON_INCS) -I./

COMMONDIR ?= ../common
COMMONOBJDIR = $(BUILD_PLAT_DIR)/common

COMMON1 = \
	$(COMMONOBJDIR)/board.o \
	$(COMMONOBJDIR)/boarddrw.o \
	$(COMMONOBJDIR)/tray.o \
	$(COMMONOBJDIR)/scorebdp.o \
	$(COMMONOBJDIR)/draw.o \

COMMON2 = \
	$(COMMONOBJDIR)/model.o \
	$(COMMONOBJDIR)/mscore.o \
	$(COMMONOBJDIR)/server.o \
	$(COMMONOBJDIR)/pool.o \

COMMON3 = \
	$(COMMONOBJDIR)/game.o \
	$(COMMONOBJDIR)/nwgamest.o \
	$(COMMONOBJDIR)/dictnry.o \
	$(COMMONOBJDIR)/dictiter.o \
	$(COMMONOBJDIR)/engine.o \

COMMON4 = \
	$(COMMONOBJDIR)/dragdrpp.o \
	$(COMMONOBJDIR)/memstream.o \
	$(COMMONOBJDIR)/comms.o \
	$(COMMONOBJDIR)/xwmutex.o \
	$(COMMONOBJDIR)/nli.o \
	$(COMMONOBJDIR)/mempool.o \

COMMON5 = \
	$(COMMONOBJDIR)/movestak.o \
	$(COMMONOBJDIR)/strutils.o \
	$(COMMONOBJDIR)/bufqueue.o \
	$(COMMONOBJDIR)/vtabmgr.o \
	$(COMMONOBJDIR)/dictmgr.o \
	$(COMMONOBJDIR)/dbgutil.o \
	$(COMMONOBJDIR)/smsproto.o \
	$(COMMONOBJDIR)/dutil.o \
	$(COMMONOBJDIR)/device.o \
	$(COMMONOBJDIR)/knownplyr.o \
	$(COMMONOBJDIR)/dllist.o \
	$(COMMONOBJDIR)/stats.o \
	$(COMMONOBJDIR)/timers.o \
	$(COMMONOBJDIR)/md5.o \

CJSON = \
	$(COMMONOBJDIR)/cJSON.o \
	$(COMMONOBJDIR)/cJSON_Utils.o \

COMMONOBJ = $(COMMON1) $(COMMON2) $(COMMON3) $(COMMON4) $(COMMON5) $(CJSON)

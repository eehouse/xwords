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

COMMON_INCS = -I ./$(PLATFORM) -I../common -I../relay
INCLUDES += $(COMMON_INCS) -I./

COMMONDIR ?= ../common
COMMONOBJDIR = ../common/$(PLATFORM)

COMMONSRC = \
	$(COMMONDIR)/board.c \
	$(COMMONDIR)/boarddrw.c \
	$(COMMONDIR)/dragdrpp.c \
	$(COMMONDIR)/scorebdp.c \
	$(COMMONDIR)/tray.c \
	$(COMMONDIR)/draw.c \
	$(COMMONDIR)/model.c \
	$(COMMONDIR)/mscore.c \
	$(COMMONDIR)/server.c \
	$(COMMONDIR)/pool.c \
	$(COMMONDIR)/game.c \
	$(COMMONDIR)/nwgamest.c \
	$(COMMONDIR)/dictnry.c \
	$(COMMONDIR)/engine.c \
	$(COMMONDIR)/memstream.c \
	$(COMMONDIR)/comms.c \
	$(COMMONDIR)/mempool.c \
	$(COMMONDIR)/movestak.c \
	$(COMMONDIR)/strutils.c \
	$(COMMONDIR)/bufqueue.c \
	$(COMMONDIR)/vtabmgr.c \
	$(COMMONDIR)/dbgutil.c \

# PENDING: define this in terms of above!!!

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
	$(COMMONOBJDIR)/engine.o \

COMMON4 = \
	$(COMMONOBJDIR)/dragdrpp.o \
	$(COMMONOBJDIR)/memstream.o \
	$(COMMONOBJDIR)/comms.o \
	$(COMMONOBJDIR)/mempool.o \

COMMON5 = \
	$(COMMONOBJDIR)/movestak.o \
	$(COMMONOBJDIR)/strutils.o \
	$(COMMONOBJDIR)/bufqueue.o \
	$(COMMONOBJDIR)/vtabmgr.o \
	$(COMMONOBJDIR)/dbgutil.o \

COMMONOBJ = $(COMMON1) $(COMMON2) $(COMMON3) $(COMMON4) $(COMMON5)

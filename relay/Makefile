# -*- mode: Makefile; -*-
# Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

CXX = g++-4.1
CC=$(CXX)
SRC = xwrelay.cpp \
	cref.cpp \
	ctrl.cpp \
	tpool.cpp \
	states.cpp \
	timermgr.cpp \
	configs.cpp \
	crefmgr.cpp \
	permid.cpp \

# STATIC ?= -static

OBJ = $(patsubst %.cpp,%.o,$(SRC))
LDFLAGS += -pthread -g -lmcheck $(STATIC)
CPPFLAGS += -DSPAWN_SELF -g -Wall -DSVN_REV=\"$(shell svnversion -n .)\"
# turn on semaphore debugging
# CPPFLAGS += -DDEBUG_LOCKS

memdebug all: xwrelay

xwrelay: $(OBJ)

clean:
	rm -f xwrelay $(OBJ)

tags:
	etags *.cpp *.h

tarball:
	tar cvfz RELAY_SRC.tgz \
		../relay/*.{cpp,h} \
		../relay/Makefile \
		../relay/xwrelay.conf

help:
	@echo $(MAKE) [STATIC=\"-static\"]
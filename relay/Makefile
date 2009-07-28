# -*- mode: Makefile; -*-
# Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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

CXX = g++
CC=$(CXX)
SRC = xwrelay.cpp \
	cref.cpp \
	ctrl.cpp \
	http.cpp \
	tpool.cpp \
	states.cpp \
	timermgr.cpp \
	configs.cpp \
	crefmgr.cpp \
	permid.cpp \
	lstnrmgr.cpp \

# STATIC ?= -static
SVNINFO = svnversion.txt


OBJ = $(patsubst %.cpp,%.o,$(SRC))
#LDFLAGS += -pthread -g -lmcheck $(STATIC)
LDFLAGS += -pthread -g $(STATIC)
CPPFLAGS += -DSPAWN_SELF -DRELAY_HEARTBEAT -DDO_HTTP -g -Wall \
	-DSVN_REV=\"$(shell cat $(SVNINFO) 2>/dev/null || svnversion -n ..)\"

# turn on semaphore debugging
# CPPFLAGS += -DDEBUG_LOCKS

memdebug all: xwrelay

xwrelay: $(OBJ)

clean:
	rm -f xwrelay $(OBJ)

tags:
	etags *.cpp *.h

tarball:
	@svnversion -n ../ > $(SVNINFO)
	tar cvfz RELAY_SRC.tgz \
		../relay/*.cpp \
		../relay/*.h \
		../relay/Makefile \
		../relay/xwrelay.conf \
		../relay/xwrelay.sh \
		../relay/xwrelay.css \
		../relay/$(SVNINFO)
	@rm -f $(SVNINFO)
	@echo "RELAY_SRC.tgz done"

help:
	@echo $(MAKE) [STATIC=\"-static\"]
	@echo $(MAKE) tarball

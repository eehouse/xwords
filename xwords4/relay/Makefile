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
SRC = \
	configs.cpp \
	cref.cpp \
	crefmgr.cpp \
	dbmgr.cpp \
	http.cpp \
	lstnrmgr.cpp \
	permid.cpp \
	states.cpp \
	timermgr.cpp \
	tpool.cpp \
	cidlock.cpp \
	xwrelay.cpp \

# STATIC ?= -static
GITINFO = gitversion.txt
HASH=$(shell git describe)

OBJ = $(patsubst %.cpp,%.o,$(SRC))
#LDFLAGS += -pthread -g -lmcheck $(STATIC)
LDFLAGS += -pthread -g $(STATIC) \
	-L$(shell pg_config --libdir) -lpq

CPPFLAGS += -DSPAWN_SELF -g -Wall \
	-I $(shell pg_config --includedir) \
	-DSVN_REV=\"$(shell cat $(GITINFO) 2>/dev/null || echo -n $(HASH) )\"
# CPPFLAGS += -DDO_HTTP

# turn on semaphore debugging
# CPPFLAGS += -DDEBUG_LOCKS

memdebug all: xwrelay rq

xwrelay: $(OBJ)

rq: rq.c

clean:
	rm -f xwrelay $(OBJ) rq

tags:
	etags *.cpp *.h

tarball:
	@echo -n $(HASH) > $(GITINFO)
	tar cvfz RELAY_SRC.tgz \
		../relay/*.cpp \
		../relay/*.h \
		../relay/Makefile \
		../relay/xwrelay.conf \
		../relay/xwrelay.sh \
		../relay/xwrelay.css \
		../relay/$(GITINFO)
	@rm -f $(GITINFO)
	@echo "RELAY_SRC.tgz done"

help:
	@echo $(MAKE) [STATIC=\"-static\"]
	@echo $(MAKE) tarball

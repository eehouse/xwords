# -*- mode: Makefile; -*-
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

PLATFORM=palm
LANG=en_US
TARGET=$(PLATFORM)/xwords4.prc
ROOTNAME = Crosswords
BITMAPS = ./bmps
NAME = "Crosswords"
TYPE = appl
ICONTEXT = "Crosswords"
APPID = Xwr4
MLPREFIX = /usr
DICT = $(shell echo $$BASENG_PATH)

#OWNERNAME = "Eric House"
NO_REG_REQUIRED = -DNO_REG_REQUIRED

PALM_TOOLS_PREFIX = $(shell echo $$PALM_TOOLS_PREFIX)

ifeq (x$(PALM_TOOLS_PREFIX)x, xx)
	PALM_TOOLS_PREFIX = m68k-palmos-
endif
#PALM_TOOLS_PREFIX=m68k-palmos-coff-

CC = $(PALM_TOOLS_PREFIX)gcc
AR = $(PALM_TOOLS_PREFIX)ar
MULTILINK = $(PALM_TOOLS_PREFIX)multilink
PAR = par
PILRC = pilrc

MULTILINK_OPTIONS = -basename $(ROOTNAME) -segmentsize 27k -g \
	-deadstrip -verbose -gdb-script app.gdb

ifneq (x$(OWNERNAME)x,xx)
HASHDEF = -DOWNER_HASH=$$(./namehash $(OWNERNAME))
endif

MYDEFINES = -DXW_FEATURE_UTILS -DPOINTER_SUPPORT -DKEY_SUPPORT \
	-DOVERRIDE_EDGE_FOR_INDEX -DCOLOR_SUPPORT \
	-DSHOW_PROGRESS \
	$(HASHDEF) $(NO_REG_REQUIRED)

# turn on letting users pick tiles "face-up"
MYDEFINES += -DFEATURE_TRAY_EDIT

# Turn on network play over IP via cellular modem.  Very much
# experimental at this point!
# MYDEFINES += -DBEYOND_IR

# Turn on ability to handle 4-byte-node DAWG files.  These are of type
# Xwr4, not Xwr3 like the old ones.  Currently this option means the
# binary won't be able to read or even find the old dictionaries.
# Don't release with this on until there's a UI decision and code on
# how to help users transition.
MYDEFINES += -DNODE_CAN_4

ifdef XWFEATURE_STANDALONE_ONLY
MYDEFINES += -DXWFEATURE_STANDALONE_ONLY
else
MYDEFINES += -DIR_SUPPORT -DIR_EXCHMGR
endif
#	  -DEIGHT_TILES
# -DSUPPORT_SONY_JOGDIAL

# HS_DUO_SUPPORT = 1

DEFINES += -DPLATFORM_PALM -D__BIG_ENDIAN $(MYDEFINES)

BITMAP_RSRCS = \
	$(BITMAPS)/rightarrow.pbitm \
	$(BITMAPS)/downarrow.pbitm \
	$(BITMAPS)/flipbutton.pbitm \
	$(BITMAPS)/valuebutton.pbitm \
	$(BITMAPS)/lightbulb.pbitm \
	$(BITMAPS)/traybuttons.pbitm \
	$(BITMAPS)/showtray.pbitm \
	$(BITMAPS)/xwords4.pbitm \
	$(BITMAPS)/xwcoloricon.ppm \
	$(BITMAPS)/xwords4small.pbitm \

INCLUDES += -I/usr/local/share/palmdev/sdk-4.0/Extensions/ExpansionMgr

ifneq (x$(HS_DUO_SUPPORT)x, xx)
	INCLUDES += -I/usr/local/share/palmdev/duoIncs 
	INCLUDES += -I/usr/local/share/palmdev/duoIncs/68K
	INCLUDES += -I/usr/local/share/palmdev/duoIncs/68K/System 
	INCLUDES += -I/usr/local/share/palmdev/duoIncs/Common/System 

	MYDEFINES += -DHS_DUO_SUPPORT

	FNAVS = ./fnav03e9.bin
endif


CSFLAGS = -O2 -g -S -Wall -DAPPID=\'$(APPID)\' $(DEFINES) $(INCLUDES)
#CFLAGS = -O2 -g -Wall -DAPPID=\'$(APPID)\' $(DEFINES) $(INCLUDES) -palmos3.5
CFLAGS = -O2 -g -Wall -DAPPID=\'$(APPID)\' $(DEFINES) $(INCLUDES) 


# In the non-debug mode (for which DONT_OMIT is undefined) build without
# the frame pointer.
ifneq ($(DONT_OMIT), true)
	CSFLAGS += -fomit-frame-pointer
	CFLAGS += -fomit-frame-pointer
endif

include ../common/config.mk


OBJS = 	$(PLATFORM)/palmmain.o \
	$(PLATFORM)/palmsavg.o \
	$(PLATFORM)/gameutil.o \
	$(PLATFORM)/newgame.o \
	$(PLATFORM)/palmdict.o \
	$(PLATFORM)/palmdraw.o \
	$(PLATFORM)/palmutil.o \
	$(PLATFORM)/dictui.o \
	$(PLATFORM)/dictlist.o \
	$(PLATFORM)/palmir.o \
	$(PLATFORM)/prefsdlg.o \
	$(PLATFORM)/connsdlg.o \
	$(COMMONOBJ)

include ../common/rules.mk

$(TARGET): $(PLATFORM)/objs.prc $(PLATFORM)/res.prc 
	$(PAR) -c -a 'resource|backup' $@ $(NAME) $(TYPE) $(APPID) $^
	if [ x$(shell echo $$XW_UPLOAD_DIR) != x ]; then \
		zip -j xw4.zip $@; \
		mv xw4.zip $(shell echo $$XW_UPLOAD_DIR); \
		cp $@ $(shell echo $$XW_UPLOAD_DIR); \
	fi

solo:
	$(MAKE) XWFEATURE_STANDALONE_ONLY=1

debug:
	$(MAKE) MYDEFINES="$(MYDEFINES) -DDEBUG" DONT_OMIT=true

memdebug:
	$(MAKE) MYDEFINES="$(MYDEFINES) -DDEBUG -DMEM_DEBUG" DONT_OMIT=true

gremlins:
	$(MAKE) MYDEFINES="$(MYDEFINES) -DDEBUG -DMEM_DEBUG -DFOR_GREMLINS" DONT_OMIT=true

#
REL=405
REL_PATH=public_html/xwords/4.0.5
ship-all:
	make clean; make; \
	make; (cd xwconfig && make); \
	for l in fr_FR en_US es_ES es_CT sv_SE de_DE ; do \
		make clean; \
		make LANG=$$l; \
		zip -j xw$(REL)_$$l.zip xwconfig/xwconfig.prc $(TARGET); \
	done

.S.o:
	$(CC) $(TARGETFLAGS) -c $<

.c.s:
	$(CC) $(CSFLAGS) $<

$(BITMAPS)/%.pbitm: $(BITMAPS)/%.bmp
	bmtoa $< > $@

$(PLATFORM)/objs.prc: LocalizedStrIncludes.h $(OBJS) gdbload
	@rm -f *.grc *.bin
	$(MULTILINK) $(MULTILINK_OPTIONS) $(OBJS)
	$(PAR) -c -a resource $(PLATFORM)/objs.prc Code rsrc rsrc *.grc
	@rm -f *.grc *.bin

$(PLATFORM)/res.prc: xwords4.rcp $(HEADERS) $(CODESEG) StrL03e8.bin $(FNAVS)
	$(PILRC) $< >/dev/null
	$(PAR) -c -a 'resource' $@ Rsrc rsrc rsrc *.bin
	rm -f $< *.bin

xwords4.rcp: l10n/xwords4_$(LANG).rcp.pre xwords4defines.h $(BITMAP_RSRCS)
	gcc -x c -E -P $(INCLUDES) $(MYDEFINES) \
		-DICONTEXT=\"$(ICONTEXT)\" $< > $@

LocalizedStrIncludes.h StrL03e8.bin: ./l10n/StrRes_$(LANG).pre ./l10n/mkstrsres.c 
	gcc $(CFLAGS) $(FORMATDEFINES) \
		-DLANGSTRFILE=\"$<\" ./l10n/mkstrsres.c \
		-o mkstrsres
	./mkstrsres StrL03e8.bin LocalizedStrIncludes.h
	rm -f mkstrsres

namehash: namehash.c ownerhash.h
fnavgen: fnavgen.c
	gcc $< -o $@

$(FNAVS): ./fnavgen
	./$<

# GDB seems confused by relative paths these days.  So generate the
# file rather than trying to keep in in cvs.
gdbload:
	echo "source app.gdb" > $@
	echo "load-segments" >> $@
	echo "dir $(shell pwd)" >> $@
	echo "dir $(shell pwd)/../common" >> $@

clean:
	cd ../common && $(MAKE) PLATFORM=$(PLATFORM) $@
	rm -rf $(PLATFORM)/*.[oa] xwords4 *.bin *.stamp *.[pg]rc \
		xwords4.rcp *.btxt $(PLATFORM)/* $(CODESEG) \
		LocalizedStrIncludes.h gdbload

#cmod03E8.bin: palmdraw.c Makefile
#	$(CC) -O2 -nostartfiles $(INCLUDES) $(MYDEFINES) -o tmp $<
#	$(OBJRES) tmp
#	mv code0001.tmp.grc $@ ; rm *.tmp.grc

help:
	@echo make memdebug OR
	@echo make LANG=en_US OR
	@echo make ship-all OR
	@echo make gremlins OR
	@echo make clean

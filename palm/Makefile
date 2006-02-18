# -*- mode: Makefile; compile-command: "make ARCH=COMBINED"; -*-
# Copyright 2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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
ROOTNAME = Crosswords
BITMAPS = ./bmps
NAME ?= "Crosswords"
TYPE = appl
ICONTEXT ?= "Crosswords"
APPID ?= Xwr4
MLPREFIX = /usr
DICT = $(shell echo $$BASENG_PATH)

ARCH ?= ARM_ONLY

ifeq ($(ARCH),ARM_ONLY)
BUILD_AS_PNOLET = 1
FEATURE_HIGHRES = -DFEATURE_HIGHRES
TARGET=$(PLATFORM)/xw4_ARM.prc
else
ifeq ($(ARCH),68K_ONLY)
FEATURE_HIGHRES = -DFEATURE_HIGHRES
TARGET=$(PLATFORM)/xw4_68K.prc
else
ifeq ($(ARCH),SONY)
# No highres in sony build case
BUILD_AS_PNOLET = 1
FEATURE_PNOAND68K = -DFEATURE_PNOAND68K
TARGET=$(PLATFORM)/xw4_SONY.prc
else
ifeq ($(ARCH),COMBINED)
BUILD_AS_PNOLET = 1
FEATURE_PNOAND68K = -DFEATURE_PNOAND68K
FEATURE_HIGHRES = -DFEATURE_HIGHRES
TARGET=$(PLATFORM)/xwords4.prc
endif
endif
endif
endif

DEBUG_SET = APPID=Xwdb ICONTEXT="CrossDbg" NAME="CrossDbg" DONT_OMIT=true

APPNAME = "$(NAME)"

PALM_TOOLS_PREFIX = $(shell echo $$PALM_TOOLS_PREFIX)

ifeq (x$(PALM_TOOLS_PREFIX)x, xx)
	PALM_TOOLS_PREFIX = m68k-palmos-
endif
#PALM_TOOLS_PREFIX=m68k-palmos-coff-

CC = $(PALM_TOOLS_PREFIX)gcc
AR = $(PALM_TOOLS_PREFIX)ar
MULTILINK = $(PALM_TOOLS_PREFIX)multilink
###############################################################################
# par is a utility for manipulating .prc and .pdb files.  See
# http://djw.org/product/palm/par/index.html to download source.
###############################################################################
PAR = par
PILRC = pilrc
BUILD-PRC = build-prc
OBJ-RES = m68k-palmos-obj-res

MULTILINK_OPTIONS = -basename $(ROOTNAME) -segmentsize 27k -g \
	-deadstrip -verbose -gdb-script app.gdb

ifneq (x$(OWNERNAME)x,xx)
HASHDEF = -DOWNER_HASH=$$(./namehash $(OWNERNAME))
endif

MYDEFS_COMMON = $(PASSTHRU) -DXW_FEATURE_UTILS -DPOINTER_SUPPORT \
	-DKEY_SUPPORT 	-DOVERRIDE_EDGE_FOR_INDEX -DCOLOR_SUPPORT \
	-DSHOW_PROGRESS $(HASHDEF) -DNO_REG_REQUIRED

# turn on letting users pick tiles "face-up"
MYDEFS_COMMON += -DFEATURE_TRAY_EDIT

# turn on limits to searches on board and in tray (number of tiles to
# use)
MYDEFS_COMMON += -DXWFEATURE_SEARCHLIMIT

#MYDEFS_COMMON += -DXWFEATURE_HINT_CONFIG

# Turn on network play over IP via cellular modem.  Very much
# experimental at this point!
MYDEFS_COMMON += -DBEYOND_IR

# For Danish and perhaps other languages, custom-measure glyph height
# so that overtall letters have a chance of fitting.
MYDEFS_COMMON += -DTALL_FONTS

# Turn on ability to handle 4-byte-node DAWG files.  These are of type
# Xwr4, not Xwr3 like the old ones.  Currently this option means the
# binary won't be able to read or even find the old dictionaries.
# Don't release with this on until there's a UI decision and code on
# how to help users transition.
MYDEFS_COMMON += -DNODE_CAN_4

ifdef XWFEATURE_STANDALONE_ONLY
MYDEFS_COMMON += -DXWFEATURE_STANDALONE_ONLY
else
MYDEFS_COMMON += -DIR_SUPPORT -DIR_EXCHMGR
endif

MYDEFS_COMMON += $(FEATURE_HIGHRES)

MYDEFS_ARM = -D__LITTLE_ENDIAN -DXW_TARGET_PNO $(MYDEFS_COMMON)
MYDEFS_68K = -DPLATFORM_PALM -D__BIG_ENDIAN $(MYDEFS_COMMON) -DAPPNAME=\"$(APPNAME)\"

BITMAP_RSRCS = \
	$(BITMAPS)/rightarrow.pbitm \
	$(BITMAPS)/downarrow.pbitm \
	$(BITMAPS)/flipbutton.pbitm \
	$(BITMAPS)/valuebutton.pbitm \
	$(BITMAPS)/lightbulb.pbitm \
	$(BITMAPS)/traybuttons.pbitm \
	$(BITMAPS)/showtray.pbitm \
	$(BITMAPS)/xwords4.pbitm \
	$(BITMAPS)/xwords4small.pbitm \

INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/Extensions/ExpansionMgr

# HS_DUO_SUPPORT = 1

ifneq (x$(HS_DUO_SUPPORT)x, xx)
	INCLUDES += -I/usr/local/share/palmdev/duoIncs 
	INCLUDES += -I/usr/local/share/palmdev/duoIncs/68K
	INCLUDES += -I/usr/local/share/palmdev/duoIncs/68K/System 
	INCLUDES += -I/usr/local/share/palmdev/duoIncs/Common/System 

	MYDEFS_68K += -DHS_DUO_SUPPORT
	MYDEFS_ARM += -DHS_DUO_SUPPORT

	FNAVS = ./fnav03e9.bin
endif

ifneq (x$(FEATURE_HIGHRES)x, xx)
	INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/include/SonyIncs
	INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/include/SonyIncs/System
	INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/include/SonyIncs/Libraries
	MYDEFS_68K += -DCPU_TYPE=CPU_68K	
endif

include ../common/config.mk

OBJS_68K = $(PLATFORM)/palmmain.o \
	$(PLATFORM)/palmsavg.o \
	$(PLATFORM)/gameutil.o \
	$(PLATFORM)/newgame.o \
	$(PLATFORM)/palmdict.o \
	$(PLATFORM)/palmdraw.o \
	$(PLATFORM)/palmutil.o \
	$(PLATFORM)/dictui.o \
	$(PLATFORM)/dictlist.o \
	$(PLATFORM)/palmir.o \
	$(PLATFORM)/palmip.o \
	$(PLATFORM)/prefsdlg.o \
	$(PLATFORM)/connsdlg.o \
	$(COMMONOBJ)

# if BUILD_AS_PNOLET is defined and FEATURE_PNOAND68K isn't, exclude
# all but enter68k.o from the 68K part of the binary (code resources).
ifdef BUILD_AS_PNOLET
	OBJS = $(PLATFORM)/enter68k.o
	PNOLET = pnolet
endif
ifndef BUILD_AS_PNOLET
	OBJS += $(OBJS_68K)
else
ifdef FEATURE_PNOAND68K
	OBJS += $(OBJS_68K)
endif
endif

MYDEFS_68K += $(FEATURE_PNOAND68K)
MYDEFS_ARM += $(FEATURE_PNOAND68K)

include ../common/rules.mk

CSFLAGS = -O2 -g -S -Wall -DAPPID=\'$(APPID)\' $(MYDEFS_68K) $(INCLUDES)
CFLAGS = -O2 -g -Wall -DAPPID=\'$(APPID)\' $(MYDEFS_68K) $(INCLUDES) 
# In the non-debug mode (for which DONT_OMIT is undefined) build without
# the frame pointer.
ifneq ($(DONT_OMIT), true)
	CSFLAGS += -fomit-frame-pointer
	CFLAGS += -fomit-frame-pointer
endif

$(TARGET): $(PLATFORM)/objs.prc $(PLATFORM)/res.prc 
	$(PAR) -c -a 'resource|backup' -v 4 $@ $(NAME) $(TYPE) $(APPID) $^
ifdef XW_UPLOAD_CMD
	zip $(TARGET).zip $(TARGET)
	$(XW_UPLOAD_CMD) $(TARGET)
	$(XW_UPLOAD_CMD) $(TARGET).zip
endif

solo:
	$(MAKE) XWFEATURE_STANDALONE_ONLY=1

debug:
	$(MAKE) PASSTHRU="-DDEBUG" $(DEBUG_SET)

memdebug:
	$(MAKE) PASSTHRU="-DDEBUG -DMEM_DEBUG" $(DEBUG_SET)

gremlins:
	$(MAKE) PASSTHRU="-DDEBUG -DMEM_DEBUG -DFOR_GREMLINS" DONT_OMIT=true

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
ifeq ($(ARCH),ARM_ONLY)
	$(CC) $(OBJS) -o tmp.o
	$(OBJ-RES) tmp.o
	rm -f tmp.o
else
	$(MULTILINK) $(MULTILINK_OPTIONS) $(OBJS)
endif
	$(PAR) -c -a resource $(PLATFORM)/objs.prc Code rsrc rsrc *.grc
	@rm -f *.grc *.bin


$(PLATFORM)/res.prc: xwords4.rcp $(HEADERS) StrL03e8.bin $(FNAVS) $(PNOLET)
	$(PILRC) $< >/dev/null
	$(PAR) -c -a 'resource' $@ Rsrc rsrc rsrc *.bin
	rm -f $< *.bin

xwords4.rcp: l10n/xwords4_$(LANG).rcp.pre xwords4defines.h $(BITMAP_RSRCS)
	gcc -x c -E -P $(INCLUDES) $(MYDEFS_68K) \
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

ifneq (x$(BUILD_AS_PNOLET)x, xx)
.PHONY : pnolet

pnolet: LocalizedStrIncludes.h
	$(MAKE) -f Makefile.PNO MYDEFS="$(MYDEFS_ARM) -DAPPID=\'$(APPID)\' \
	-DAPPNAME=\\\"$(APPNAME)\\\""
endif

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
		xwords4.rcp *.btxt $(PLATFORM)/* LocalizedStrIncludes.h gdbload
ifneq (x$(BUILD_AS_PNOLET)x, xx)
	$(MAKE) -f Makefile.PNO clean
endif

#cmod03E8.bin: palmdraw.c Makefile
#	$(CC) -O2 -nostartfiles $(INCLUDES) $(MYDEFS) -o tmp $<
#	$(OBJRES) tmp
#	mv code0001.tmp.grc $@ ; rm *.tmp.grc

help:
	@echo make [ARCH=(ARM_ONLY|68K_ONLY|SONY|COMBINED)]
	@echo make memdebug OR
	@echo make LANG=en_US OR
	@echo make ship-all OR
	@echo make gremlins OR
	@echo make clean

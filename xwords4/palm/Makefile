# -*- mode: Makefile; compile-command: "make ARCH=68K_ONLY MEMDEBUG=TRUE"; -*-
# Copyright 2002-2006 by Eric House (xwords@eehouse.org).  All rights
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

PLATFORM=obj_palm
SVNREV = $(shell svnversion -n .. | tr ':' '_')

ifeq ($(MEMDEBUG),TRUE)
DEBUG = -DMEM_DEBUG -DDEBUG -DENABLE_LOGGING
PLATFORM := $(PLATFORM)_memdebug
CFLAGS += $(DEBUG) -g -O0
CSFLAGS += -O0 -g
NAME = "CrossDbg"
ICONTEXT = "CrossDbg"
APPID ?= Xwdb
else
PLATFORM := $(PLATFORM)_rel
CFLAGS += -O2 -fomit-frame-pointer
# The -Os flag breaks multilink, at least as I'm using it
CSFLAGS += -OS -fomit-frame-pointer
NAME = "Crosswords"
ICONTEXT = "Crosswords"
APPID ?= Xwr4
endif

RELAY_NAME_DEFAULT="eehouse.org"

LANG=en_US
ROOTNAME = $(NAME)
BITMAPS = ./bmps
TYPE = appl
MLPREFIX = /usr
DICT = $(shell echo $$BASENG_PATH)

ARCH ?= ARM_ONLY

ifeq ($(ARCH),ARM_ONLY)
PLATFORM := $(PLATFORM)_armonly
BUILD_AS_PNOLET = 1
TARGET=$(PLATFORM)/xw4_ARM_$(SVNREV).prc
else
ifeq ($(ARCH),68K_ONLY)
PLATFORM := $(PLATFORM)_68konly
TARGET=$(PLATFORM)/xw4_68K_$(SVNREV).prc
else
ifeq ($(ARCH),SONY)
# No highres in sony build case
PLATFORM := $(PLATFORM)_sony
BUILD_AS_PNOLET = 1
FEATURE_PNOAND68K = -DFEATURE_PNOAND68K
TARGET=$(PLATFORM)/xw4_SONY_$(SVNREV).prc
else
ifeq ($(ARCH),COMBINED)
PLATFORM := $(PLATFORM)_combined
BUILD_AS_PNOLET = 1
FEATURE_PNOAND68K = -DFEATURE_PNOAND68K
TARGET=$(PLATFORM)/xw4_COMB_$(SVNREV).prc
endif
endif
endif
endif

PLATFORM := $(PLATFORM)_$(shell echo $(LANG) | tr [A-Z] [a-z])

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

MYDEFS_COMMON = $(PASSTHRU) -DXW_FEATURE_UTILS -DPOINTER_SUPPORT \
	-DKEY_SUPPORT 	-DOVERRIDE_EDGE_FOR_INDEX -DCOLOR_SUPPORT \
	-DSHOW_PROGRESS $(HASHDEF) -DNO_REG_REQUIRED -DPERIMETER_FOCUS

# turn on letting users pick tiles "face-up"
MYDEFS_COMMON += -DFEATURE_TRAY_EDIT

# turn on limits to searches on board and in tray (number of tiles to
# use)
MYDEFS_COMMON += -DXWFEATURE_SEARCHLIMIT

#MYDEFS_COMMON += -DXWFEATURE_HINT_CONFIG

ifdef XWFEATURE_STANDALONE_ONLY
MYDEFS_COMMON += -DXWFEATURE_STANDALONE_ONLY
else
# Turn on network play over IP via cellular modem.  Very much
# experimental at this point!
RELAY = -DXWFEATURE_RELAY
MYDEFS_COMMON += $(RELAY)

# turn on bluetooth comms option for 68K and ARM
BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_L2CAP 
# -DCOMMS_HEARTBEAT
#BLUETOOTH = -DXWFEATURE_BLUETOOTH -DBT_USE_RFCOMM
MYDEFS_COMMON += $(BLUETOOTH)

MYDEFS_COMMON += -DXWFEATURE_IR

endif # ifdef XWFEATURE_STANDALONE_ONLY

# Add menu allowing to choose to run 68K or ARM
ifeq ($(ARCH),COMBINED)
MYDEFS_COMMON += -DFEATURE_DUALCHOOSE
endif

# For Danish and perhaps other languages, custom-measure glyph height
# so that overtall letters have a chance of fitting.
MYDEFS_COMMON += -DTALL_FONTS

# Turn on ability to handle 4-byte-node DAWG files.  These are of type
# Xwr4, not Xwr3 like the old ones.  Currently this option means the
# binary won't be able to read or even find the old dictionaries.
# Don't release with this on until there's a UI decision and code on
# how to help users transition.
MYDEFS_COMMON += -DNODE_CAN_4

MYDEFS_COMMON += -DSVN_REV=\"$(shell svnversion -n .)\"

MYDEFS_ARM = -D__LITTLE_ENDIAN -DXW_TARGET_PNO $(MYDEFS_COMMON)
MYDEFS_68K = -DPLATFORM_PALM -D__BIG_ENDIAN $(MYDEFS_COMMON) \
	-DAPPNAME=\"$(APPNAME)\" \
	-DRELAY_NAME_DEFAULT=\"$(RELAY_NAME_DEFAULT)\" \
	$(BLUETOOTH)


BITMAP_RSRCS = \
	$(BITMAPS)/rightarrow.pbitm \
	$(BITMAPS)/downarrow.pbitm \
	$(BITMAPS)/flipbutton.pbitm \
	$(BITMAPS)/valuebutton.pbitm \
	$(BITMAPS)/lightbulb.pbitm \
	$(BITMAPS)/traybuttons.pbitm \
	$(BITMAPS)/showtray.pbitm \
	$(BITMAPS)/xwords4.pbitm
ifdef BLUETOOTH
BITMAP_RSRCS += \
	$(BITMAPS)/bts_conn.bmp \
	$(BITMAPS)/bts_listen.bmp \
	$(BITMAPS)/bts_off.bmp \
	$(BITMAPS)/bts_seek.bmp
endif
ifdef RELAY
BITMAP_RSRCS += \
	$(BITMAPS)/rls_allconn.bmp \
	$(BITMAPS)/rls_off.bmp \
	$(BITMAPS)/rls_pending.bmp
endif

# INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/Extensions/ExpansionMgr
ifdef BLUETOOTH
INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/include/Extensions/Bluetooth
endif

XWFEATURE_FIVEWAY = -DXWFEATURE_FIVEWAY -DKEYBOARD_NAV
XWFEATURE_FIVEWAY += -DDO_TUNGSTEN_FIVEWAY

ifneq (x$(XWFEATURE_FIVEWAY)x, xx)
	INCLUDES += -I/usr/local/share/palmdev/Handspring5
	INCLUDES += -I/usr/local/share/palmdev/Handspring5/68K
# 	INCLUDES += -I/usr/local/share/palmdev/duoIncs 
# 	INCLUDES += -I/usr/local/share/palmdev/duoIncs/68K
# 	INCLUDES += -I/usr/local/share/palmdev/duoIncs/68K/System 
# 	INCLUDES += -I/usr/local/share/palmdev/duoIncs/Common/System 

	MYDEFS_68K += $(XWFEATURE_FIVEWAY)
	MYDEFS_ARM += $(XWFEATURE_FIVEWAY)

endif

INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/include/SonyIncs
INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/include/SonyIncs/System
INCLUDES += -I/usr/local/share/palmdev/sdk-5r3/include/SonyIncs/Libraries
MYDEFS_68K += -DCPU_TYPE=CPU_68K	

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
	$(PLATFORM)/palmbt.o \
	$(PLATFORM)/prefsdlg.o \
	$(PLATFORM)/connsdlg.o \
	$(PLATFORM)/palmblnk.o \
	$(PLATFORM)/palmdbg.o \
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

CSFLAGS += -S -Wall -DAPPID=\'$(APPID)\' $(MYDEFS_68K) $(INCLUDES)
CFLAGS +=  -Wall -DAPPID=\'$(APPID)\' $(MYDEFS_68K) $(INCLUDES) 
# In the non-debug mode (for which DONT_OMIT is undefined) build without
# the frame pointer.

$(TARGET): $(PLATFORM)/objs.prc $(PLATFORM)/res.prc 
	$(PAR) -c -a 'resource|backup' -v 4 $@ $(NAME) $(TYPE) $(APPID) $^
ifdef XW_UPLOAD_SCRIPT
	$(XW_UPLOAD_SCRIPT) $@
endif

solo:
	$(MAKE) XWFEATURE_STANDALONE_ONLY=1

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

$(PLATFORM)/objs.prc: $(PLATFORM)/LocalizedStrIncludes.h $(OBJS) gdbload
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
	gcc -x c -E -P $(INCLUDES) $(MYDEFS_68K) $(DEBUG) \
		-DICONTEXT=\"$(ICONTEXT)\" $< > $@

# LocalizedStrIncludes.h: SVN_REV can change, but I don't have a way
# to express a dependency on it.  If it does change, strings can wind
# up with offsets out of sync with the .h file.  Not sure what to do
# about this....  Clean builds are probably a safe fallback.

$(PLATFORM)/LocalizedStrIncludes.h StrL03e8.bin: \
		./l10n/StrRes_$(LANG).pre ./l10n/mkstrsres.c
	gcc $(CFLAGS) $(FORMATDEFINES) \
		-DLANGSTRFILE=\"$<\" ./l10n/mkstrsres.c \
		-o mkstrsres
	mkdir -p $(PLATFORM)
	./mkstrsres StrL03e8.bin $(PLATFORM)/LocalizedStrIncludes.h
	rm -f mkstrsres

ifneq (x$(BUILD_AS_PNOLET)x, xx)
.PHONY : pnolet

pnolet: $(PLATFORM)/LocalizedStrIncludes.h
	$(MAKE) -f Makefile.PNO MEMDEBUG=$(MEMDEBUG) PLATFORM=$(PLATFORM) \
		MYDEFS="$(MYDEFS_ARM) -DAPPID=\'$(APPID)\' -DAPPNAME=\\\"$(APPNAME)\\\" -DRELAY_NAME_DEFAULT=\\\"$(RELAY_NAME_DEFAULT)\\\""
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
		xwords4.rcp *.btxt $(PLATFORM)/* $(PLATFORM)/LocalizedStrIncludes.h \
			gdbload
ifneq (x$(BUILD_AS_PNOLET)x, xx)
	$(MAKE) -f Makefile.PNO PLATFORM=$(PLATFORM) clean
endif

#cmod03E8.bin: palmdraw.c Makefile
#	$(CC) -O2 -nostartfiles $(INCLUDES) $(MYDEFS) -o tmp $<
#	$(OBJRES) tmp
#	mv code0001.tmp.grc $@ ; rm *.tmp.grc

help:
	@echo "make [ARCH=(ARM_ONLY|68K_ONLY|SONY|COMBINED)] [MEMDEBUG=TRUE] [LANG=fr_FR|en_US|es_ES|es_CT|sv_SE|de_DE] [clean]"
	@echo OR
	@echo "make ship-all"
	@echo OR
	@echo "make gremlins"

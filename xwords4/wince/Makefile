# -*- compile-command: "make -j3 TARGET_OS=wince DEBUG=TRUE RELAY_NAME_DEFAULT=localhost"; -*-
# Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights
# reserved.

# This Makefile is meant to allow building Crosswords for PocketPC and
# Windows using Linux.  As of March 2006, it compiles and runs on
# PocketPC as well as when built with eVC.  Better, since bugs have
# been fixed since then.  It also runs on Windows natively and under
# WINE.
#
# I'm bulding on Debian's "testing" distribution, current as of the
# above date.  Here are some of the packages I had to install to build
# for win32.  I make no guarantees this list is complete.  Patches are
# required to the pocketpc-sdk and mingw32 packages to build.  They've
# been submitted to the maintainers and should be included soon.
# Contact me if you need them in the interim.
#
# mingw32 mingw32-binutils mingw32-runtime pocketpc-sdk

SVNREV = $(shell svnversion -n .. | tr ':' '_')

PLATFORM = wince

TARGET_OS ?= win32
RELAY_NAME_DEFAULT ?= eehouse.org

NATIVE_CC = gcc
STRIDS = strids
#TARGET_OS = wince
#NO_DRAW = -DNO_DRAW

# The preferred build environment for wince is now cegcc.  Debian's
# pocketpc-sdk works in combination with the mingw projects headers,
# but not without effort.  cegcc has its own version of those headers
# that it refers to without need for a -I flag.  They work well.  But
# I'd like to retain the ability to build using pocketpc-sdk if
# possible.

# USE_RAW_MINGW = -DUSE_RAW_MINGW
# arm-wince-mingw32ce for cegcc
ifdef USE_RAW_MINGW
MINGW_INC_PATH ?= -I/usr/i586-mingw32msvc/include
CE_ARCH = arm-wince-pe
else
MINGW_INC_PATH = 				#in case it's in env
#CE_ARCH = arm-wince-mingw32ce
CE_ARCH = arm-mingw32ce
endif
CEOPT_ROOT ?= /opt

# This requires custom Debian pocketpc-sdk package at the moment
HAVE_COMMCTRL = 1
# Dialog to browse fonts on device.  I doubt I want to ship with this.
# CHOOSE_FONTS = -DALLOW_CHOOSE_FONTS

ifeq ($(TARGET_OS),wince)
#SMS = -DXWFEATURE_SMS
RELAY = -DXWFEATURE_RELAY -DCOMMS_HEARTBEAT -DUSE_BUFQUEUE
# IPDIRECT = -DXWFEATURE_IP_DIRECT
# BLUETOOTH = -DXWFEATURE_BLUETOOTH
CC = ${CE_ARCH}-gcc
WINDRES = ${CE_ARCH}-windres
STRIP = ${CE_ARCH}-strip
ifdef USE_RAW_MINGW
CELIBS = -L/usr/arm-wince-pe/lib
else
CELIBS = -L${CEOPT_ROOT}/${CE_ARCH}/lib/ -lws2
endif
#STANDALONE = -DXWFEATURE_STANDALONE_ONLY
STANDALONE = -DPREV_WAS_STANDALONE_ONLY
COLOR_SEL = -DMY_COLOR_SEL
CFLAGS += -D_WIN32_WCE=0x0400 -D_WIN32_IE=0x0400 -DUNDER_CE \
	-D__USE_W32_SOCKETS \
	$(STANDALONE) $(USE_RAW_MINGW) -DDRAW_LINK_DIRECT \
    -DXWFEATURE_TURNCHANGENOTIFY \
	-fdollars-in-identifiers -D__W32API_USE_DLLIMPORT__
# back key should raise focus on main board and backspace in edit controls
CFLAGS += -DOVERRIDE_BACKKEY	
RESFLAGS += -D_WIN32_WCE=0x0400
LFLAGS += -Wl,--major-subsystem-version,4,--minor-subsystem-version,20
ifdef HAVE_COMMCTRL
CELIBS += -lcommctrl -laygshell
else
CFLAGS += -DCANT_DO_CMDBAR 
endif
else							# ifeq ($(TARGET_OS),wince)
ifeq ($(TARGET_OS),win32)
#STANDALONE = -DXWFEATURE_STANDALONE_ONLY
STANDALONE = -DPREV_WAS_STANDALONE_ONLY
#BLUETOOTH = -DXWFEATURE_BLUETOOTH
#SMS = -DXWFEATURE_SMS
RELAY = -DXWFEATURE_RELAY -DUSE_BUFQUEUE
# IPDIRECT = -DXWFEATURE_IP_DIRECT
CC = i586-mingw32msvc-gcc
WINDRES = i586-mingw32msvc-windres
STRIP = i586-mingw32msvc-strip
CELIBS += -lws2_32 

CFLAGS += -DCANT_DO_CMDBAR -DDRAW_LINK_DIRECT -DXWFEATURE_TURNCHANGENOTIFY
# CFLAGS += -DFORCE_FONT="L\"Segoe Condensed\""
#CFLAGS += -DFORCE_HEIGHT=240 -DFORCE_WIDTH=320
endif
endif

# CFLAGS += -DFORCE_SCROLL=3
CFLAGS += -DARM -I./ -I../common -I../relay

CFLAGS += -Wall -Wunused-parameter

CFLAGS += $(BLUETOOTH) $(RELAY) $(SMS) $(IPDIRECT) $(STANDALONE) $(COLOR_SEL) \
	$(CHOOSE_FONTS) $(NO_DRAW)

SVNDEF = -DSVN_REV='\"$(SVNREV)\"'
CFLAGS += $(SVNDEF)
RESFLAGS += $(SVNDEF)

# warn upgrading users of new dict location
CFLAGS += -DKEYBOARD_NAV
CFLAGS += -DPERIMETER_FOCUS

# Hack until figure out how to turn off IME (12-key-to-text) on CE.
CFLAGS += -DNUMBER_KEY_AS_INDEX
ifdef RELAY_NAME_DEFAULT
	CFLAGS += -DRELAY_NAME_DEFAULT=\"$(RELAY_NAME_DEFAULT)\"
endif
# This is normally part of MEM_DEBUG, but sometimes we want logging in
# a release build, e.g. to figure out why opening a saved game isn't
# working.  So normally it's commented out here.
# LOGGING_FLAG = -DENABLE_LOGGING

ifeq ($(DEBUG),TRUE)
MEM_DEBUG_FLAG=-DMEM_DEBUG 
LOGGING_FLAG = -DENABLE_LOGGING
DEBUG_FLAG=-DDEBUG 
CFLAGS += -g -DDEBUG_TS
OBJDIR = obj_$(TARGET_OS)_dbg
else
CFLAGS += -Os
OBJDIR = obj_$(TARGET_OS)_rel
endif

BUILTDIR=$(OBJDIR)/built
TARGET = $(BUILTDIR)/xwords4_$(SVNREV).exe
DLL_SRCS = $(shell ls ./l10n/*.rc)
#DLLS = $(patsubst %.rc,%.dll,$(addprefix $(OBJDIR)/l10n/,$(notdir $(DLL_SRCS))))
DLLS = \
	$(BUILTDIR)/xwords4_french.dll \
	$(BUILTDIR)/xwords4_caps.dll \
	$(BUILTDIR)/xwords4_polish_ISO.dll \

RESOBJS = \
	$(OBJDIR)/xwords4.rc.o \

WINCESRC = \
	ceaskpwd.c \
	ceclrsel.c\
	cedict.c \
	ceginfo.c \
	cemain.c \
	cesockwr.c \
	ceutil.c \
	ceblank.c \
	cecondlg.c \
	cesvdgms.c \
	cehntlim.c \
	ceprefs.c \
	cestrbx.c \
	cedraw.c \
	cefonts.c \
	ceresstr.c \
	cesms.c \
	debhacks.c \
	cedebug.c \

WINCEOBJS = $(patsubst %.c,%.o,$(addprefix $(OBJDIR)/,$(WINCESRC)))

WINCEICONS = bmps/xwords4_ico_16x16.png \
			 bmps/xwords4_ico_22x22.png \
			 bmps/xwords4_ico_32x32.png \

include ../common/config.mk

COMMONOBJ = $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(notdir $(COMMONSRC))))

OBJS = $(WINCEOBJS) $(COMMONOBJ)

WINCE_INCLUDES = $(shell ls *.h)
COMMON_INCLUDES = $(shell ls ../common/*.h)

INCLUDES = $(WINCE_INCLUDES) $(COMMON_INCLUDES)

XW_BOTH_DEFINES = \
	$(TARGET_OS_DEF) \
	-DNODE_CAN_4 \
	-DCOLOR_SUPPORT -DFEATURE_TRAY_EDIT -DXWFEATURE_SEARCHLIMIT \
	-DXWFEATURE_HINT_CONFIG \
	-DPOINTER_SUPPORT -DKEY_SUPPORT -D__LITTLE_ENDIAN \
	-DCEFEATURE_CANSCROLL -DUNICODE \
	-DLOADSTRING_BROKEN \
	$(DEBUG_FLAG) $(MEM_DEBUG_FLAG) $(LOGGING_FLAG) \

CFLAGS += $(XW_BOTH_DEFINES) -DARM
RESFLAGS += $(XW_BOTH_DEFINES) $(STANDALONE) $(BLUETOOTH) $(RELAY) $(SMS) \
	$(IPDIRECT) -D_WIN32 -D_WIN32_IE=0x0400 $(COLOR_SEL) $(CHOOSE_FONTS)

####################################################################
# Rules start here
####################################################################

all : $(TARGET) $(DLLS)

memdebug:
	$(MAKE) DEBUG=TRUE all

$(TARGET) : $(OBJS) $(RESOBJS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LFLAGS) -mwindows $^ $(CELIBS) -o $@
	$(STRIP) $@

$(OBJDIR)/%.o : %.c $(INCLUDES)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : ../common/%.c $(INCLUDES)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/xwords4.rc.o : xwords4.rc $(INCLUDES) xwords4.ico
	$(WINDRES) -v $(MINGW_INC_PATH) $(RESFLAGS) -o $@ $< 

$(OBJDIR)/l10n/%.rc.o : l10n/%.rc $(WINCE_INCLUDES)
	mkdir -p $(dir $@)
	UTF8=x$(shell file -b -n -i $< | grep 'utf-8'); \
		if [ x != $$UTF8 ]; then ENC="-c 65001"; fi; \
		$(WINDRES) $$ENC -v $(MINGW_INC_PATH) $(RESFLAGS) $< -o $@

$(BUILTDIR)/%.dll: $(OBJDIR)/l10n/%.rc.o
	mkdir -p $(dir $@)
	$(CC) -shared -o $@ $<
	$(STRIP) $@

# Checking in xwords4.ico for now.  Hand-built using GIMP and layers
# it's 1/4 the size (because it has only two colors).  Should figure
# out how to script GIMP and replace the below.
# xwords4.ico:
# 	$(WINCEICONS) icotool -c -o $@ $^

CAB: $(TARGET)
	$(STRIP) $<
	./exe2cab.pl $<

SHIP: $(TARGET)
	$(STRIP) $<
ifdef WINCE_UPLOAD_LOC
	zip $<.zip $<
	scp $<.zip $(WINCE_UPLOAD_LOC)
	scp $< $(WINCE_UPLOAD_LOC)
endif

test:
	echo $(COMMONOBJ)

clean :
	rm -rf $(OBJDIR) $(TARGET)

help:
	@echo "try 'make TARGET_OS=wince [DEBUG=TRUE]'"
	@echo "or 'make TARGET_OS=win32 [DEBUG=TRUE]'"


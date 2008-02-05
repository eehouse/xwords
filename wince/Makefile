# -*- compile-command: "make TARGET_OS=wince DEBUG=TRUE"; -*-

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
#TARGET_OS = wince

MINGW_INC_PATH ?= /usr/i586-mingw32msvc/include

# This requires custom Debian pocketpc-sdk package at the moment
HAVE_COMMCTRL = 1

# arm-wince-mingw32ce for cegcc, arm-wince-pe for pocketpc-gcc
#ARCH = arm-wince-pe
ARCH = arm-wince-mingw32ce
ifeq ($(TARGET_OS),wince)
# BLUETOOTH = -DXWFEATURE_BLUETOOTH
# RELAY = -DXWFEATURE_RELAY -DCOMMS_HEARTBEAT
CC = ${ARCH}-gcc
WINDRES = ${ARCH}-windres
STRIP = ${ARCH}-strip
#CELIBS = -L/usr/arm-wince-pe/lib
CELIBS = -L${CEOPT_ROOT}/opt/${ARCH}/lib/ -lws2
TARGET_OS_DEF = -DTARGET_OS_WINCE
STANDALONE = -DXWFEATURE_STANDALONE_ONLY
CFLAGS += -D_WIN32_WCE=400 -DUNDER_CE -DMY_COLOR_SEL \
	$(STANDALONE) -DDRAW_LINK_DIRECT \
	-fdollars-in-identifiers -D__W32API_USE_DLLIMPORT__
RESFLAGS += -DMY_COLOR_SEL -D_WIN32_WCE=400 
LFLAGS += -Wl,--major-subsystem-version,4,--minor-subsystem-version,20
ifdef HAVE_COMMCTRL
CELIBS += -lcommctrl -laygshell
else
CFLAGS += -DCANT_DO_CMDBAR 
endif
else							# ifeq ($(TARGET_OS),wince)
ifeq ($(TARGET_OS),win32)
STANDALONE = -DXWFEATURE_STANDALONE_ONLY
#BLUETOOTH = -DXWFEATURE_BLUETOOTH
#RELAY = -DXWFEATURE_RELAY
CC = i586-mingw32msvc-gcc
WINDRES = i586-mingw32msvc-windres
STRIP = i586-mingw32msvc-strip
TARGET_OS_DEF = -DTARGET_OS_WIN32
CELIBS += -lws2_32 
CFLAGS += -DCANT_DO_CMDBAR -DDRAW_LINK_DIRECT
#CFLAGS += -DFORCE_HEIGHT=240 -DFORCE_WIDTH=320
endif
endif

CFLAGS += -DARM -I$(MINGW_INC_PATH) -I./ -I../common -I../relay

CFLAGS += -Wall -Wunused-parameter

CFLAGS += $(BLUETOOTH) $(RELAY) $(STANDALONE)

SVNDEF = -DSVN_REV='\"$(SVNREV)\"'
CFLAGS += $(SVNDEF)
RESFLAGS += $(SVNDEF)

# warn upgrading users of new dict location
CFLAGS += -DDICTS_MOVED_ALERT
CFLAGS += -DKEYBOARD_NAV
CFLAGS += -DPERIMETER_FOCUS

ifeq ($(DEBUG),TRUE)
MEM_DEBUG_FLAG=-DMEM_DEBUG 
DEBUG_FLAG=-DDEBUG 
CFLAGS += -g -DDEBUG_TS
OBJDIR = obj_$(TARGET_OS)_dbg
else
CFLAGS += -Os
OBJDIR = obj_$(TARGET_OS)_rel
endif

TARGET = $(OBJDIR)/xwords4_$(SVNREV).exe

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
	cehntlim.c \
	ceprefs.c \
	cestrbx.c \
	cedraw.c \
	debhacks.c \

WINCEOBJS = $(patsubst %.c,%.o,$(addprefix $(OBJDIR)/,$(WINCESRC)))

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
	$(DEBUG_FLAG) $(MEM_DEBUG_FLAG) \

CFLAGS += $(XW_BOTH_DEFINES) -DARM
RESFLAGS += $(XW_BOTH_DEFINES) $(STANDALONE) $(BLUETOOTH) $(RELAY) \
	-D_WIN32 -D_WIN32_IE=0x0400 

####################################################################
# Rules start here
####################################################################

all : $(TARGET)

memdebug:
	$(MAKE) DEBUG=TRUE all

$(TARGET) : $(OBJS) $(RESOBJS)
	$(CC) $(CFLAGS) $(LFLAGS) -mwindows $^ $(CELIBS) -o $@
	$(STRIP) $@

$(OBJDIR)/%.o : %.c $(INCLUDES)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : ../common/%.c $(INCLUDES)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/xwords4.rc.o : xwords4.rc $(INCLUDES)
	$(WINDRES) -v -I$(MINGW_INC_PATH) $(RESFLAGS) -o $@ $< 

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

# This Makefile is meant to allow building Crosswords for PocketPC and
# Windows using Linux.  As of Nov 27, 2005, it compiles and runs (on
# XP) enough to draw its board, but then crashes.  It appears from the
# log that an assert is failing.
#
# It does not yet link for PPC.
#
# I'm bulding on Debian's "testing" distribution, current as of the
# above date.  Here are some of the packages I had to install to build
# for win32.  I make no guarantees this list is complete.
#
# mingw32 mingw32-binutils mingw32-runtime

PLATFORM = wince

# TARGET_OS = wince
TARGET_OS = win32

ifeq ($(TARGET_OS),wince)
CC = arm-wince-pe-gcc
WINDRES = arm-wince-pe-windres
STRIP = arm-wince-pe-strip
CELIBS = -L/usr/arm-wince-pe/lib
TARGET_OS_DEF = -DTARGET_OS_WINCE
CFLAGS += -DUNDER_CE
else
ifeq ($(TARGET_OS),win32)
CC = i586-mingw32msvc-gcc
WINDRES = i586-mingw32msvc-windres
STRIP = i586-mingw32msvc-strip
TARGET_OS_DEF = -DTARGET_OS_WIN32
CELIBS += -lws2_32 
endif
endif

CFLAGS += -DARM -I/usr/i586-mingw32msvc/include -I./ -I../common -I../relay 

DEBUG ?= -DDEBUG
MEM_DEBUG ?= -DMEM_DEBUG

OBJDIR = obj_$(TARGET_OS)
TARGET = $(OBJDIR)/xwords4.exe

RESOBJS = \
	$(OBJDIR)/xwords4.rc.o \

WINCEOBJS = $(OBJDIR)/

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

WINCEOBJS = $(patsubst %.c,%.o,$(addprefix $(OBJDIR)/,$(WINCESRC)))

include ../common/config.mk

COMMONOBJ = $(addprefix $(OBJDIR)/,$(patsubst %.c,%.o,$(notdir $(COMMONSRC))))

OBJS = $(WINCEOBJS) $(COMMONOBJ)

WINCE_INCLUDES = $(shell ls *.h)
COMMON_INCLUDES = $(shell ls ../common/*.h)

INCLUDES = $(WINCE_INCLUDES) $(COMMON_INCLUDES)

XW_BOTH_DEFINES = \
	$(TARGET_OS_DEF) \
	-DBEYOND_IR -DNODE_CAN_4 -DMY_COLOR_SEL \
	-DCOLOR_SUPPORT -DFEATURE_TRAY_EDIT -DXWFEATURE_SEARCHLIMIT \
	-DXWFEATURE_HINT_CONFIG \
	-DPOINTER_SUPPORT -DKEY_SUPPORT -D__LITTLE_ENDIAN \
	-DCEFEATURE_CANSCROLL -DUNICODE \
	-DCANT_DO_SHELL_THING \
	$(DEBUG) $(MEM_DEBUG) \

CFLAGS += $(XW_BOTH_DEFINES) -DARM

####################################################################
# Rules start here
####################################################################

all : $(TARGET)

$(TARGET) : $(OBJS) $(RESOBJS)
	$(CC) $(CFLAGS) -mwindows $^ $(CELIBS) -o $@

$(OBJDIR)/%.o : %.c $(INCLUDES)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/%.o : ../common/%.c $(INCLUDES)
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR)/xwords4.rc.o : xwords4.rc $(INCLUDES)
	$(WINDRES) -v -I/usr/i586-mingw32msvc/include \
	-D_WIN32_WCE -D_WIN32 -D_WIN32_IE=0x0400 \
		-o $@ $< 

SHIP: $(TARGET)
	$(STRIP) $<
ifdef UPLOAD_LOC
	cp $< $(UPLOAD_LOC)
endif

test:
	echo $(COMMONOBJ)

clean :
	rm -rf $(OBJDIR) $(TARGET)

# -*- mode: Makefile; -*-

ifeq (x$(shell echo $$WINCE_DEV_PATH)x,xx)
WINCE_PATH = "PLEASE DEFINE shell env variable WINCE_DEV_PATH"
else
WINCE_PATH = $(shell echo $$WINCE_DEV_PATH)
endif

PLATOBJ = \
	$(PLATFORM)/StdAfx.o \
	$(PLATFORM)/ceaskpwd.o \
	$(PLATFORM)/ceblank.o \
	$(PLATFORM)/cedict.o \
	$(PLATFORM)/cedraw.o \
	$(PLATFORM)/ceginfo.o \
	$(PLATFORM)/cemain.o \
	$(PLATFORM)/ceprefs.o \
	$(PLATFORM)/cestrbx.o \
	$(PLATFORM)/ceutil.o \

XW_C_DEFINES = \
	/D __LITTLE_ENDIAN /D POINTER_SUPPORT /D KEY_SUPPORT \
	/D XWFEATURE_STANDALONE_ONLY $(DEBUG) $(MEM_DEBUG)

XW_RES_DEFINES = \
	/d XWFEATURE_STANDALONE_ONLY $(DEBUG) $(MEM_DEBUG)

all: $(TARGET)

debug:
	$(MAKE) DEBUG="/DDEBUG"

memdebug:
	$(MAKE) DEBUG="/DDEBUG" MEM_DEBUG="/DMEM_DEBUG"

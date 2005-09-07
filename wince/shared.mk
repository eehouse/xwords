# -*- mode: Makefile; -*-

ifeq (x$(shell echo $$PPC_SDK_PPC)x,xx)
WINCE_PATH = "PLEASE DEFINE shell env variable WINCE_DEV_PATH"
include exit_right_now
else
WINCE_PATH = $(shell echo $$PPC_SDK_PPC)
endif

PLATOBJ = \
	$(PLATFORM)/StdAfx.o \
	$(PLATFORM)/ceaskpwd.o \
	$(PLATFORM)/ceblank.o \
	$(PLATFORM)/cedict.o \
	$(PLATFORM)/cedraw.o \
	$(PLATFORM)/ceginfo.o \
	$(PLATFORM)/cecondlg.o \
	$(PLATFORM)/cemain.o \
	$(PLATFORM)/ceprefs.o \
	$(PLATFORM)/cestrbx.o \
	$(PLATFORM)/ceutil.o \
	$(PLATFORM)/ceclrsel.o \
	$(PLATFORM)/cesockwr.o \
	$(PLATFORM)/cehntlim.o \

XW_BOTH_DEFINES = \
	/DBEYOND_IR /DNODE_CAN_4 /DMY_COLOR_SEL \
	/DCOLOR_SUPPORT /DFEATURE_TRAY_EDIT /DXWFEATURE_SEARCHLIMIT \
	/DXWFEATURE_HINT_CONFIG \
	/D POINTER_SUPPORT /D KEY_SUPPORT /D __LITTLE_ENDIAN \
	/DCEFEATURE_CANSCROLL \
	$(DEBUG) $(MEM_DEBUG) \

XW_C_DEFINES = \
	$(XW_BOTH_DEFINES) \

XW_RES_DEFINES = \
	$(XW_BOTH_DEFINES)

all: $(TARGET)

debug:
	$(MAKE) DEBUG="/DDEBUG"

memdebug:
	$(MAKE) DEBUG="/DDEBUG" MEM_DEBUG="/DMEM_DEBUG"

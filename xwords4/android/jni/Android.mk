# -*- mode: Makefile; -*-
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

COMMON_PATH=../../common
LOCAL_C_INCLUDES+= \
	-I$(LOCAL_PATH)/$(COMMON_PATH) \
	-I$(LOCAL_PATH)/../../relay \

LOCAL_LDLIBS += -llog

ifeq ($(BUILD_TARGET),debug)
	LOCAL_DEBUG = -DMEM_DEBUG -DDEBUG -DENABLE_LOGGING -DCOMMS_CHECKSUM -Wno-unused-variable
endif
LOCAL_DEFINES += \
	$(LOCAL_DEBUG) \
	-DXWFEATURE_BLUETOOTH \
	-DXWFEATURE_SMS \
	-DXWFEATURE_P2P \
	-DXWFEATURE_COMMSACK \
	-DXWFEATURE_TURNCHANGENOTIFY \
	-DCOMMS_XPORT_FLAGSPROC \
	-DKEY_SUPPORT \
	-DXWFEATURE_CROSSHAIRS \
	-DPOINTER_SUPPORT \
	-DSCROLL_DRAG_THRESHHOLD=1 \
	-DDROP_BITMAPS \
	-DXWFEATURE_TRAYUNDO_ONE \
	-DDISABLE_TILE_SEL \
	-DXWFEATURE_BOARDWORDS \
	-DXWFEATURE_WALKDICT \
	-DXWFEATURE_WALKDICT_FILTER \
	-DXWFEATURE_DICTSANITY \
	-DFEATURE_TRAY_EDIT \
	-DXWFEATURE_BONUSALL \
	-DMAX_ROWS=32 \
	-DHASH_STREAM \
	-DXWFEATURE_DEVID \
	-DXWFEATURE_CHAT \
	-DXWFEATURE_KNOWNPLAYERS \
	-DCOMMON_LAYOUT \
	-DXWFEATURE_WIDER_COLS \
	-DNATIVE_NLI \
	-DINITIAL_CLIENT_VERS=${INITIAL_CLIENT_VERS} \
	-DXW_BT_UUID=\"${XW_BT_UUID}\" \
	-DVARIANT_${VARIANT} \
	-DRELAY_ROOM_DEFAULT=\"\" \
	-D__LITTLE_ENDIAN \

#	-DMQTT_USE_PROTO=2 \

# XWFEATURE_RAISETILE: first, fix to not use timer
#   -DXWFEATURE_RAISETILE \

#   -DNO_ADD_MQTT_TO_ALL \
#	-DXWFEATURE_SCOREONEPASS \

LOCAL_SRC_FILES +=         \
	xwjni.c                \
	utilwrapper.c          \
	drawwrapper.c          \
	xportwrapper.c         \
	anddict.c              \
	andutils.c             \
	jniutlswrapper.c       \


COMMON_PATH=../../common
COMMON_SRC_FILES +=        \
	$(COMMON_PATH)/boarddrw.c   \
	$(COMMON_PATH)/scorebdp.c   \
	$(COMMON_PATH)/dragdrpp.c   \
	$(COMMON_PATH)/pool.c       \
	$(COMMON_PATH)/tray.c       \
	$(COMMON_PATH)/dictnry.c    \
	$(COMMON_PATH)/dictiter.c   \
	$(COMMON_PATH)/dictmgr.c    \
	$(COMMON_PATH)/mscore.c     \
	$(COMMON_PATH)/vtabmgr.c    \
	$(COMMON_PATH)/strutils.c   \
	$(COMMON_PATH)/engine.c     \
	$(COMMON_PATH)/board.c      \
	$(COMMON_PATH)/mempool.c    \
	$(COMMON_PATH)/game.c       \
	$(COMMON_PATH)/server.c     \
	$(COMMON_PATH)/model.c      \
	$(COMMON_PATH)/comms.c      \
	$(COMMON_PATH)/memstream.c  \
	$(COMMON_PATH)/movestak.c   \
	$(COMMON_PATH)/dbgutil.c    \
	$(COMMON_PATH)/nli.c    	\
	$(COMMON_PATH)/smsproto.c  	\
	$(COMMON_PATH)/dutil.c  	\
	$(COMMON_PATH)/device.c  	\
	$(COMMON_PATH)/knownplyr.c  \

LOCAL_CFLAGS+=$(LOCAL_C_INCLUDES) $(LOCAL_DEFINES) -Wall -std=c99
LOCAL_SRC_FILES := $(linux_SRC_FILES) $(LOCAL_SRC_FILES) $(COMMON_SRC_FILES)
LOCAL_MODULE    := xwjni
LOCAL_LDLIBS 	:= -L${SYSROOT}/usr/lib -llog -lz 

include $(BUILD_SHARED_LIBRARY)

# This recipe doesn't work with clang. Fix if using gcc again
# ifneq ($(shell which ccache),)
# 	TARGET_CC = ccache $(TOOLCHAIN_PREFIX)gcc
# 	TARGET_CXX = ccache $(TOOLCHAIN_PREFIX)g++
# endif

COMMON_SRC_FILES :=
COMMON_PATH :=

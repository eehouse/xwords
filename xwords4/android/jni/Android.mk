# -*- mode: Makefile; -*-
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

COMMON_PATH=../../common
LOCAL_C_INCLUDES+= \
	-I$(LOCAL_PATH)/$(COMMON_PATH) \
	-I$(LOCAL_PATH)/../../relay \

LOCAL_LDLIBS += -llog

ifeq ($(BUILD_TARGET),debug)
	LOCAL_DEBUG = -DMEM_DEBUG -DDEBUG -DENABLE_LOGGING -Wno-unused-variable
	LOCAL_DEBUG += -DWAIT_ALL_SECS=3
endif
LOCAL_DEFINES += \
	$(LOCAL_DEBUG) \
	-DXWFEATURE_BLUETOOTH \
	-DXWFEATURE_SMS \
	-DXWFEATURE_P2P \
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
	-DXWFEATURE_WIDER_COLS \
	-DINITIAL_CLIENT_VERS=${INITIAL_CLIENT_VERS} \
	-DXW_BT_UUID=\"${XW_BT_UUID}\" \
	-DVARIANT_${VARIANT} \
	-DRELAY_ROOM_DEFAULT=\"\" \
	-D__LITTLE_ENDIAN \
	-DXWFEATURE_GAMEREF_CONVERT \
	-DXWFEATURE_BASE64 \
	-DGITREV=\"${GITREV}\" \

# XWFEATURE_RAISETILE: first, fix to not use timer
#   -DXWFEATURE_RAISETILE \

#   -DNO_ADD_MQTT_TO_ALL \
#	-DXWFEATURE_SCOREONEPASS \

LOCAL_SRC_FILES +=         \
	xwjni.c                \
	utilwrapper.c          \
	drawwrapper.c          \
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
	$(COMMON_PATH)/util.c       \
	$(COMMON_PATH)/dictnry.c    \
	$(COMMON_PATH)/dictiter.c   \
	$(COMMON_PATH)/dictmgr.c    \
	$(COMMON_PATH)/mscore.c     \
	$(COMMON_PATH)/strutils.c   \
	$(COMMON_PATH)/engine.c     \
	$(COMMON_PATH)/board.c      \
	$(COMMON_PATH)/draw.c      	\
	$(COMMON_PATH)/mempool.c    \
	$(COMMON_PATH)/game.c       \
	$(COMMON_PATH)/contrlr.c    \
	$(COMMON_PATH)/model.c      \
	$(COMMON_PATH)/comms.c      \
	$(COMMON_PATH)/xwmutex.c    \
	$(COMMON_PATH)/xwstream.c  \
	$(COMMON_PATH)/movestak.c   \
	$(COMMON_PATH)/dbgutil.c    \
	$(COMMON_PATH)/nli.c    	\
	$(COMMON_PATH)/msgchnk.c  	\
	$(COMMON_PATH)/dutil.c  	\
	$(COMMON_PATH)/device.c  	\
	$(COMMON_PATH)/dvcbt.c  	\
	$(COMMON_PATH)/knownplyr.c  \
	$(COMMON_PATH)/dllist.c     \
	$(COMMON_PATH)/xwarray.c    \
	$(COMMON_PATH)/stats.c      \
	$(COMMON_PATH)/timers.c     \
	$(COMMON_PATH)/gamemgr.c    \
	$(COMMON_PATH)/gameref.c    \
	$(COMMON_PATH)/chatp.c    	\
	$(COMMON_PATH)/cJSON.c      \
	$(COMMON_PATH)/cJSON_Utils.c\

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

# -*- mode: Makefile; compile-command: "cd ../; ${NDK_ROOT}/ndk-build -j3"; -*-
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

COMMON_PATH=../../../common
LOCAL_C_INCLUDES+= \
	-I$(LOCAL_PATH)/$(COMMON_PATH) \
	-I$(LOCAL_PATH)/../../../relay \

LOCAL_LDLIBS += -llog

ifeq ($(BUILD_TARGET),debug)
	LOCAL_DEBUG = -DMEM_DEBUG -DDEBUG -DENABLE_LOGGING -DCOMMS_CHECKSUM -Wno-unused-but-set-variable
endif
LOCAL_DEFINES += \
	$(LOCAL_DEBUG) \
	-DXWFEATURE_RELAY \
	-DXWFEATURE_SMS \
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
	-DXWFEATURE_BASE64 \
	-DXWFEATURE_DEVID \
	-DINITIAL_CLIENT_VERS=${INITIAL_CLIENT_VERS} \
	-DRELAY_ROOM_DEFAULT=\"\" \
	-D__LITTLE_ENDIAN \

ifeq ($(CHAT_ENABLED),true)

	LOCAL_DEFINES += -DXWFEATURE_CHAT
endif

#	-DXWFEATURE_SCOREONEPASS \

LOCAL_SRC_FILES +=         \
	xwjni.c                \
	utilwrapper.c          \
	drawwrapper.c          \
	xportwrapper.c         \
	anddict.c              \
	andutils.c             \
	jniutlswrapper.c       \


COMMON_PATH=../../../common
COMMON_SRC_FILES +=        \
	$(COMMON_PATH)/boarddrw.c   \
	$(COMMON_PATH)/scorebdp.c   \
	$(COMMON_PATH)/dragdrpp.c   \
	$(COMMON_PATH)/pool.c       \
	$(COMMON_PATH)/tray.c       \
	$(COMMON_PATH)/dictnry.c    \
	$(COMMON_PATH)/dictiter.c   \
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


LOCAL_CFLAGS+=$(LOCAL_C_INCLUDES) $(LOCAL_DEFINES)
LOCAL_SRC_FILES := $(linux_SRC_FILES) $(LOCAL_SRC_FILES) $(COMMON_SRC_FILES)
LOCAL_MODULE    := xwjni
LOCAL_LDLIBS 	:= -L${SYSROOT}/usr/lib -llog -lz 

include $(BUILD_SHARED_LIBRARY)

COMMON_SRC_FILES :=
COMMON_PATH :=

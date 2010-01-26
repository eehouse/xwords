# -*- mode: Makefile -*-

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

local_C_INCLUDES+= 			  \
	-I$(LOCAL_PATH)/../common \
	-I$(LOCAL_PATH)/../relay \

local_LDLIBS += -llog

# local_DEBUG = -DMEM_DEBUG -DDEBUG -DENABLE_LOGGING

local_DEFINES += \
	$(local_DEBUG) \
	-DXWFEATURE_TURNCHANGENOTIFY \
	-DXWFEATURE_SEARCHLIMIT \
	-DKEYBOARD_NAV \
	-DKEY_SUPPORT \
	-DPOINTER_SUPPORT \
	-DNODE_CAN_4 \
	-D__LITTLE_ENDIAN \

local_SRC_FILES +=         \
	xwjni.c                \
	utilwrapper.c          \
	drawwrapper.c          \
	anddict.c              \
	andutils.c             \

common_SRC_FILES +=        \
	../common/boarddrw.c   \
	../common/scorebdp.c   \
	../common/dragdrpp.c   \
	../common/pool.c       \
	../common/tray.c       \
	../common/dictnry.c    \
	../common/mscore.c     \
	../common/vtabmgr.c    \
	../common/strutils.c   \
	../common/engine.c     \
	../common/board.c      \
	../common/mempool.c    \
	../common/game.c       \
	../common/server.c     \
	../common/model.c      \
	../common/comms.c      \
	../common/memstream.c  \
	../common/movestak.c   \


LOCAL_CFLAGS+=$(local_C_INCLUDES) $(local_DEFINES)
LOCAL_SRC_FILES := $(linux_SRC_FILES) $(local_SRC_FILES) $(common_SRC_FILES)
LOCAL_MODULE    := xwjni
LOCAL_LDLIBS := -L${SYSROOT}/usr/lib -llog -lz 

include $(BUILD_SHARED_LIBRARY)



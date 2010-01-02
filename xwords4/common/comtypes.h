/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#ifndef _COMTYPES_H_
#define _COMTYPES_H_

#include "xptypes.h"

#ifndef EXTERN_C_START
# ifdef CPLUS
#  define EXTERN_C_START extern "C" {
# else 
#  define EXTERN_C_START
# endif
#endif

#ifndef EXTERN_C_END
# ifdef CPLUS
#  define EXTERN_C_END }
# else 
#  define EXTERN_C_END
# endif
#endif

#define VSIZE(arr) (sizeof(arr)/sizeof(arr[0]))

typedef struct XP_Rect {
    XP_S16 left;
    XP_S16 top;
    XP_S16 width;
    XP_S16 height;
} XP_Rect;

typedef XP_U16 CellTile;

typedef XP_U8 Tile;

typedef void* XP_Bitmap;

typedef enum {
    TRI_ENAB_NONE
    ,TRI_ENAB_HIDDEN
    ,TRI_ENAB_DISABLED
    ,TRI_ENAB_ENABLED
} XP_TriEnable;

typedef enum {
    DFS_NONE
    ,DFS_TOP                    /* focus is on the object */
    ,DFS_DIVED                  /* focus is inside the object */
} DrawFocusState;

typedef enum {
    TRAY_HIDDEN, /* doesn't happen unless tray overlaps board */
    TRAY_REVERSED,
    TRAY_REVEALED
} XW_TrayVisState;

typedef enum {
    OBJ_NONE,
    OBJ_BOARD,
    OBJ_SCORE,
    OBJ_TRAY
} BoardObjectType;

/* I'm going to try putting all forward "class" decls in the same file */
typedef struct BoardCtxt BoardCtxt;
typedef struct CommMgrCtxt CommMgrCtxt;
typedef struct DictionaryCtxt DictionaryCtxt;
typedef struct DrawCtx DrawCtx;
typedef struct EngineCtxt EngineCtxt;
typedef struct ModelCtxt ModelCtxt;
typedef struct CommsCtxt CommsCtxt;
typedef struct PlayerSocket PlayerSocket;
typedef struct ScoreBdContext ScoreBdContext;
typedef struct ServerCtxt ServerCtxt;
typedef struct XWStreamCtxt XWStreamCtxt;
typedef struct TrayContext TrayContext;
typedef struct PoolContext PoolContext;
typedef struct XW_UtilCtxt XW_UtilCtxt;

/* Low two bits treated as channel; rest can be random to aid detection of
 * duplicate packets. */
#define CHANNEL_MASK 0x0003
typedef XP_U16 XP_PlayerAddr;

typedef enum {
    TIMER_PENDOWN = 1, /* ARM doesn't like ids of 0... */
    TIMER_TIMERTICK,
#ifndef XWFEATURE_STANDALONE_ONLY
    TIMER_COMMS,
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    TIMER_SLOWROBOT,
#endif
    NUM_TIMERS_PLUS_ONE          /* must be last */
} XWTimerReason;

#define MAX_NUM_PLAYERS 4
#define PLAYERNUM_NBITS 2
#define NDEVICES_NBITS 2        /* 1-4, but reduced by 1 fits in 2 bits */
#define NPLAYERS_NBITS 3
#define EMPTIED_TRAY_BONUS 50

/* I need a way to communiate prefs to common/ code.  For now, though, I'll
 * leave storage of these values up to the platforms.  First, because I don't
 * want to deal with versioning in the common code.  Second, becuase they
 * already have the notion of per-game and all-game prefs.
 */
typedef struct CommonPrefs {
    XP_Bool         showBoardArrow;  /* applies to all games */
    XP_Bool         showRobotScores; /* applies to all games */
    XP_Bool         hideTileValues; 
    XP_Bool         skipCommitConfirm; /* applies to all games */
#ifdef XWFEATURE_SLOW_ROBOT
    XP_U16          robotThinkMin, robotThinkMax;
#endif
} CommonPrefs;

#ifdef XWFEATURE_BLUETOOTH
/* temporary debugging hack */

/* From BtLibTypes.h: Pre-assigned assigned PSM values are permitted; however,
 * they must be odd, within the range of 0x1001 to 0xFFFF, and have the 9th
 * bit (0x0100) set to zero. Passing in BT_L2CAP_RANDOM_PSM will automatically
 * create a usable PSM for the channel. In this case the actual PSM value will
 * be filled in by the call. */

# define XW_PSM     0x3031
#endif

/* used for all vtables */
#define SET_VTABLE_ENTRY( vt, name, prefix ) \
         (vt)->m_##name = prefix##_##name

#ifdef DRAW_LINK_DIRECT
# define DLSTATIC
#else
# define DLSTATIC static
#endif

#ifdef DEBUG
# define DEBUG_ASSIGN(a,b) (a) = (b)
#else
# define DEBUG_ASSIGN(a,b)
#endif

#define OFFSET_OF(typ,var)  ((XP_U16)&(((typ*) 0)->var))

#ifndef RELAY_NAME_DEFAULT
# define RELAY_NAME_DEFAULT "eehouse.org"
#endif
#ifndef RELAY_ROOM_DEFAULT
# define RELAY_ROOM_DEFAULT "Room 1"
#endif
#ifndef RELAY_PORT_DEFAULT
# define RELAY_PORT_DEFAULT 10999
#endif

#ifdef MEM_DEBUG
# define XP_MALLOC(pool,nbytes) \
    mpool_alloc((pool),(nbytes),__FILE__,__func__, __LINE__)
# define XP_CALLOC(pool,nbytes) \
    mpool_calloc((pool),(nbytes),__FILE__,__func__, __LINE__)
# define XP_REALLOC(pool,p,s) \
    mpool_realloc((pool),(p),(s),__FILE__,__func__,__LINE__)
# define XP_FREE(pool,p) \
    mpool_free((pool), (p),__FILE__,__func__,__LINE__)

# define MPFORMAL_NOCOMMA MemPoolCtx* mpool
# define MPFORMAL         MPFORMAL_NOCOMMA,
# define MPSLOT           MPFORMAL_NOCOMMA;
# define MPPARM_NOCOMMA(p)  (p)
# define MPPARM(p)          MPPARM_NOCOMMA(p),
# define MPASSIGN(slot,val) (slot)=(val)

#else

# define MPFORMAL_NOCOMMA
# define MPFORMAL
# define MPSLOT
# define MPPARM_NOCOMMA(p)
# define MPPARM(p)
# define MPASSIGN(slot,val)

#endif

#define LOG_FUNC()  XP_LOGF( "IN: %s", __func__ )
#define LOG_RETURNF(fmt, ...)  XP_LOGF( "%s => " fmt, __func__, __VA_ARGS__ )
#define LOG_RETURN_VOID() LOG_RETURNF("%s","void")
#define XP_LOGLOC() XP_LOGF( "%s(), line %d", __func__, __LINE__ )

#ifndef XP_UNUSED
# if defined __GNUC__
#  define XP_UNUSED(x) UNUSED__ ## x __attribute__((unused))
# else
#  define XP_UNUSED(x) x
# endif
#endif

#ifdef DEBUG
# define XP_UNUSED_DBG(x) x
#else
# define XP_UNUSED_DBG(x) XP_UNUSED(x)
#endif

#ifdef ENABLE_LOGGING
# define XP_UNUSED_LOG(x) x
#else
# define XP_UNUSED_LOG(x) XP_UNUSED(x)
#endif

#ifdef XWFEATURE_RELAY
#  define XP_UNUSED_RELAY(x) x
#else
#  define XP_UNUSED_RELAY(x) UNUSED__ ## x __attribute__((unused))
#endif

#ifdef COMMS_HEARTBEAT
#  define XP_UNUSED_HEARTBEAT(x) x
#else
#  define XP_UNUSED_HEARTBEAT(x) UNUSED__ ## x __attribute__((unused))
#endif

#ifdef XWFEATURE_BLUETOOTH
#  define XP_UNUSED_BT(x) x
#else
#  define XP_UNUSED_BT(x) UNUSED__ ## x __attribute__((unused))
#endif

#if BT_USE_RFCOMM
# define XP_UNUSED_RFCOMM(x) x
#else
# define XP_UNUSED_RFCOMM(x) UNUSED__ ## x __attribute__((unused))
#endif

#ifdef KEYBOARD_NAV
#  define XP_UNUSED_KEYBOARD_NAV(x) x
#else
#  define XP_UNUSED_KEYBOARD_NAV(x) UNUSED__ ## x __attribute__((unused))
#endif

#ifndef XWFEATURE_STANDALONE_ONLY
#  define XP_UNUSED_STANDALONE(x) x
#else
#  define XP_UNUSED_STANDALONE(x) UNUSED__ ## x __attribute__((unused))
#endif

#endif

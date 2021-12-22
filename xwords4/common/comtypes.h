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

#ifndef MAX_ROWS
# define MAX_ROWS 16
#endif
#define MAX_COLS MAX_ROWS

#define STREAM_VERS_SMALLCOMMS 0x1F
#define STREAM_VERS_NINETILES 0x1E
#define STREAM_VERS_NOEMPTYDICT 0x1D
#define STREAM_VERS_GICREATED 0x1C /* game struct gets created timestamp */
#define STREAM_VERS_DUPLICATE 0x1B
#define STREAM_VERS_DISABLEDS 0x1A
#define STREAM_VERS_DEVIDS 0x19
#define STREAM_VERS_MULTIADDR 0x18
#define STREAM_VERS_MODELDIVIDER 0x17
#define STREAM_VERS_COMMSBACKOFF 0x16
#define STREAM_VERS_DICTNAME 0x15
#ifdef HASH_STREAM 
# define STREAM_VERS_HASHSTREAM 0x14
#endif
#if MAX_COLS > 16
# define STREAM_VERS_BIGBOARD 0x13
#endif
#define STREAM_VERS_BLUETOOTH2 0x12
#define STREAM_SAVE_PREVWORDS 0x11
#define STREAM_VERS_SERVER_SAVES_TOSHOW 0x10
/* STREAM_VERS_PLAYERDICTS affects stream sent between devices.  May not be
   able to upgrade somebody who's this far back to something with
   STREAM_VERS_BIGBOARD defined.  It was added in rev 3b7b4802, on 2011-04-01,
   which makes it part of android_beta_25 (tag added Apr 29 2011).
 */
#define STREAM_VERS_PLAYERDICTS 0x0F
#define STREAM_SAVE_PREVMOVE 0x0E /* server saves prev move explanation */
#define STREAM_VERS_ROBOTIQ STREAM_SAVE_PREVMOVE /* robots have different smarts */
#define STREAM_VERS_DICTLANG 0x0D /* save dict lang code in CurGameInfo */
#define STREAM_VERS_NUNDONE 0x0C /* save undone tile in model */
#define STREAM_VERS_GAMESECONDS 0x0B /* save gameSeconds whether or not
                                        timer's enabled */
#define STREAM_VERS_4YOFFSET 0x0A /* 4 bits for yOffset on board */
#define STREAM_VERS_CHANNELSEED 0x09 /* new short in relay connect must be
                                        saved in comms */
#define STREAM_VERS_UTF8 0x08
#define STREAM_VERS_ALWAYS_MULTI 0x07 /* stream format same for multi and
                                         one-device game builds */
#define STREAM_VERS_MODEL_NO_DICT 0x06
#define STREAM_VERS_BLUETOOTH 0x05
#define STREAM_VERS_KEYNAV 0x04
#define STREAM_VERS_RELAY 0x03
#define STREAM_VERS_41B4 0x02
#define STREAM_VERS_405  0x01

/* search for FIX_NEXT_VERSION_CHANGE next time this is changed */
#define CUR_STREAM_VERS STREAM_VERS_SMALLCOMMS

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
    OBJ_TRAY,
    OBJ_TIMER,
} BoardObjectType;

enum {
    SERVER_STANDALONE,
    SERVER_ISSERVER,
    SERVER_ISCLIENT
};
typedef XP_U8 DeviceRole;

enum {
    PHONIES_IGNORE,
    /* You can commit a phony after viewing a warning  */
    PHONIES_WARN,
    /* You can commit a phony, but you'll lose your turn */
    PHONIES_DISALLOW,
    /* a phony is an illegal move, like tiles out-of-line */
    PHONIES_BLOCK,
};
typedef XP_U8 XWPhoniesChoice;

typedef XP_U8 XP_LangCode;

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
typedef struct XW_DUtilCtxt XW_DUtilCtxt;

/* Low two bits treated as channel, third as short-term flag indicating
 * sender's role; rest can be random to aid detection of duplicate packets. */
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
    TIMER_DUP_TIMERCHECK,
    NUM_TIMERS_PLUS_ONE          /* must be last */
} XWTimerReason;

#define MAX_NUM_PLAYERS 4
#define MIN_TRAY_TILES 7
#define MAX_TRAY_TILES 9
#define PLAYERNUM_NBITS 2
#define NDEVICES_NBITS 2        /* 1-4, but reduced by 1 fits in 2 bits */
#define NPLAYERS_NBITS 3
#define BINGO_BONUS 50

#if MAX_ROWS <= 16
typedef XP_U16 RowFlags;
#elif MAX_ROWS <= 32
typedef XP_U32 RowFlags;
#else
    error
#endif

typedef enum {
    BONUS_NONE,
    BONUS_DOUBLE_LETTER,
    BONUS_DOUBLE_WORD,
    BONUS_TRIPLE_LETTER,
    BONUS_TRIPLE_WORD,

    BONUS_LAST
} XWBonusType;

typedef enum _TileValueType {
    TVT_FACES,
    TVT_VALUES,
    TVT_BOTH,

    TVT_N_ENTRIES,
} TileValueType;

/* For now, let's define keys here. Old method based on __FILE__ was
 * stupid. Can be more clever later -- as long as these don't change again
*/

/* Partial keys allow us to recover old data using a KEY LIKE "%PARTIAL"
   query. Yuck, but better than resetting everybody!!!

   PENDING() remove this after a few months.
*/
#define SUFFIX_PARTIALS "partials"
#define SUFFIX_NEXTID "nextID"
#define SUFFIX_DEVSTATE "devState"
#define SUFFIX_MQTT_DEVID "mqtt_devid_key"
#define SUFFIX_KNOWN_PLAYERS "known_players_key_dev1"

#define FULL_KEY(PARTIAL) "persist_key:" PARTIAL

#define KEY_PARTIALS FULL_KEY(SUFFIX_PARTIALS)
#define KEY_NEXTID FULL_KEY(SUFFIX_NEXTID)
#define KEY_DEVSTATE FULL_KEY(SUFFIX_DEVSTATE)
#define MQTT_DEVID_KEY FULL_KEY(SUFFIX_MQTT_DEVID)
#define KNOWN_PLAYERS_KEY FULL_KEY(SUFFIX_KNOWN_PLAYERS)

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
    XP_Bool         sortNewTiles;    /* applies to all games */
#ifdef XWFEATURE_SLOW_ROBOT
    XP_U16          robotThinkMin, robotThinkMax;
    XP_U16          robotTradePct;
#endif
    XP_Bool         showColors; /* applies to all games */
    XP_Bool         allowPeek;  /* applies to all games */
#ifdef XWFEATURE_CROSSHAIRS
    XP_Bool         hideCrosshairs;  /* applies to all games */
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    XP_U16          makePhonyPct;
#endif
    TileValueType tvType;
} CommonPrefs;

typedef struct _PlayerDicts {
    const DictionaryCtxt* dicts[MAX_NUM_PLAYERS];
} PlayerDicts;

typedef uint64_t MQTTDevID;

#if __WORDSIZE == 64
# define MQTTDevID_FMT "%016lX"
#elif __WORDSIZE == 32
# define MQTTDevID_FMT "%016llX"
#endif
# define MQTTTopic_FMT "xw4/device/" MQTTDevID_FMT

/* Used by scoring code and engine as fast representation of moves. */
typedef struct _MoveInfoTile {
    XP_U8 varCoord; /* 5 bits ok (0-16 for 17x17 board) */
    Tile tile;      /* 6 bits will do */
} MoveInfoTile;

typedef struct _MoveInfo {
    XP_U8 nTiles;         /* 4 bits: 0-7 */
    XP_U8 commonCoord;    /* 5 bits: 0-16 if 17x17 possible */
    XP_Bool isHorizontal; /* 1 bit */
    /* If this is to go on an undo stack, we need player num here, or the code
       has to keep track of it *and* there must be exactly one entry per
       player per turn. */
    MoveInfoTile tiles[MAX_TRAY_TILES];
} MoveInfo;

typedef struct _LastMoveInfo {
    const XP_UCHAR* names[MAX_NUM_PLAYERS];
    XP_U16 nWinners;            /* >1 possible in duplicate case only */
    XP_U16 score;
    XP_U16 nTiles;
    XP_UCHAR word[MAX_COLS * 2]; /* be safe */
    XP_U8 moveType;
    XP_Bool inDuplicateMode;
} LastMoveInfo;

typedef XP_U8 TrayTile;
typedef struct _TrayTileSet {
    XP_U8 nTiles;
    TrayTile tiles[MAX_TRAY_TILES];
} TrayTileSet;

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

#define OFFSET_OF(typ,var)  ((XP_U32)&(((typ*) 0)->var))

#ifndef RELAY_NAME_DEFAULT
# define RELAY_NAME_DEFAULT "eehouse.org"
#endif
#ifndef RELAY_ROOM_DEFAULT
# define RELAY_ROOM_DEFAULT "Room 1"
#endif
#ifndef RELAY_PORT_DEFAULT
# define RELAY_PORT_DEFAULT 10997
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
# define XP_FREEP(pool,pp) \
    mpool_freep((pool), (void**)(pp),__FILE__,__func__,__LINE__)

# define MPFORMAL_NOCOMMA MemPoolCtx* mpool
# define MPFORMAL         MPFORMAL_NOCOMMA,
# define MPSLOT           MPFORMAL_NOCOMMA;
# define MPPARM_NOCOMMA(p)  (p)
# define MPPARM(p)          MPPARM_NOCOMMA(p),
# define MPASSIGN(slot,val) (slot)=(val)

#else

# define MPFORMAL_NOCOMMA void
# define MPFORMAL
# define MPSLOT
# define MPPARM_NOCOMMA(p)
# define MPPARM(p)
# define MPASSIGN(slot,val)

#endif

#define LOG_FUNC()  XP_LOGFF( "IN" )
#define LOG_RETURNF(fmt, ...)  XP_LOGFF( "OUT: => " fmt, __VA_ARGS__ )
#define LOG_RETURN_VOID() LOG_RETURNF("%s","void")
#define XP_LOGLOC() XP_LOGF( "%s(), line %d", __func__, __LINE__ )
#define LOG_POS(strm) XP_LOGF( "%s(); line %d; read_pos: %X", __func__, __LINE__, \
                               stream_getPos((strm), POS_READ) )

#ifndef XP_USE
# define XP_USE(v) v=v
#endif

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

#ifdef BT_USE_RFCOMM
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

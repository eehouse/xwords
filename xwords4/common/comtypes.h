/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2000 by Eric House (fixin@peak.org).  All rights reserved.
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

typedef struct XP_Rect {
    XP_S16 left;
    XP_S16 top;
    XP_S16 width;
    XP_S16 height;
} XP_Rect;

typedef XP_U16 CellTile;

typedef XP_U8 Tile;

typedef void* XP_Bitmap;

typedef XP_UCHAR XP_UCHAR4[4];

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

typedef XP_U16 XP_PlayerAddr;

typedef enum {
    TIMER_PENDOWN = 1, /* ARM doesn't like ids of 0... */
    TIMER_TIMERTICK,
#ifdef BEYOND_IR
    TIMER_HEARTBEAT,
#endif

    NUM_TIMERS_PLUS_ONE          /* must be last */
} XWTimerReason;

#define MAX_NUM_PLAYERS 4
#define PLAYERNUM_NBITS 2
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
    XP_Bool         reserved1;       /* get to 32-bit for ARM... */
    XP_Bool         reserved2;
} CommonPrefs;

/* used for all vtables */
#define SET_VTABLE_ENTRY( vt, name, prefix ) \
         (vt)->m_##name = prefix##_##name

#ifdef DEBUG
# define DEBUG_ASSIGN(a,b) (a) = (b)
#else
# define DEBUG_ASSIGN(a,b)
#endif

#define OFFSET_OF(typ,var)  ((XP_U16)&(((typ*) 0)->var))


#ifdef MEM_DEBUG
# define XP_MALLOC(pool,nbytes) \
      mpool_alloc((pool),(nbytes),__FILE__,__LINE__)
# define XP_REALLOC(pool,p,s)         mpool_realloc((pool),(p),(s))
# define XP_FREE(pool,p)              mpool_free((pool), (p), __FILE__, __LINE__)

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

#endif

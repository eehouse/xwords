/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "util.h"
#include "game.h"
#include "dbgutil.h"
#include "timers.h"
#include "gamerefp.h"

XW_UtilCtxt*
check_uc(XW_UtilCtxt* uc)
{
    XP_ASSERT( uc->_inited );
    return uc;
}

const CurGameInfo*
util_getGI( XW_UtilCtxt* util )
{
    XP_ASSERT( !!util->gi );
    return util->gi;
}

struct UtilTimerState {
    TimerKey keys[NUM_TIMERS_PLUS_ONE];
};

static UtilTimerState*
initTimersOnce( XW_UtilCtxt* util )
{
    if ( !util->uts ) {
#ifdef MEM_DEBUG
        MemPoolCtx* mpool = util->_mpool;
#endif
        UtilTimerState* uts = XP_CALLOC( mpool, sizeof(*uts) );
        util->uts = uts;
    }
    return util->uts;
}

static void
clearTimers( XW_UtilCtxt* util )
{
    UtilTimerState* uts = util->uts;
    for ( int ii = 0; ii < VSIZE(uts->keys); ++ii ) {
        if ( uts->keys[ii] ) {
            XP_LOGFF( "unfired timer why = %d", uts->keys[ii] );
        }
    }
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = util->_mpool;
#endif
    XP_FREE( mpool, util->uts );
}

typedef struct _TimerClosure {
    UtilTimerState* uts;
    UtilTimerProc utilProc;
    void* utilClosure;
    XWTimerReason why;
} TimerClosure;

static void
timerProc( XW_DUtilCtxt* XP_UNUSED_DBG(dutil), XWEnv xwe, void* closure,
           TimerKey key, XP_Bool fired )
{
    XP_ASSERT( key );
    TimerClosure* tc = (TimerClosure*)closure;
    XWTimerReason why = tc->why;
    UtilTimerState* uts = tc->uts;
    /* XP_LOGFF("(key:%d, fired: %s); why: %s", key, boolToStr(fired), */
    /*          whyToStr(why) ); */
    if ( uts->keys[why] ) {
        if ( key == uts->keys[why] ) {
            uts->keys[why] = 0;
            if ( fired ) {
                (*tc->utilProc)( tc->utilClosure, xwe, why );
            }
        } else {
            XP_LOGFF( "dropping timer: key mismatch: got %d, expected %d",
                      key, uts->keys[why] );
            // XP_ASSERT(0);       /*  ?? might not be an error */
        }
    }
    XP_FREE( dutil->mpool, tc );
}

void
util_setTimer( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why,
               XP_U16 inSecs, UtilTimerProc proc, void* closure )
{
    UtilTimerState* uts = initTimersOnce( uc );
    XW_DUtilCtxt* dutil = util_getDevUtilCtxt(uc);
    XP_U32 inWhenMS = inSecs * 1000;
    TimerClosure* tc = XP_CALLOC( dutil->mpool, sizeof(*tc) );
    tc->uts = uts;
    tc->utilProc = proc;
    tc->utilClosure = closure;
    tc->why = why;
    TimerKey key = tmr_set( dutil, xwe, inWhenMS, timerProc, tc );
    if ( uts->keys[why] ) {
        XP_LOGFF( "keys[%s] already set; am I leaking", whyToStr(why) );
    }
    uts->keys[why] = key;
    /* XP_LOGFF( "set timer why=%s for %d s with key %d", whyToStr(why), */
    /*           inSecs, key ); */
}

void
util_clearTimer( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWTimerReason why )
{
    UtilTimerState* uts = initTimersOnce( uc );
    TimerKey key = uts->keys[why];
    if ( key ) {
        XP_LOGFF( "clearing timer why %d key %d", why, key );
        uts->keys[why] = 0;
    } else {
        XP_LOGFF( "no timer why %d", why );
    }
    /* if ( key ) { */
    /*     tmr_clear( util_getDevUtilCtxt(uc), xwe, key ); */
    /* } */
    XP_LOGFF("(why: %d)", why );
}

/* Eventually this should go away and at some point the game's CurGameInfo
   should be updated via ctrl_inviteeName() to hold names that can be used
   to display -- that will be replaced when invitees register with a
   potentially different name. */
void
util_getInviteeName( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 plyrNum,
                     XP_UCHAR* buf, XP_U16* bufLen )
{
    XP_USE(uc);
    XP_USE(xwe);
    XP_SNPRINTF( buf, *bufLen, "InvitEE %d", plyrNum );
}

void
util_super_init( MPFORMAL XW_UtilCtxt* util, const CurGameInfo* gi,
                 XW_DUtilCtxt* dutil, GameRef gr, UtilDestroy dp )
{
    XP_ASSERT( !!gi );
    MPASSIGN( util->_mpool, mpool );
#ifdef MEM_DEBUG
    XP_USE( mpool );
#endif
    util->vtable->m_util_destroy = dp;
    util->gi = gi;
    util->dutil = dutil;
#ifdef DEBUG
    util->_inited = XP_TRUE;
#endif
    util->gr = gr;
    initTimersOnce(util);
    XP_ASSERT( util->refCount == 0 );
    util->refCount = 1;
}

XW_UtilCtxt*
_util_ref( XW_UtilCtxt* util
#ifdef DEBUG
                    ,const char* proc, int line
#endif
                    )
{
    if ( !!util ) {
        ++util->refCount;
        XP_LOGFF( "refCount now %d (from %s(), line %d)",
                  util->refCount, proc, line );
    }
    return util;
}

void
_util_unref( XW_UtilCtxt* util, XWEnv xwe
#ifdef DEBUG
                    ,const char* proc, int line
#endif
                  )
{
    if ( !!util ) {
        XP_ASSERT( 0 < util->refCount );
        --util->refCount;
        XP_LOGFF( "refCount now %d (from %s(), line %d)",
                  util->refCount, proc, line );
        if ( !util->refCount ) {
            util_destroy( util, xwe );
        }
    }
}

void
util_super_cleanup( XW_UtilCtxt* util, XWEnv XP_UNUSED(xwe) )
{
    clearTimers( util );
}

#ifdef MEM_DEBUG
MemPoolCtx*
util_getMemPool( const XW_UtilCtxt* util, XWEnv xwe )
{
    XP_ASSERT( !!util );
    XP_ASSERT( !!util->gr );
    return gr_getMemPool( util_getDevUtilCtxt(util), util->gr, xwe );
}
#endif

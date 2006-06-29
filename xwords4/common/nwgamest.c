/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2006 by Eric House (xwords@eehouse.org).  All rights
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


#include "nwgamest.h"
#include "strutils.h"
#include "LocalizedStrIncludes.h"

#ifdef CPLUS
extern "C" {
#endif

#define NG_NUM_COLS ((XP_U16)(NG_COL_PASSWD+1))

struct NewGameCtx {
    NewGameEnableColProc enableColProc;
    NewGameEnableAttrProc enableAttrProc;
    NewGameSetColProc setColProc;
    NewGameGetColProc getColProc;
    NewGameSetAttrProc setAttrProc;
    XW_UtilCtxt* util;
    void* closure;

    /* Palm needs to store cleartext passwords separately in order to
       store '***' in the visible field */
    NewGameEnable enabled[NG_NUM_COLS][MAX_NUM_PLAYERS];
    XP_U16 nPlayers;
#ifndef XWFEATURE_STANDALONE_ONLY
    Connectedness role;
#endif
    XP_Bool isNewGame;
    NewGameEnable juggleEnabled;

    MPSLOT
};

static void enableOne( NewGameCtx* ngc, XP_U16 player, NewGameColumn col,
                       NewGameEnable enable, XP_Bool force );
static void adjustAllRows( NewGameCtx* ngc, XP_Bool force );
static void adjustOneRow( NewGameCtx* ngc, XP_U16 player, XP_Bool force );
static void setRoleStrings( NewGameCtx* ngc );
static void considerEnableJuggle( NewGameCtx* ngc );
static void storePlayer( NewGameCtx* ngc, XP_U16 player, LocalPlayer* lp );
static void loadPlayer( NewGameCtx* ngc, XP_U16 player, 
                        const LocalPlayer* lp );

NewGameCtx*
newg_make( MPFORMAL XP_Bool isNewGame, 
           XW_UtilCtxt* util,
           NewGameEnableColProc enableColProc, 
           NewGameEnableAttrProc enableAttrProc, 
           NewGameGetColProc getColProc, NewGameSetColProc setColProc,
           NewGameSetAttrProc setAttrProc, void* closure )
{
    NewGameCtx* result = XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    result->enableColProc = enableColProc;
    result->enableAttrProc = enableAttrProc;
    result->setColProc = setColProc;
    result->getColProc = getColProc;
    result->setAttrProc = setAttrProc;
    result->closure = closure;
    result->isNewGame = isNewGame;
    result->util = util;
    MPASSIGN(result->mpool, mpool);

    return result;
} /* newg_make */

void
newg_destroy( NewGameCtx* ngc )
{
    XP_FREE( ngc->mpool, ngc );
} /* newg_destroy */

void
newg_load( NewGameCtx* ngc, const CurGameInfo* gi )
{
    void* closure = ngc->closure;
    NGValue value;
    XP_U16 i;

    ngc->nPlayers = gi->nPlayers;
    value.ng_u16 = ngc->nPlayers;
    (*ngc->setAttrProc)( closure, NG_ATTR_NPLAYERS, value );
    (*ngc->enableAttrProc)( closure, NG_ATTR_NPLAYERS, ngc->isNewGame?
                            NGEnableEnabled : NGEnableDisabled );

#ifndef XWFEATURE_STANDALONE_ONLY
    ngc->role = gi->serverRole;
    value.ng_role = ngc->role;
    (*ngc->setAttrProc)( closure, NG_ATTR_ROLE, value );
    (*ngc->enableAttrProc)( closure, NG_ATTR_ROLE, ngc->isNewGame? 
                            NGEnableEnabled : NGEnableDisabled );
#endif
    setRoleStrings( ngc );
    considerEnableJuggle( ngc );   

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        loadPlayer( ngc, i, &gi->players[i] );
    }
    
    adjustAllRows( ngc, XP_TRUE );
} /* newg_load */

typedef struct NGCopyClosure {
    XP_U16 player;
    NewGameColumn col;
    NewGameCtx* ngc;
    LocalPlayer* lp;
} NGCopyClosure;

static void
cpToLP( NGValue value, const void* cbClosure )
{
    NGCopyClosure* cpcl = (NGCopyClosure*)cbClosure;
    LocalPlayer* lp = cpcl->lp;
    XP_UCHAR** strAddr = NULL;
    
    switch ( cpcl->col ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
        lp->isLocal = !value.ng_bool;
        break;
#endif
    case NG_COL_NAME:
        strAddr = &lp->name;
        break;
    case NG_COL_PASSWD:
        strAddr = &lp->password;
        break;
    case NG_COL_ROBOT:
        lp->isRobot = value.ng_bool;
        break;
    }

    if ( !!strAddr ) {
        replaceStringIfDifferent( MPPARM(cpcl->ngc->mpool) strAddr,
                                  value.ng_cp );
    }
} /* cpToLP */

void
newg_store( NewGameCtx* ngc, CurGameInfo* gi )
{
    XP_U16 player;

    gi->nPlayers = ngc->nPlayers;
#ifndef XWFEATURE_STANDALONE_ONLY
    gi->serverRole = ngc->role;
#endif

    for ( player = 0; player < MAX_NUM_PLAYERS; ++player ) {
        storePlayer( ngc, player, &gi->players[player] );
    }
} /* newg_store */

void
newg_colChanged( NewGameCtx* ngc, XP_U16 player, NewGameColumn col, 
                 NGValue value )
{
    /* Sometimes we'll get this notification for inactive rows, e.g. when
       setting default values. */
    if ( player < ngc->nPlayers ) {
        adjustOneRow( ngc, player, XP_FALSE );
    }
}

void
newg_attrChanged( NewGameCtx* ngc, NewGameAttr attr, NGValue value )
{
    if ( attr == NG_ATTR_NPLAYERS ) {
        ngc->nPlayers = value.ng_u16;
        considerEnableJuggle( ngc );
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( NG_ATTR_ROLE == attr ) { 
        ngc->role = value.ng_role;
        setRoleStrings( ngc );
#endif
    } else {
        XP_ASSERT( 0 );
    }
    adjustAllRows( ngc, XP_FALSE );
}

typedef struct DeepValue {
    NGValue value;
    NewGameColumn col;
    MPSLOT
} DeepValue;

static void
deepCopy( NGValue value, const void* closure )
{
    DeepValue* dvp = (DeepValue*)closure;
    switch ( dvp->col ) {
    case NG_COL_ROBOT:
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
        dvp->value.ng_bool = value.ng_bool;
        break;
    case NG_COL_NAME:
    case NG_COL_PASSWD:
        dvp->value.ng_cp = copyString( MPPARM(dvp->mpool) value.ng_cp );
        break;
    }
}

XP_Bool
newg_juggle( NewGameCtx* ngc )
{
    XP_Bool changed = XP_FALSE;
    XP_U16 nPlayers = ngc->nPlayers;
    
    if ( nPlayers > 1 ) {
        LocalPlayer tmpPlayers[MAX_NUM_PLAYERS];
        XP_U16 pos[MAX_NUM_PLAYERS];
        XP_U16 player;

        /* Get a randomly juggled array of numbers 0..nPlayers-1.  Then the
           number at pos[n] inicates where the entry currently at n should
           be. */
        changed = randIntArray( pos, nPlayers );
        if ( changed ) {

            /* Deep-copy off to tmp storage.  But skip lines that won't be moved
               in the juggle. */
            XP_MEMSET( &tmpPlayers, 0, sizeof(tmpPlayers) );
            for ( player = 0; player < nPlayers; ++player ) {
                if ( player != pos[player] ) {
                    storePlayer( ngc, player, &tmpPlayers[player] );
                }
            }

            for ( player = 0; player < nPlayers; ++player ) {
                if ( player != pos[player] ) {
                    LocalPlayer* lp = &tmpPlayers[player];
                    loadPlayer( ngc, pos[player], lp );
                    if ( !!lp->name ) {
                        XP_FREE( ngc->mpool, lp->name );
                    }
                    if ( !!lp->password ) {
                        XP_FREE( ngc->mpool, lp->password );
                    }
                }
            }
        }
    }
    return changed;
} /* newg_juggle */

static void
enableOne( NewGameCtx* ngc, XP_U16 player, NewGameColumn col, 
           NewGameEnable enable, XP_Bool force )
{
    NewGameEnable* esp = &ngc->enabled[col][player];
    if ( force || (*esp != enable) ) {
        (*ngc->enableColProc)( ngc->closure, player, col, enable );
    }
    *esp = enable;
} /* enableOne */

static void
adjustAllRows( NewGameCtx* ngc, XP_Bool force )
{
    XP_U16 player;
    for ( player = 0; player < MAX_NUM_PLAYERS; ++player ) {
        adjustOneRow( ngc, player, force );
    }
} /* adjustAllRows */

static void
adjustOneRow( NewGameCtx* ngc, XP_U16 player, XP_Bool force )
{
    NewGameEnable enable[NG_NUM_COLS];
    NewGameColumn col;
    XP_Bool isLocal = XP_TRUE;
    DeepValue dValue;

    for ( col = 0; col < NG_NUM_COLS; ++col ) {
        enable[col] = NGEnableHidden;
    }

    /* If there aren't this many players, all are disabled */
    if ( player >= ngc->nPlayers ) {
        /* do nothing: all are hidden above */
    } else {
#ifndef XWFEATURE_STANDALONE_ONLY
        /* If standalone or client, remote is hidden.  If server but not
           new game, it's disabled */
        if ( ngc->role == SERVER_ISSERVER ) {
            if ( ngc->isNewGame ) {
                enable[NG_COL_REMOTE] = NGEnableEnabled;
            } else {
                enable[NG_COL_REMOTE] = NGEnableDisabled;
            }
            dValue.col = NG_COL_REMOTE;
            (*ngc->getColProc)( ngc->closure, player, NG_COL_REMOTE,
                                deepCopy, &dValue );
            isLocal = !dValue.value.ng_bool;
        }
#endif

        /* If remote is enabled and set, then if it's a new game all else is
           hidden.  But if it's not a new game, they're disabled.  Password is
           always hidden if robot is set. */
        if ( isLocal ) {
            /* No changing name or robotness since they're sent to remote
               host. */
            enable[NG_COL_NAME] = NGEnableEnabled;
            enable[NG_COL_ROBOT] = NGEnableEnabled;

            dValue.col = NG_COL_ROBOT;
            (*ngc->getColProc)( ngc->closure, player, NG_COL_ROBOT, deepCopy,
                                &dValue );
            if ( !dValue.value.ng_bool ) {
                /* It is's a robot, leave it hidden */
                enable[NG_COL_PASSWD] = NGEnableEnabled;
            }
                                  
        } else {
            if ( ngc->isNewGame ) {
                /* leave 'em hidden */
            } else {
                enable[NG_COL_NAME] = NGEnableDisabled;
                enable[NG_COL_ROBOT] = NGEnableDisabled;
                /* leave passwd hidden */
            }
        }
    }

    for ( col = 0; col < NG_NUM_COLS; ++col ) {
        enableOne( ngc, player, col, enable[col], force );
    }
} /* adjustOneRow */

static void
setRoleStrings( NewGameCtx* ngc )
{
    XP_U16 strID;
    NGValue value;
    void* closure = ngc->closure;
    /* Tell client to set/change players label text, and also to add remote
     checkbox column header if required. */

#ifndef XWFEATURE_STANDALONE_ONLY
    (*ngc->enableAttrProc)( closure, NG_ATTR_REMHEADER, 
                            ngc->role == SERVER_ISSERVER?
                            NGEnableEnabled : NGEnableHidden );
#endif

    if ( 0 ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( ngc->role == SERVER_ISCLIENT ) {
        strID = STR_LOCALPLAYERS;
#endif
    } else {
        strID = STR_TOTALPLAYERS;
    }

    value.ng_cp = util_getUserString( ngc->util, strID );
    (*ngc->setAttrProc)( closure, NG_ATTR_NPLAYHEADER, value );
} /* setRoleStrings */

static void
considerEnableJuggle( NewGameCtx* ngc )
{
    NewGameEnable newEnable;
    newEnable = (ngc->isNewGame && ngc->nPlayers > 1)?
        NGEnableEnabled : NGEnableDisabled;

    if ( newEnable != ngc->juggleEnabled ) {
        (*ngc->enableAttrProc)( ngc->closure, NG_ATTR_CANJUGGLE, newEnable );
        ngc->juggleEnabled = newEnable;
    }
} /* considerEnableJuggle */

static void
storePlayer( NewGameCtx* ngc, XP_U16 player, LocalPlayer* lp )
{
    void* closure = ngc->closure;
    NGCopyClosure cpcl;
    cpcl.player = player;
    cpcl.ngc = ngc;
    cpcl.lp = lp;

    for ( cpcl.col = 0; cpcl.col < NG_NUM_COLS; ++cpcl.col ) {
        (*ngc->getColProc)( closure, cpcl.player, cpcl.col, 
                            cpToLP, &cpcl );
    }
}

static void
loadPlayer( NewGameCtx* ngc, XP_U16 player, const LocalPlayer* lp )
{
    NGValue value;
    void* closure = ngc->closure;

#ifndef XWFEATURE_STANDALONE_ONLY
    value.ng_bool = !lp->isLocal;
    (*ngc->setColProc)(closure, player, NG_COL_REMOTE, value );
#endif
    value.ng_cp = lp->name;
    (*ngc->setColProc)(closure, player, NG_COL_NAME, value );

    value.ng_cp = lp->password;
    (*ngc->setColProc)(closure, player, NG_COL_PASSWD, value );

    value.ng_bool = lp->isRobot;
    (*ngc->setColProc)(closure, player, NG_COL_ROBOT, value );
}

#ifdef CPLUS
}
#endif

/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE"; -*- */
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
    XP_TriEnable enabled[NG_NUM_COLS][MAX_NUM_PLAYERS];
    XP_U16 nPlayersShown;       /* real nPlayers lives in gi */
    XP_U16 nPlayersTotal;       /* used only until changedNPlayers set */
    XP_U16 nLocalPlayers;       /* not changed except in newg_load */
    DeviceRole role;
    XP_Bool isNewGame;
    XP_Bool changedNPlayers;
    XP_TriEnable juggleEnabled;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_TriEnable settingsEnabled;
#endif

    MPSLOT
};

static void enableOne( NewGameCtx* ngc, XP_U16 player, NewGameColumn col,
                       XP_TriEnable enable, XP_Bool force );
static void adjustAllRows( NewGameCtx* ngc, XP_Bool force );
static void adjustOneRow( NewGameCtx* ngc, XP_U16 player, XP_Bool force );
static void setRoleStrings( NewGameCtx* ngc );
static void considerEnable( NewGameCtx* ngc );
static void storePlayer( NewGameCtx* ngc, XP_U16 player, LocalPlayer* lp );
static void loadPlayer( NewGameCtx* ngc, XP_U16 player, 
                        const LocalPlayer* lp );
#ifndef XWFEATURE_STANDALONE_ONLY
static XP_Bool changeRole( NewGameCtx* ngc, DeviceRole role );
static XP_Bool checkConsistent( NewGameCtx* ngc, XP_Bool warnUser );
#else
# define checkConsistent( ngc, warnUser ) XP_TRUE
#endif

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
    XP_U16 nPlayers, nLoaded;
    XP_S16 ii, jj;
    DeviceRole role;
    XP_Bool localOnly;
    XP_Bool shown[MAX_NUM_PLAYERS] = { XP_FALSE, XP_FALSE, XP_FALSE, XP_FALSE};

    ngc->juggleEnabled = TRI_ENAB_NONE;
    ngc->settingsEnabled = TRI_ENAB_NONE;
    for ( ii = 0; ii < NG_NUM_COLS; ++ii ) {
        for ( jj = 0; jj < MAX_NUM_PLAYERS; ++jj ) {
            ngc->enabled[ii][jj] = TRI_ENAB_NONE;
        }
    }

    ngc->role = role = gi->serverRole;
    localOnly = (role == SERVER_ISCLIENT) && ngc->isNewGame;
#ifndef XWFEATURE_STANDALONE_ONLY
    value.ng_role = role;
    (*ngc->setAttrProc)( closure, NG_ATTR_ROLE, value );
    (*ngc->enableAttrProc)( closure, NG_ATTR_ROLE, ngc->isNewGame? 
                            TRI_ENAB_ENABLED : TRI_ENAB_DISABLED );
#endif

    nPlayers = gi->nPlayers;
    ngc->nPlayersTotal = nPlayers;
#ifndef XWFEATURE_STANDALONE_ONLY
    for ( ii = nPlayers - 1; ii >= 0; --ii ) {
        if ( gi->players[ii].isLocal ) {
            ++ngc->nLocalPlayers;
        }
    }
#endif
    if ( localOnly ) {
        nPlayers = ngc->nLocalPlayers;
    }
    ngc->nPlayersShown = nPlayers;
        
    value.ng_u16 = ngc->nPlayersShown;
    (*ngc->setAttrProc)( closure, NG_ATTR_NPLAYERS, value );
    (*ngc->enableAttrProc)( closure, NG_ATTR_NPLAYERS, ngc->isNewGame?
                            TRI_ENAB_ENABLED : TRI_ENAB_DISABLED );

    setRoleStrings( ngc );
    considerEnable( ngc );   

    /* Load local players first */
    nLoaded = 0;
    do {
        for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
            if ( !shown[ii] ) {
                const LocalPlayer* lp = &gi->players[ii];
                if ( !localOnly
                     || (lp->isLocal && (nLoaded < ngc->nLocalPlayers)) ) {
                    shown[ii] = XP_TRUE;
                    loadPlayer( ngc, nLoaded++, lp );
                }
            }
        }
        XP_ASSERT( localOnly || nLoaded == MAX_NUM_PLAYERS );
        localOnly = XP_FALSE;   /* for second pass */
    } while ( nLoaded < MAX_NUM_PLAYERS );
    
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
        /* This is leaking!!!  But doesn't leak if am playing via IR, at least
           in the simulator.  */
        replaceStringIfDifferent( cpcl->ngc->mpool, strAddr,
                                  value.ng_cp );
    }
} /* cpToLP */

XP_Bool
newg_store( NewGameCtx* ngc, CurGameInfo* gi, 
            XP_Bool XP_UNUSED_STANDALONE(warn) )
{
    XP_U16 player;
    XP_Bool consistent = checkConsistent( ngc, warn );

    if ( consistent ) {
        XP_Bool makeLocal = XP_FALSE;
        gi->nPlayers = ngc->nPlayersShown;
#ifndef XWFEATURE_STANDALONE_ONLY
        gi->serverRole = ngc->role;
        makeLocal = ngc->role != SERVER_ISSERVER;
#endif

        for ( player = 0; player < MAX_NUM_PLAYERS; ++player ) {
            storePlayer( ngc, player, &gi->players[player] );
            if ( makeLocal ) {
                gi->players[player].isLocal = XP_TRUE;
            }
        }
    }
    return consistent;
} /* newg_store */

void
newg_colChanged( NewGameCtx* ngc, XP_U16 player )
{
    /* Sometimes we'll get this notification for inactive rows, e.g. when
       setting default values. */
    if ( player < ngc->nPlayersShown ) {
        adjustOneRow( ngc, player, XP_FALSE );
    }
}

void
newg_attrChanged( NewGameCtx* ngc, NewGameAttr attr, NGValue value )
{
    XP_Bool changed = XP_FALSE;
    if ( attr == NG_ATTR_NPLAYERS ) {
        if ( ngc->nPlayersShown != value.ng_u16 ) {
            ngc->nPlayersShown = value.ng_u16;
            ngc->changedNPlayers = XP_TRUE;
            changed = XP_TRUE;
        }
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( NG_ATTR_ROLE == attr ) { 
        changed = changeRole( ngc, value.ng_role );
#endif
    } else {
        XP_ASSERT( 0 );
    }

    if ( changed ) {
        considerEnable( ngc );
        adjustAllRows( ngc, XP_FALSE );
    }
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
        dvp->value.ng_cp = copyString( dvp->mpool, value.ng_cp );
        break;
    }
}

XP_Bool
newg_juggle( NewGameCtx* ngc )
{
    XP_Bool changed = XP_FALSE;
    XP_U16 nPlayers = ngc->nPlayersShown;

    XP_ASSERT( ngc->isNewGame );
    
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
                    XP_U16 dest = pos[player];

                    loadPlayer( ngc, dest, lp );

                    if ( !!lp->name ) {
                        XP_FREE( ngc->mpool, lp->name );
                    }
                    if ( !!lp->password ) {
                        XP_FREE( ngc->mpool, lp->password );
                    }

                    adjustOneRow( ngc, dest, XP_FALSE );
                }
            }
        }
    }
    return changed;
} /* newg_juggle */

#ifndef XWFEATURE_STANDALONE_ONLY
static XP_Bool
checkConsistent( NewGameCtx* ngc, XP_Bool warnUser )
{
    XP_Bool consistent;
    XP_U16 i;

    /* If ISSERVER, make sure there's at least one non-local player. */
    consistent = ngc->role != SERVER_ISSERVER;
    for ( i = 0; !consistent && i < ngc->nPlayersShown; ++i ) {
        DeepValue dValue;
        dValue.col = NG_COL_REMOTE;
        (*ngc->getColProc)( ngc->closure, i, NG_COL_REMOTE,
                            deepCopy, &dValue );
        if ( dValue.value.ng_bool ) {
            consistent = XP_TRUE;
        }
    }
    if ( !consistent && warnUser ) {
        util_userError( ngc->util, ERR_REG_SERVER_SANS_REMOTE );
    }

    /* Add other consistency checks, and error messages, here. */

    return consistent;
} /* checkConsistent */
#endif

static void
enableOne( NewGameCtx* ngc, XP_U16 player, NewGameColumn col, 
           XP_TriEnable enable, XP_Bool force )
{
    XP_TriEnable* esp = &ngc->enabled[col][player];
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
    XP_TriEnable enable[NG_NUM_COLS];
    NewGameColumn col;
    XP_Bool isLocal = XP_TRUE;
    XP_Bool isNewGame = ngc->isNewGame;
    DeviceRole role = ngc->role;
    DeepValue dValue;

    for ( col = 0; col < NG_NUM_COLS; ++col ) {
        enable[col] = TRI_ENAB_HIDDEN;
    }

    /* If there aren't this many players, all are disabled */
    if ( player >= ngc->nPlayersShown ) {
        /* do nothing: all are hidden above */
    } else {
#ifndef XWFEATURE_STANDALONE_ONLY
        /* If standalone or client, remote is hidden.  If server but not
           new game, it's disabled */
        if ( (role == SERVER_ISSERVER ) 
             || (role == SERVER_ISCLIENT && !isNewGame ) ) {
            if ( isNewGame ) {
                enable[NG_COL_REMOTE] = TRI_ENAB_ENABLED;
            } else {
                enable[NG_COL_REMOTE] = TRI_ENAB_DISABLED;
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
            XP_TriEnable tmp;

            /* No changing name or robotness since they're sent to remote
               host. */
            tmp = (isNewGame || role == SERVER_STANDALONE)? 
                TRI_ENAB_ENABLED:TRI_ENAB_DISABLED;
            enable[NG_COL_NAME] = tmp;
            enable[NG_COL_ROBOT] = tmp;

            /* Password and game info (the not isNewGame case): passwords are
               not transmitted: they're local only.  There's no harm in
               allowing local players to change them.  So passwords should be
               enabled whenever it's not a robot regardless of both isNewGame
               and role. */

            dValue.col = NG_COL_ROBOT;
            (*ngc->getColProc)( ngc->closure, player, NG_COL_ROBOT, deepCopy,
                                &dValue );
            if ( !dValue.value.ng_bool ) {
                /* If it's a robot, leave it hidden */
                enable[NG_COL_PASSWD] = TRI_ENAB_ENABLED;
            }
                                  
        } else {
            if ( isNewGame ) {
                /* leave 'em hidden */
            } else {
                enable[NG_COL_NAME] = TRI_ENAB_DISABLED;
                enable[NG_COL_ROBOT] = TRI_ENAB_DISABLED;
                /* leave passwd hidden */
            }
        }
    }

    for ( col = 0; col < NG_NUM_COLS; ++col ) {
        enableOne( ngc, player, col, enable[col], force );
    }
} /* adjustOneRow */

/* changeRole.  When role changes, number of players displayed, and which
 * players, may change.  Host shows all players (up to nPlayers).  Guest shows
 * only local players, but if role changes should show the rest.  Change from
 * Host or Standalone to guest should reduce the number shown.
 * 
 * Here's the fun part: what happens when user changes nPlayers, then changes
 * role?  Say we're a guest with one player.  User makes it two, than makes us
 * host.  Do we pull in a new player?  No.  Let's not change any of this stuff
 * ONCE USER'S CHANGED NPLAYERS.  Goal is to prevent his having to do that for
 * the most common case, which is playing again with the same players.  In
 * that case changing role then back again should not lose/change data.
 */
#ifndef XWFEATURE_STANDALONE_ONLY
static XP_Bool
changeRole( NewGameCtx* ngc, DeviceRole newRole )
{
    DeviceRole oldRole = ngc->role;
    XP_Bool changing = oldRole != newRole;
    if ( changing ) {
        if ( !ngc->changedNPlayers ) {
            NGValue value;
            if ( newRole == SERVER_ISCLIENT ) {
                value.ng_u16 = ngc->nLocalPlayers;
            } else {
                value.ng_u16 = ngc->nPlayersTotal;
            }
            if ( value.ng_u16 != ngc->nPlayersShown ) {
                ngc->nPlayersShown = value.ng_u16;
                (*ngc->setAttrProc)( ngc->closure, NG_ATTR_NPLAYERS, value );
            }
        }
        ngc->role = newRole;
        setRoleStrings( ngc );
    }
    return changing;
}
#endif

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
                            ( (ngc->role == SERVER_ISSERVER)
                              || (!ngc->isNewGame
                                  && (ngc->role != SERVER_STANDALONE)) )?
                            TRI_ENAB_ENABLED : TRI_ENAB_HIDDEN );
#endif

    if ( 0 ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( ngc->role == SERVER_ISCLIENT && ngc->isNewGame ) {
        strID = STR_LOCALPLAYERS;
#endif
    } else {
        strID = STR_TOTALPLAYERS;
    }

    value.ng_cp = util_getUserString( ngc->util, strID );
    (*ngc->setAttrProc)( closure, NG_ATTR_NPLAYHEADER, value );
} /* setRoleStrings */

static void
considerEnable( NewGameCtx* ngc )
{
    XP_TriEnable newEnable;
    newEnable = (ngc->isNewGame && ngc->nPlayersShown > 1)?
        TRI_ENAB_ENABLED : TRI_ENAB_HIDDEN;

    if ( newEnable != ngc->juggleEnabled ) {
        (*ngc->enableAttrProc)( ngc->closure, NG_ATTR_CANJUGGLE, newEnable );
        ngc->juggleEnabled = newEnable;
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    newEnable = (ngc->role == SERVER_STANDALONE)?
        TRI_ENAB_HIDDEN : TRI_ENAB_ENABLED;

    if ( newEnable != ngc->settingsEnabled ) {
        ngc->settingsEnabled = newEnable;
        (*ngc->enableAttrProc)( ngc->closure, NG_ATTR_CANCONFIG, newEnable );
    }
#endif
} /* considerEnable */

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

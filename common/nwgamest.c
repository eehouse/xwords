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


#ifdef CPLUS
extern "C" {
#endif

struct NewGameCtx {
    NewGameEnableColProc enableColProc;
    NewGameEnableAttrProc enableAttrProc;
    NewGameSetColProc setColProc;
    NewGameGetColProc getColProc;
    NewGameSetAttrProc setAttrProc;
    void* closure;

    /* Palm needs to store cleartext passwords separately in order to
       store '***' in the visible field */
    XP_Bool enabled[NG_NUM_COLS][MAX_NUM_PLAYERS];
    XP_U16 nPlayers;
#ifndef XWFEATURE_STANDALONE_ONLY
    Connectedness role;
#endif
    XP_Bool isNewGame;

    MPSLOT
};

static void enableOne( NewGameCtx* ngc, XP_U16 player, NewGameColumn col,
                       XP_Bool enable, XP_Bool force );
static void adjustAllRows( NewGameCtx* ngc, XP_Bool force );
static void adjustOneRow( NewGameCtx* ngc, XP_U16 player, XP_Bool force );

NewGameCtx*
gamedlg_make( MPFORMAL XP_Bool isNewGame, 
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
/*     result->getAttrProc = getAttrProc; */
    result->closure = closure;
    result->isNewGame = isNewGame;
    MPASSIGN(result->mpool, mpool);

    return result;
} /* gamedlg_make */

void
gamedlg_destroy( NewGameCtx* ngc )
{
#ifdef PLATFORM_PALM
    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        XP_UCHAR* passwd = ngc->passwds[i];
        if ( !!passwd ) {
            XP_FREE( ngc->mpool, passwd );
        }
    }
#endif
} /* gamedlg_destroy */

void
gamedlg_load( NewGameCtx* ngc, const CurGameInfo* gi )
{
    void* closure = ngc->closure;
    NGValue cValue;
    NGValue aValue;
    XP_U16 i;

    ngc->nPlayers = gi->nPlayers;
    aValue.ng_u16 = ngc->nPlayers;
    (*ngc->setAttrProc)( closure, NG_ATTR_NPLAYERS, aValue );
    (*ngc->enableAttrProc)( closure, NG_ATTR_NPLAYERS, ngc->isNewGame );

    ngc->role = gi->serverRole;
    aValue.ng_role = ngc->role;
    (*ngc->setAttrProc)( closure, NG_ATTR_ROLE, aValue );
    (*ngc->enableAttrProc)( closure, NG_ATTR_ROLE, ngc->isNewGame );

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {

#ifndef XWFEATURE_STANDALONE_ONLY        
        cValue.ng_bool = gi->players[i].isLocal;
        (*ngc->setColProc)(closure, i, NG_COL_LOCAL, cValue );
#endif

        cValue.ng_cp = gi->players[i].name;
        (*ngc->setColProc)(closure, i, NG_COL_NAME, cValue );

        cValue.ng_cp = gi->players[i].password;
        (*ngc->setColProc)(closure, i, NG_COL_PASSWD, cValue );

        cValue.ng_bool = gi->players[i].isRobot;
        (*ngc->setColProc)(closure, i, NG_COL_ROBOT, cValue );
    }
    
    adjustAllRows( ngc, XP_TRUE );
} /* gamedlg_load */

void
gamedlg_store( NewGameCtx* ngc, CurGameInfo* gi )
{
    NGValue aValue;
    XP_U16 nPlayers, i;
    void* closure = ngc->closure;

    gi->nPlayers = nPlayers = ngc->nPlayers;
    gi->serverRole = ngc->role;

    for ( i = 0; i < nPlayers; ++i ) {
        NGValue cValue;

#ifndef XWFEATURE_STANDALONE_ONLY
        (*ngc->getColProc)( closure, i, NG_COL_LOCAL, &cValue );
        gi->players[i].isLocal = cValue.ng_bool;
#endif

        (*ngc->getColProc)( closure, i, NG_COL_NAME, &cValue );
        replaceStringIfDifferent( MPPARM(ngc->mpool) &gi->players[i].name,
                                  cValue.ng_cp );
        XP_LOGF( "copied %s", gi->players[i].name );

        (*ngc->getColProc)( closure, i, NG_COL_PASSWD, &cValue );
        replaceStringIfDifferent( MPPARM(ngc->mpool) &gi->players[i].password,
                                  cValue.ng_cp );

        (*ngc->getColProc)( closure, i, NG_COL_ROBOT, &cValue );
        gi->players[i].isRobot = cValue.ng_bool;
    }
} /* gamedlg_store */

void
gamedlg_colChanged( NewGameCtx* ngc, XP_U16 player, NewGameColumn col, 
                    NGValue value )
{
    XP_ASSERT( player < ngc->nPlayers );
    adjustOneRow( ngc, player, XP_FALSE );
}

void
gamedlg_attrChanged( NewGameCtx* ngc, NewGameAttr attr, NGValue value )
{
    XP_LOGF( "%s(%d)", __FUNCTION__ );
    if ( attr == NG_ATTR_NPLAYERS ) {
        ngc->nPlayers = value.ng_u16;
    } else if ( NG_ATTR_ROLE == attr ) { 
        ngc->role = value.ng_role;
    } else {
        XP_ASSERT( 0 );
    }
    adjustAllRows( ngc, XP_FALSE );
}

static void
enableOne( NewGameCtx* ngc, XP_U16 player, NewGameColumn col, 
           XP_Bool enable, XP_Bool force )
{
    XP_Bool* esp = &ngc->enabled[col][player];
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
    XP_Bool enable[NG_NUM_COLS];
    NewGameColumn col;
    XP_MEMSET( enable, 0, sizeof(enable) );
    XP_Bool isLocal = XP_TRUE;
    NGValue value;

    /* If there aren't this many players, all are disabled */
    if ( player >= ngc->nPlayers ) {
        /* do nothing: all are false */

    } else {

#ifndef XWFEATURE_STANDALONE_ONLY
        /* If standalone or client, local is disabled */
        if ( ngc->role == SERVER_ISSERVER ) {
            enable[NG_COL_LOCAL] = XP_TRUE;
            (*ngc->getColProc)( ngc->closure, player, NG_COL_LOCAL, &value );
            isLocal = value.ng_bool;
        }
#endif

        /* If local is enabled and not set, all else is disabled */
        if ( isLocal ) {
            enable[NG_COL_NAME] = XP_TRUE;
            enable[NG_COL_ROBOT] = XP_TRUE;

            (*ngc->getColProc)( ngc->closure, player, NG_COL_ROBOT,
                                &value );
            if ( !value.ng_bool ) {
                enable[NG_COL_PASSWD] = XP_TRUE;
            }
        }
    } 

    for ( col = 0; col < NG_NUM_COLS; ++col ) {
        enableOne( ngc, player, col, enable[col], force );
    }
} /* adjustOneRow */

#ifdef CPLUS
}
#endif

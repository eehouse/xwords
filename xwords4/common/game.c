/* 
 * Copyright 2001-2011 by Eric House (xwords@eehouse.org).  All rights
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

#include "game.h"
#include "dictnry.h"
#include "strutils.h"
#include "nli.h"
#include "dbgutil.h"
#include "stats.h"
#include "device.h"
#include "gamemgr.h"
#include "gamerefp.h"
#include "utilsp.h"

#ifdef CPLUS
extern "C" {
#endif

void
gi_copy( CurGameInfo* destGI, const CurGameInfo* srcGI )
{
    *destGI = *srcGI;
} /* gi_copy */

static XP_Bool
strEq( const XP_UCHAR* str1, const XP_UCHAR* str2 )
{
    if ( NULL == str1 ) {
        str1 = "";
    }
    if ( NULL == str2 ) {
        str2 = "";
    }
    return 0 == XP_STRCMP(str1, str2);
}

XP_Bool
gi_equal( const CurGameInfo* gi1, const CurGameInfo* gi2 )
{
    XP_Bool equal = XP_FALSE;
    int ii;
    for ( ii = 0; ; ++ii ) {
        switch ( ii ) {
        case 0:
            equal = gi1->gameID == gi2->gameID;
            break;
        case 1:
            equal = gi1->gameSeconds == gi2->gameSeconds;
            break;
        case 2:
            equal = gi1->nPlayers == gi2->nPlayers;
            break;
        case 3:
            equal = gi1->boardSize == gi2->boardSize;
            break;
        case 4:
            equal = gi1->traySize == gi2->traySize;
            break;
        case 5:
            equal = gi1->bingoMin == gi2->bingoMin;
            break;
        case 6:
            equal = gi1->forceChannel == gi2->forceChannel;
            break;
        case 7:
            equal = gi1->deviceRole == gi2->deviceRole;
            break;
        case 8:
            equal = gi1->hintsNotAllowed == gi2->hintsNotAllowed;
            break;
        case 9:
            equal = gi1->timerEnabled == gi2->timerEnabled;
            break;
        case 10:
            equal = gi1->allowPickTiles == gi2->allowPickTiles;
            break;
        case 11:
            equal = gi1->allowHintRect == gi2->allowHintRect;
            break;
        case 12:
            equal = gi1->inDuplicateMode == gi2->inDuplicateMode;
            break;
        case 13:
            equal = gi1->phoniesAction == gi2->phoniesAction;
            break;
        case 14:
            equal = gi1->confirmBTConnect == gi2->confirmBTConnect;
            break;
        case 15:
            equal = strEq( gi1->dictName, gi2->dictName );
            break;
        case 16:
            equal = strEq( gi1->isoCodeStr, gi2->isoCodeStr );
            break;
        case 17:
            equal = gi1->tradeSub7 == gi2->tradeSub7;
            break;
        case 18:
            for ( int jj = 0; equal && jj < gi1->nPlayers; ++jj ) {
                const LocalPlayer* lp1 = &gi1->players[jj];
                const LocalPlayer* lp2 = &gi2->players[jj];
                equal = strEq( lp1->name, lp2->name )
                    && strEq( lp1->password, lp2->password )
                    && strEq( lp1->dictName, lp2->dictName )
                    && lp1->isLocal == lp2->isLocal
                    && lp1->robotIQ == lp2->robotIQ
                    ;
            }
            break;
        case 19:
            equal = gi1->created == gi2->created;
            break;
        case 20:
            equal = gi1->conTypes == gi2->conTypes;
            break;
        case 21:
            equal = strEq( gi1->gameName, gi2->gameName );
            break;
        case 22:
            equal = gi1->fromRematch == gi2->fromRematch;
            break;
        default:
            goto done;
            break;
        }
        if ( !equal ) {
            break;
        }
    }
 done:
    if ( !equal ) {
        XP_LOGFF( "exited when ii = %d", ii );
        LOG_GI( gi1, "gi1" );
        LOG_GI( gi2, "gi2" );
    }

    return equal;
}

void
gi_setNPlayers( XW_DUtilCtxt* dutil, XWEnv xwe, CurGameInfo* gi,
                XP_U16 nTotal, XP_U16 nHere )
{
    LOG_GI( gi, "before" );
    XP_ASSERT( nTotal <= MAX_NUM_PLAYERS );
    XP_ASSERT( nHere < nTotal );

    gi->nPlayers = nTotal;

    XP_U16 curLocal = 0;
    for ( XP_U16 ii = 0; ii < nTotal; ++ii ) {
        if ( gi->players[ii].isLocal ) {
            ++curLocal;
        }
    }

    if ( nHere != curLocal ) {
        /* This will happen when a device has more than one player. Not sure I
           handle that correctly, but don't assert for now. */
        XP_LOGFF( "nHere: %d; curLocal: %d; a problem?", nHere, curLocal );
        for ( XP_U16 ii = 0; ii < nTotal; ++ii ) {
            if ( !gi->players[ii].isLocal ) {
                gi->players[ii].isLocal = XP_TRUE;
                XP_LOGFF( "making player #%d local when wasn't before", ii );
                ++curLocal;
                XP_ASSERT( curLocal <= nHere );
                if ( curLocal == nHere ) {
                    break;
                }
            }
        }
    }

    for ( XP_U16 ii = 0; ii < nTotal; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        if ( !lp->name[0] ) {
            XP_U16 len = VSIZE(lp->name);
            dutil_getUsername( dutil, xwe, ii, LP_IS_LOCAL(lp),
                               LP_IS_ROBOT(lp), lp->name, &len );
        }
    }

    LOG_GI( gi, "after" );
}

XP_U16
gi_countLocalPlayers( const CurGameInfo* gi, XP_Bool humanOnly )
{
    XP_U16 count = 0;
    XP_U16 nPlayers = gi->nPlayers;
    const LocalPlayer* lp = gi->players;
    while ( nPlayers-- ) {
        if ( lp->isLocal ) {
            if ( humanOnly && LP_IS_ROBOT(lp) ) {
                // skip
            } else {
                ++count;
            }
        }
        ++lp;
    }
    return count;
} /* gi_countLocalPlayers */

/* Given a hint (likely the selected player), return that player if it's valid
   and local. Otherwise the first local player. */
XP_U16
gi_getLocalPlayer( const CurGameInfo* gi, XP_S16 fromHint )
{
    if ( fromHint < 0 || gi->nPlayers <= fromHint ) {
        fromHint = 0;
    }
    if ( ! gi->players[fromHint].isLocal ) {
        for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
            if ( gi->players[ii].isLocal ) {
                fromHint = ii;
                break;
            }
        }
    }
    return fromHint;
}

void
gi_readFromStream( XWStreamCtxt* stream, CurGameInfo* gi )
{
#ifdef DEBUG
    XP_Bool success =
#endif
        gi_gotFromStream( stream, gi );
    XP_ASSERT( success );
}

XP_Bool
gi_gotFromStream( XWStreamCtxt* stream, CurGameInfo* gip )
{
    CurGameInfo gi = {};
    XP_Bool success = XP_FALSE;
    XP_U16 strVersion = strm_getVersion( stream );
    XP_U16 nColsNBits;
    XP_ASSERT( 0 < strVersion );
#ifdef STREAM_VERS_BIGBOARD
    nColsNBits = STREAM_VERS_BIGBOARD > strVersion ? NUMCOLS_NBITS_4
        : NUMCOLS_NBITS_5;
#else
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    if ( STREAM_VERS_BIGGERGI <= strVersion ) {
        if ( !gotStringFromStreamHere( stream, gi.gameName,
                                       VSIZE(gi.gameName) ) ) {
            GOTO_FAIL();
        }
    }

    if ( !gotStringFromStreamHere( stream, gi.dictName,
                                   VSIZE(gi.dictName) ) ) {
        GOTO_FAIL();
    }

    XP_U32 tmp32;

    if ( !strm_gotBits( stream, NPLAYERS_NBITS, &tmp32 ) ) GOTO_FAIL();
    gi.nPlayers = tmp32;
    if ( !strm_gotBits( stream, nColsNBits, &tmp32 ) ) GOTO_FAIL();
    gi.boardSize = tmp32;
    if ( STREAM_VERS_NINETILES <= strVersion ) {
        if ( !strm_gotBits( stream, NTILES_NBITS_9, &tmp32 ) ) GOTO_FAIL();
        gi.traySize = tmp32;
        if ( !strm_gotBits( stream, NTILES_NBITS_9, &tmp32 ) ) GOTO_FAIL();
        gi.bingoMin = tmp32;
    } else {
        gi.traySize = gi.bingoMin = 7;
    }
    if ( !strm_gotBits( stream, 2, &tmp32 ) ) GOTO_FAIL();
    gi.deviceRole = (DeviceRole)tmp32;
    /* XP_LOGF( "%s: read deviceRole of %d", __func__, gi.deviceRole ); */

    if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
    gi.hintsNotAllowed = tmp32;
    if ( strVersion < STREAM_VERS_ROBOTIQ ) {
        if ( !strm_gotBits( stream, 2, &tmp32 ) ) GOTO_FAIL();
    }
    if ( !strm_gotBits( stream, 2, &tmp32 ) ) GOTO_FAIL();
    gi.phoniesAction = (XWPhoniesChoice)tmp32;
    if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
    gi.timerEnabled = tmp32;

    gi.inDuplicateMode = XP_FALSE;
    if ( strVersion >= STREAM_VERS_DUPLICATE ) {
        if ( ! strm_gotBits( stream, 1, &tmp32 ) ) {
            GOTO_FAIL();
        }
        gi.inDuplicateMode = (XP_Bool)tmp32;
    }

    if ( strVersion >= STREAM_VERS_SUBSEVEN ) {
        if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
        gi.tradeSub7 = tmp32;
    } else {
        gi.tradeSub7 =  XP_FALSE;
    }
    if ( strVersion >= STREAM_VERS_41B4 ) {
        if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
        gi.allowPickTiles = tmp32;
        if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
        gi.allowHintRect =tmp32;
    } else {
        gi.allowPickTiles = XP_FALSE;
        gi.allowHintRect = XP_FALSE;
    }

    if ( STREAM_VERS_BIGGERGI <= strVersion ) {
        if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
        gi.fromRematch = tmp32;
    }

    if ( strVersion >= STREAM_VERS_BLUETOOTH ) {
        if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
        gi.confirmBTConnect = tmp32;
    } else {
        gi.confirmBTConnect = XP_TRUE; /* safe given all the 650s out there. */
    }

    if ( STREAM_VERS_MULTIADDR <= strVersion ) {
        if ( !strm_gotBits( stream, 2, &tmp32 ) ) GOTO_FAIL();
        gi.forceChannel = tmp32;
    }

    if ( strVersion < STREAM_VERS_BLUETOOTH2 ) {
        XP_U16 tmp16;
        if ( !strm_gotU16( stream, &tmp16 ) ) GOTO_FAIL();
        gi.gameID = tmp16;
    } else {
        if ( !strm_gotU32( stream, &gi.gameID ) ) GOTO_FAIL();
    }

    if ( STREAM_VERS_BIGGERGI <= strVersion
         && gi.deviceRole != ROLE_STANDALONE ) {
        if ( !strm_gotU16( stream, &gi.conTypes ) ) GOTO_FAIL();
    }

    if ( STREAM_VERS_GI_ISO <= strVersion ) {
        if ( !gotStringFromStreamHere( stream, gi.isoCodeStr,
                                       VSIZE(gi.isoCodeStr) ) ) {
            GOTO_FAIL();
        }
    } else if ( STREAM_VERS_DICTLANG <= strVersion ) {
        XP_LangCode dictLang;
        if ( !strm_gotU8( stream, &dictLang ) ) GOTO_FAIL();
        const XP_UCHAR* isoCode = lcToLocale( dictLang );
        XP_ASSERT( !!isoCode );
        XP_STRNCPY( gi.isoCodeStr, isoCode, VSIZE(gi.isoCodeStr) );
        // XP_LOGFF( "upgrading; faked isoCode: %s", gi.isoCodeStr );
    }

    if ( gi.timerEnabled || strVersion >= STREAM_VERS_GAMESECONDS ) {
        if ( !strm_gotU16( stream, &gi.gameSeconds ) ) GOTO_FAIL();
    }

    if ( STREAM_VERS_BIGGI <= strVersion ) {
        if ( !strm_gotU32( stream, &gi.created ) ) GOTO_FAIL();
        XP_ASSERT( gi.created );
    } else {
        XP_ASSERT( 0 == gi.created );
    }

    for ( int ii = 0; ii < gi.nPlayers; ++ii ) {
        LocalPlayer* pl = &gi.players[ii];
        if ( !gotStringFromStreamHere( stream, pl->name, VSIZE(pl->name) ) ) {
            GOTO_FAIL();
        }

        if ( !gotStringFromStreamHere( stream, pl->password, VSIZE(pl->password) ) ) {
            GOTO_FAIL();
        }

        if ( strVersion >= STREAM_VERS_PLAYERDICTS ) {
            if ( !gotStringFromStreamHere( stream, pl->dictName, VSIZE(pl->dictName) ) ) {
                GOTO_FAIL();
            }
        }

        if ( STREAM_VERS_BIGGERGI > strVersion ) {
            XP_U16 ignore;
            if ( !strm_gotU16( stream, &ignore ) ) { /* consume it */
                GOTO_FAIL();
            }
        }
        if ( strVersion < STREAM_VERS_ROBOTIQ ) {
            if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
            pl->robotIQ = tmp32;
        } else {
            if ( !strm_gotU8( stream, &pl->robotIQ ) ) GOTO_FAIL();
        }
        if ( !strm_gotBits( stream, 1, &tmp32 ) ) GOTO_FAIL();
        pl->isLocal = tmp32;
    }

    XP_ASSERT( !!gi.isoCodeStr[0] );
    success = XP_TRUE;
    *gip = gi;
 fail:
    return success;
} /* gi_gotFromStream */

/* PENDING Get rid of this */
CurGameInfo
gi_readFromStream2( XWStreamCtxt* stream )
{
    CurGameInfo gi;
    gi_readFromStream( stream, &gi );
    return gi;
}

void
gi_writeToStream( XWStreamCtxt* stream, const CurGameInfo* gi )
{
    XP_U16 nColsNBits;
#ifdef STREAM_VERS_BIGBOARD
    XP_U16 strVersion = strm_getVersion( stream );
    /* XP_LOGF( "%s: strVersion = 0x%x", __func__, strVersion ); */
    XP_ASSERT( STREAM_SAVE_PREVWORDS <= strVersion );
    nColsNBits = STREAM_VERS_BIGBOARD > strVersion ? NUMCOLS_NBITS_4
        : NUMCOLS_NBITS_5;
#else
    nColsNBits = NUMCOLS_NBITS_4;
#endif

    if ( STREAM_VERS_BIGGERGI <= strVersion ) {
        stringToStream( stream, gi->gameName );
    }

    stringToStream( stream, gi->dictName );

    strm_putBits( stream, NPLAYERS_NBITS, gi->nPlayers );
    strm_putBits( stream, nColsNBits, gi->boardSize );

    if ( STREAM_VERS_NINETILES <= strVersion ) {
        XP_ASSERT( 0 < gi->traySize );
        strm_putBits( stream, NTILES_NBITS_9, gi->traySize );
        strm_putBits( stream, NTILES_NBITS_9, gi->bingoMin );
    } else {
        XP_LOGFF( "strVersion: %d so not writing traySize", strVersion );
    }
    strm_putBits( stream, 2, gi->deviceRole );
    strm_putBits( stream, 1, gi->hintsNotAllowed );
    strm_putBits( stream, 2, gi->phoniesAction );
    strm_putBits( stream, 1, gi->timerEnabled );
    strm_putBits( stream, 1, gi->inDuplicateMode );
    if ( strVersion >= STREAM_VERS_SUBSEVEN ) {
        strm_putBits( stream, 1, gi->tradeSub7 );
    }
    strm_putBits( stream, 1, gi->allowPickTiles );
    strm_putBits( stream, 1, gi->allowHintRect );
    strm_putBits( stream, 1, gi->confirmBTConnect );
    strm_putBits( stream, 2, gi->forceChannel );

    if ( STREAM_VERS_BIGGERGI <= strVersion ) {
        strm_putBits( stream, 1, gi->fromRematch );
    }

    // XP_LOGFF( "wrote forceChannel: %d for gid %X", gi->forceChannel, gi->gameID );

    if ( 0 ) {
#ifdef STREAM_VERS_BIGBOARD
    } else if ( STREAM_VERS_BIGBOARD <= strVersion ) {
        strm_putU32( stream, gi->gameID );
#endif
    } else {
        strm_putU16( stream, gi->gameID );
    }

    if ( STREAM_VERS_BIGGERGI <= strVersion
         && gi->deviceRole != ROLE_STANDALONE ) {
        strm_putU16( stream, gi->conTypes );
    }

    XP_ASSERT( !!gi->isoCodeStr[0] );
    if ( STREAM_VERS_GI_ISO <= strVersion ) {
        stringToStream( stream, gi->isoCodeStr );
    } else {
        XP_LangCode code;
        if ( haveLocaleToLc( gi->isoCodeStr, &code ) ) {
            strm_putU8( stream, code );
        } else {
            XP_ASSERT( 0 );
        }
    }
    strm_putU16( stream, gi->gameSeconds );

    if ( STREAM_VERS_BIGGI <= strVersion ) {
        XP_ASSERT( gi->created );
        strm_putU32( stream, gi->created );
    }

    int ii;
    const LocalPlayer* pl;
    for ( pl = gi->players, ii = 0; ii < gi->nPlayers; ++pl, ++ii ) {
        stringToStream( stream, pl->name );
        stringToStream( stream, pl->password );
        stringToStream( stream, pl->dictName );
        if ( STREAM_VERS_BIGGERGI > strVersion ) {
            strm_putU16( stream, 0 );
        }
        strm_putU8( stream, pl->robotIQ );
        strm_putBits( stream, 1, pl->isLocal );
    }
} /* gi_writeToStream */

XP_Bool
player_hasPasswd( const LocalPlayer* player )
{
    return !!player->password[0];
} /* player_hasPasswd */

XP_Bool
player_passwordMatches( const LocalPlayer* player, const XP_UCHAR* buf )
{
    XP_ASSERT( player->isLocal );

    return 0 == XP_STRCMP( player->password, (XP_UCHAR*)buf );
} /* player_passwordMatches */

GameRef
gi_formatGR( const CurGameInfo* gi )
{
    XP_ASSERT( !!gi );
    GameRef gr = formatGR( gi->gameID, gi->deviceRole );
    return gr;
}

XP_Bool
gi_isValid(const CurGameInfo* gi)
{
    XP_Bool result =
        !!gi
        && 0 < gi->nPlayers && gi->nPlayers <= MAX_NUM_PLAYERS
        && gi->created
        && gi->gameID
        && (ROLE_STANDALONE == gi->deviceRole || 0 != gi->conTypes)
        ;
    return result;
}

void
gi_augmentGI( CurGameInfo* newGI, const CurGameInfo* curGI,
              XP_U8 strVersion ) /* the version that produced newGI */
{
    XP_LOGFF( "look here (streamVersion=0x%x", strVersion );
    /* LOG_GI( newGI, "new" ); */
    /* LOG_GI( curGI, "cur" ); */
    XP_ASSERT( newGI->nPlayers == curGI->nPlayers );
    if ( strVersion < STREAM_VERS_BIGGERGI ) {
        XP_ASSERT( !newGI->conTypes );
        newGI->conTypes = curGI->conTypes;
        XP_ASSERT( !newGI->created );
        newGI->created = curGI->created;
    }
}

#ifdef DEBUG
void
gi_logGI( const CurGameInfo* gi, XP_UCHAR* buf, XP_U16 bufLen,
            const char* func, int line )
{
    int offset = 0;
    offset += XP_SNPRINTF( &buf[offset], bufLen - offset,
                           "from %s() line %d; ", func, line );
    if ( !!gi ) {
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset,
                               "name: %s, ", gi->gameName );
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset,
                               "gameID: %X; created: %d, ", gi->gameID, gi->created );
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset,
                               "role: %d, ", gi->deviceRole );
        XP_UCHAR tmp[128];
        logTypeSet( gi->conTypes, tmp, VSIZE(tmp) );
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset,
                               "types: [%s], ", tmp );

        offset += XP_SNPRINTF( &buf[offset], bufLen - offset,
                               "nPlayers: %d [", gi->nPlayers );
        for ( XP_U16 ii = 0; ii < gi->nPlayers; ++ii ) {
            const LocalPlayer* lp = &gi->players[ii];
            offset += XP_SNPRINTF( &buf[offset], bufLen - offset,
                                   "{plr[%d]: local: %s; robotIQ: %d; name: %s; dict: %s; pwd: %s}, ", ii,
                                   boolToStr(lp->isLocal), lp->robotIQ, lp->name, lp->dictName, lp->password );
        }
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset, "],  forceChannel: %d", gi->forceChannel );
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset, "  dict: %s", gi->dictName );
        offset += XP_SNPRINTF( &buf[offset], bufLen - offset, "  iso: %s", gi->isoCodeStr );
        // offset += XP_SNPRINTF( &buf[offset], bufLen - offset, "  tradSub7: %s", boolToStr(gi->tradeSub7) );
    }
}
#endif

#ifdef CPLUS
}
#endif

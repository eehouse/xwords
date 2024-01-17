/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2001 - 2019 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef NATIVE_NLI

#include "nli.h"
#include "comms.h"
#include "strutils.h"
#include "dbgutil.h"

#define NLI_VERSION_LC 1          /* adds inDuplicateMode */
#define NLI_VERSION_ISO 2          /* replaces _lang with isoCode -- for later */

void
nli_init( NetLaunchInfo* nli, const CurGameInfo* gi, const CommsAddrRec* addr,
          XP_U16 nPlayersH, XP_U16 forceChannel )
{
    XP_MEMSET( nli, 0, sizeof(*nli) );
    nli->gameID = gi->gameID;
    XP_STRCAT( nli->dict, gi->dictName );
    XP_STRCAT( nli->isoCodeStr, gi->isoCodeStr );
    nli->nPlayersT = gi->nPlayers;
    nli->nPlayersH = nPlayersH;
    nli->forceChannel = forceChannel;
    nli->inDuplicateMode = gi->inDuplicateMode;

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        types_addType( &nli->_conTypes, typ );
        switch ( typ ) {
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            XP_STRCAT( nli->room, addr->u.ip_relay.invite );
            break;
#endif
        case COMMS_CONN_SMS:
            XP_STRCAT( nli->phone, addr->u.sms.phone );
            if ( 1 != addr->u.sms.port ) {
                /* PENDING I've seen an assert about this fire, but no time
                   to investigate now */
                XP_LOGFF( "unexpected port value: %d", addr->u.sms.port );
            }
            // nli->port = addr->u.sms.port; <-- I wish
            break;
        case COMMS_CONN_MQTT:
            nli_setMQTTDevID( nli, &addr->u.mqtt.devID );
            break;
        case COMMS_CONN_BT:
            XP_STRCAT( nli->btAddress, addr->u.bt.btAddr.chars );
            XP_STRCAT( nli->btName, addr->u.bt.hostName );
            break;
        case COMMS_CONN_NFC:
            break;
        default:
            XP_ASSERT(0);
            break;
        }
    }
}

void
nliToGI( const NetLaunchInfo* nli, XWEnv xwe, XW_UtilCtxt* util, CurGameInfo* gi )
{
    gi_setNPlayers( gi, xwe, util, nli->nPlayersT, nli->nPlayersH );
    gi->gameID = nli->gameID;

    XP_U16 nLocals = 0;
    XP_Bool remotesAreRobots = nli->remotesAreRobots;
    XW_DUtilCtxt* duc = util_getDevUtilCtxt( util, xwe );
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        if ( lp->isLocal ) {
            if ( nli->remotesAreRobots ) {
                lp->robotIQ = 1;
            }
            XP_UCHAR buf[64];
            XP_U16 len = VSIZE(buf);
            dutil_getUsername( duc, xwe, nLocals++, XP_TRUE, remotesAreRobots,
                               buf, &len );
            replaceStringIfDifferent( util->mpool, &lp->name, buf );
        }
    }

    /* These defaults can be overwritten when host starts game after all
       register */
    gi->boardSize = 15;
    gi->traySize = gi->bingoMin = 7;

    XP_STRNCPY( gi->isoCodeStr, nli->isoCodeStr, VSIZE(gi->isoCodeStr) );
    gi->forceChannel = nli->forceChannel;
    gi->inDuplicateMode = nli->inDuplicateMode;
    gi->serverRole = SERVER_ISCLIENT; /* recipient of invitation is client */
    replaceStringIfDifferent( util->mpool, &gi->dictName, nli->dict );
}

static XP_U32 
gameID( const NetLaunchInfo* nli )
{
    XP_U32 gameID = nli->gameID;
    if ( 0 == gameID ) {
        sscanf( nli->inviteID, "%X", &gameID );
    }
    return gameID;
}

void
nli_setDevID( NetLaunchInfo* nli, XP_U32 devID )
{
    nli->devID = devID;
    types_addType( &nli->_conTypes, COMMS_CONN_RELAY );
}

void
nli_setInviteID( NetLaunchInfo* nli, const XP_UCHAR* inviteID )
{
    nli->inviteID[0] = '\0';
    XP_STRCAT( nli->inviteID, inviteID );
}

void
nli_setGameName( NetLaunchInfo* nli, const XP_UCHAR* gameName )
{
    XP_SNPRINTF( nli->gameName, sizeof(nli->gameName), "%s", gameName );
    nli->gameName[sizeof(nli->gameName)-1] = '\0';
}

void
nli_setMQTTDevID( NetLaunchInfo* nli, const MQTTDevID* mqttDevID )
{
    types_addType( &nli->_conTypes, COMMS_CONN_MQTT );
    formatMQTTDevID( mqttDevID, nli->mqttDevID, VSIZE(nli->mqttDevID) );
}

void 
nli_saveToStream( const NetLaunchInfo* nli, XWStreamCtxt* stream )
{
    LOGNLI( nli );

    /* We'll use version 1 unless the ISOCODE has no XP_LangCode equivalent,
       meaning the wordlist is too new and requires NLI_VERSION_2 */
    XP_LangCode code;
    XP_U8 version = haveLocaleToLc( nli->isoCodeStr, &code )
        ? NLI_VERSION_LC : NLI_VERSION_ISO;
    stream_putU8( stream, version );

    stream_putU16( stream, nli->_conTypes );
    switch ( version ) {
    case NLI_VERSION_LC:
        stream_putU16( stream, code );
        break;
    case NLI_VERSION_ISO:
        stringToStream( stream, nli->isoCodeStr );
        break;
    default:
        XP_ASSERT(0);
        break;
    }
    stringToStream( stream, nli->dict );
    stringToStream( stream, nli->gameName );
    stream_putU8( stream, nli->nPlayersT );
    stream_putU8( stream, nli->nPlayersH );
    stream_putU32( stream, gameID( nli ) );
    stream_putU8( stream, nli->forceChannel );

    if ( types_hasType( nli->_conTypes, COMMS_CONN_RELAY ) ) {
        stringToStream( stream, nli->room );
        stringToStream( stream, nli->inviteID );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_BT ) ) {
        stringToStream( stream, nli->btName );
        stringToStream( stream, nli->btAddress );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_SMS ) ) {
        stringToStream( stream, nli->phone );
        stream_putU8( stream, nli->isGSM );
        stream_putU8( stream, nli->osType );
        stream_putU32( stream, nli->osVers );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_MQTT ) ) {
        stringToStream( stream, nli->mqttDevID );
    }

    stream_putBits( stream, 1, nli->remotesAreRobots ? 1 : 0 );
    stream_putBits( stream, 1, nli->inDuplicateMode ? 1 : 0 );
}

XP_Bool 
nli_makeFromStream( NetLaunchInfo* nli, XWStreamCtxt* stream )
{
    XP_Bool success = XP_TRUE;
    LOG_FUNC();
    XP_MEMSET( nli, 0, sizeof(*nli) );
    XP_U16 version = stream_getU8( stream );
    XP_LOGF( "%s(): read version: %d", __func__, version );

    nli->_conTypes = stream_getU16( stream );
    if ( version == NLI_VERSION_LC ) {
        XP_LangCode lang = stream_getU16( stream );
        const XP_UCHAR* isoCode = lcToLocale( lang );
        XP_ASSERT( !!isoCode );
        XP_STRNCPY( nli->isoCodeStr, isoCode, VSIZE(nli->isoCodeStr) );
    } else if ( version == NLI_VERSION_ISO ) {
        stringFromStreamHere( stream, nli->isoCodeStr, sizeof(nli->isoCodeStr) );
    } else {
        success = XP_FALSE;
    }

    if ( success ) {
        stringFromStreamHere( stream, nli->dict, sizeof(nli->dict) );
        stringFromStreamHere( stream, nli->gameName, sizeof(nli->gameName) );
        nli->nPlayersT = stream_getU8( stream );
        nli->nPlayersH = stream_getU8( stream );
        nli->gameID = stream_getU32( stream );
        nli->forceChannel = stream_getU8( stream );

        if ( types_hasType( nli->_conTypes, COMMS_CONN_RELAY ) ) {
            stringFromStreamHere( stream, nli->room, sizeof(nli->room) );
            stringFromStreamHere( stream, nli->inviteID, sizeof(nli->inviteID) );
            if ( version == 0 ) {
                nli->devID = stream_getU32( stream );
            }
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_BT ) ) {
            stringFromStreamHere( stream, nli->btName, sizeof(nli->btName) );
            stringFromStreamHere( stream, nli->btAddress, sizeof(nli->btAddress) );
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_SMS ) ) {
            stringFromStreamHere( stream, nli->phone, sizeof(nli->phone) );
            nli->isGSM = stream_getU8( stream );
            nli->osType= stream_getU8( stream );
            nli->osVers = stream_getU32( stream );
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_MQTT ) ) {
            stringFromStreamHere( stream, nli->mqttDevID, sizeof(nli->mqttDevID) );
        }

        if ( version > 0 && 0 < stream_getSize( stream ) ) {
            nli->remotesAreRobots = 0 != stream_getBits( stream, 1 );
            nli->inDuplicateMode = stream_getBits( stream, 1 );
            XP_LOGF( "%s(): remotesAreRobots: %d; inDuplicateMode: %d", __func__,
                     nli->remotesAreRobots, nli->inDuplicateMode );
        } else {
            nli->inDuplicateMode = XP_FALSE;
        }

        XP_ASSERT( 0 == stream_getSize( stream ) );
        LOGNLI( nli );
    }

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

void
nli_makeAddrRec( const NetLaunchInfo* nli, CommsAddrRec* addr )
{
    XP_MEMSET( addr, 0, sizeof(*addr) );
    CommsConnType type;
    for ( XP_U32 state = 0; types_iter( nli->_conTypes, &type, &state ); ) {
        addr_addType( addr, type );
        switch( type ) {
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            XP_STRCAT( addr->u.ip_relay.invite, nli->room );
            /* String relayName = XWPrefs.getDefaultRelayHost( context ); */
            /* int relayPort = XWPrefs.getDefaultRelayPort( context ); */
            /* result.setRelayParams( relayName, relayPort, room ); */
            break;
#endif
        case COMMS_CONN_BT:
            XP_STRCAT( addr->u.bt.btAddr.chars, nli->btAddress );
            XP_STRCAT( addr->u.bt.hostName, nli->btName );
            break;
        case COMMS_CONN_SMS:
            XP_STRCAT( addr->u.sms.phone, nli->phone );
            addr->u.sms.port = 1; /* BAD, but 0 is worse */
            break;
        case COMMS_CONN_MQTT: {
#ifdef DEBUG
            XP_Bool success =
#endif
                strToMQTTCDevID( nli->mqttDevID, &addr->u.mqtt.devID );
            XP_ASSERT( success );
        }
            break;
        case COMMS_CONN_NFC:
            break;
        default:
            XP_ASSERT(0);
            break;
        }
    }
}

#ifdef XWFEATURE_NLI_FROM_ARGV
XP_Bool
nli_fromArgv( MPFORMAL NetLaunchInfo* nlip, int argc, const char** argv )
{
    XP_LOGFF( "(argc=%d)", argc );
    CurGameInfo gi = {0};
    CommsAddrRec addr = {0};
    MQTTDevID mqttDevID = 0;
    XP_U16 nPlayersH = 0;
    XP_U16 forceChannel = 0;
    const XP_UCHAR* gameName = NULL;
    const XP_UCHAR* inviteID = NULL;

    XP_LOGFF("foo");
    for ( int ii = 0; ii < argc; ++ii ) {
        const char* argp = argv[ii];
        char* param = strchr(argp, '=');
        if ( !param ) {         /* no '='? */
            XP_LOGFF( "arg bad?: %s", argp );
            continue;
        }
        char arg[8];
        int argLen = param - argp;
        XP_MEMCPY( arg, argp, argLen );
        arg[argLen] = '\0';
        ++param;                /* skip the '=' */

        if ( 0 == strcmp( "iso", arg ) ) {
            XP_STRNCPY( gi.isoCodeStr, param, VSIZE(gi.isoCodeStr) );
        } else if ( 0 == strcmp( "np", arg ) ) {
            gi.nPlayers = atoi(param);
        } else if ( 0 == strcmp( "nh", arg ) ) {
            nPlayersH = atoi(param);
        } else if ( 0 == strcmp( "gid", arg ) ) {
            gi.gameID = atoi(param);
        } else if ( 0 == strcmp( "fc", arg ) ) {
            gi.forceChannel = atoi(param);
        } else if ( 0 == strcmp( "nm", arg ) ) {
            gameName = param;
        } else if ( 0 == strcmp( "id", arg ) ) {
            inviteID = param;
        } else if ( 0 == strcmp( "wl", arg ) ) {
            replaceStringIfDifferent( mpool, &gi.dictName, param );
        } else if ( 0 == strcmp( "r2id", arg ) ) {
            if ( strToMQTTCDevID( param, &addr.u.mqtt.devID ) ) {
                addr_addType( &addr, COMMS_CONN_MQTT );
            } else {
                XP_LOGFF( "bad devid %s", param );
            }
        } else {
            XP_LOGFF( "dropping arg %s, param %s", arg, param );
        }
    }

    bool success = 0 < nPlayersH &&
        addr_hasType( &addr, COMMS_CONN_MQTT );

    if ( success ) {
        nli_init( nlip, &gi, &addr, nPlayersH, forceChannel );
        if ( !!gameName ) {
            nli_setGameName( nlip, gameName );
        }
        if ( !!inviteID ) {
            nli_setInviteID( nlip, inviteID );
        }
        LOGNLI( nlip );
    }
    gi_disposePlayerInfo( MPPARM(mpool) &gi );
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}
#endif

# ifdef DEBUG
void
logNLI( const NetLaunchInfo* nli, const char* callerFunc, const int callerLine )
{
    XP_LOGFF( "called by %s(), line %d", callerFunc, callerLine );

    XP_UCHAR conTypes[128] = {0};
    int offset = 0;
    CommsConnType typ;
    for ( XP_U32 state = 0; types_iter( nli->_conTypes, &typ, &state ); ) {
        const char* asstr = ConnType2Str( typ );
        offset += XP_SNPRINTF( &conTypes[offset], sizeof(conTypes)-offset, "%s,", asstr );
    }

    XP_UCHAR buf[1024];
    XP_SNPRINTF( buf, VSIZE(buf), "{ctyps: [%s], nPlayersT: %d, nPlayersH: %d, "
                 "channel: %d, isoCode: '%s', gameID: %X, gameName: %s",
                 conTypes, nli->nPlayersT, nli->nPlayersH, nli->forceChannel,
                 nli->isoCodeStr, nli->gameID, nli->gameName );
    if ( types_hasType( nli->_conTypes, COMMS_CONN_MQTT ) ) {
        XP_UCHAR smallBuf[128];
        XP_SNPRINTF( smallBuf, VSIZE(smallBuf), ", mqttid: %s", nli->mqttDevID );
        XP_STRCAT( buf, smallBuf );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_SMS ) ) {
        XP_UCHAR smallBuf[128];
        XP_SNPRINTF( smallBuf, VSIZE(smallBuf), ", phone: %s",
                     nli->phone );
        XP_STRCAT( buf, smallBuf );
    }
    XP_STRCAT( buf, "}" );
    XP_LOGF( "%s", buf );
}
# endif
#endif

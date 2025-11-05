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
nli_init( NetLaunchInfo* nlip, const CurGameInfo* gi, const CommsAddrRec* addr,
          XP_U16 nPlayersH, XP_U16 forceChannel )
{
    XP_ASSERT( gi_isValid(gi) );
    NetLaunchInfo nli = {};
    nli.gameID = gi->gameID;
    XP_STRCAT( nli.dict, gi->dictName );
    XP_STRCAT( nli.isoCodeStr, gi->isoCodeStr );
    nli.nPlayersT = gi->nPlayers;
    nli.nPlayersH = nPlayersH;
    nli.forceChannel = forceChannel;
    nli.inDuplicateMode = gi->inDuplicateMode;

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        if ( types_hasType( gi->conTypes, typ ) ) {
            types_addType( &nli._conTypes, typ );
            switch ( typ ) {
#ifdef XWFEATURE_RELAY
            case COMMS_CONN_RELAY:
                XP_STRCAT( nli.room, addr->u.ip_relay.invite );
                break;
#endif
            case COMMS_CONN_SMS:
                XP_STRCAT( nli.phone, addr->u.sms.phone );
                if ( 1 != addr->u.sms.port ) {
                    /* PENDING I've seen an assert about this fire, but no time
                       to investigate now */
                    XP_LOGFF( "unexpected port value: %d", addr->u.sms.port );
                }
                // nli.port = addr->u.sms.port; <-- I wish
                break;
            case COMMS_CONN_MQTT:
                nli_setMQTTDevID( &nli, &addr->u.mqtt.devID );
                break;
            case COMMS_CONN_BT:
                XP_STRCAT( nli.btAddress, addr->u.bt.btAddr.chars );
                XP_STRCAT( nli.btName, addr->u.bt.hostName );
                break;
            case COMMS_CONN_NFC:
                break;
            default:
                XP_ASSERT(0);
                break;
            }
        } else {
            XP_LOGFF( "dropping type %s because not in gi", ConnType2Str(typ) );
        }
    }
    XP_ASSERT( 0 != nli._conTypes );
    *nlip = nli;
}

void
nliToGI( XW_DUtilCtxt* dutil, XWEnv xwe, const NetLaunchInfo* nli,
         CurGameInfo* gi )
{
    gi_setNPlayers( dutil, xwe, gi, nli->nPlayersT, nli->nPlayersH );
    gi->gameID = nli->gameID;
    XP_STRNCPY( &gi->isoCodeStr[0], nli->isoCodeStr, VSIZE(gi->isoCodeStr)-1 );

    XP_U16 nLocals = 0;
    XP_Bool remotesAreRobots = nli->remotesAreRobots;
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        if ( lp->isLocal ) {
            if ( nli->remotesAreRobots ) {
                lp->robotIQ = 1;
            }
            XP_U16 len = VSIZE(lp->name);
            dutil_getUsername( dutil, xwe, nLocals++, XP_TRUE,
                               remotesAreRobots, lp->name, &len );
        }
    }

    /* These defaults can be overwritten when host starts game after all
       register */
    gi->boardSize = 15;
    gi->traySize = gi->bingoMin = 7;

    XP_STRNCPY( gi->isoCodeStr, nli->isoCodeStr, VSIZE(gi->isoCodeStr) );
    gi->forceChannel = nli->forceChannel;
    gi->inDuplicateMode = nli->inDuplicateMode;
    gi->deviceRole = ROLE_ISGUEST; /* recipient of invitation is client */
    str2ChrArray( gi->dictName, nli->dict );
    str2ChrArray( gi->gameName, nli->gameName );

    gi->conTypes = nli->_conTypes;
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
nli_setPhone( NetLaunchInfo* nli, const XP_UCHAR* phone )
{
    types_addType( &nli->_conTypes, COMMS_CONN_SMS );
    XP_STRNCPY( nli->phone, phone, VSIZE(nli->phone) );
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
    strm_putU8( stream, version );

    strm_putU16( stream, nli->_conTypes );
    switch ( version ) {
    case NLI_VERSION_LC:
        strm_putU16( stream, code );
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
    strm_putU8( stream, nli->nPlayersT );
    strm_putU8( stream, nli->nPlayersH );
    strm_putU32( stream, gameID( nli ) );
    strm_putU8( stream, nli->forceChannel );

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
        strm_putU8( stream, nli->isGSM );
        strm_putU8( stream, nli->osType );
        strm_putU32( stream, nli->osVers );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_MQTT ) ) {
        stringToStream( stream, nli->mqttDevID );
    }

    strm_putBits( stream, 1, nli->remotesAreRobots ? 1 : 0 );
    strm_putBits( stream, 1, nli->inDuplicateMode ? 1 : 0 );
}

XP_Bool 
nli_makeFromStream( NetLaunchInfo* nli, XWStreamCtxt* stream )
{
    XP_Bool success = XP_TRUE;
    LOG_FUNC();
    XP_MEMSET( nli, 0, sizeof(*nli) );
    XP_U16 version = strm_getU8( stream );
    XP_LOGFF( "read version: %d", version );

    nli->_conTypes = strm_getU16( stream );
    if ( version == NLI_VERSION_LC ) {
        XP_LangCode lang = strm_getU16( stream );
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
        nli->nPlayersT = strm_getU8( stream );
        nli->nPlayersH = strm_getU8( stream );
        nli->gameID = strm_getU32( stream );
        nli->forceChannel = strm_getU8( stream );
        XP_LOGFF( "read forceChannel: %X", nli->forceChannel );

        if ( types_hasType( nli->_conTypes, COMMS_CONN_RELAY ) ) {
            stringFromStreamHere( stream, nli->room, sizeof(nli->room) );
            stringFromStreamHere( stream, nli->inviteID, sizeof(nli->inviteID) );
            if ( version == 0 ) {
                nli->devID = strm_getU32( stream );
            }
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_BT ) ) {
            stringFromStreamHere( stream, nli->btName, sizeof(nli->btName) );
            stringFromStreamHere( stream, nli->btAddress, sizeof(nli->btAddress) );
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_SMS ) ) {
            stringFromStreamHere( stream, nli->phone, sizeof(nli->phone) );
            nli->isGSM = strm_getU8( stream );
            nli->osType= strm_getU8( stream );
            nli->osVers = strm_getU32( stream );
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_MQTT ) ) {
            stringFromStreamHere( stream, nli->mqttDevID, sizeof(nli->mqttDevID) );
        }

        if ( version > 0 && 0 < strm_getSize( stream ) ) {
            nli->remotesAreRobots = 0 != strm_getBits( stream, 1 );
            nli->inDuplicateMode = strm_getBits( stream, 1 );
            XP_LOGF( "%s(): remotesAreRobots: %d; inDuplicateMode: %d", __func__,
                     nli->remotesAreRobots, nli->inDuplicateMode );
        } else {
            nli->inDuplicateMode = XP_FALSE;
        }

        XP_ASSERT( 0 == strm_getSize( stream ) );
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
    CurGameInfo gi = {};
    CommsAddrRec addr = {};
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

void
nli_makeInviteData( const NetLaunchInfo* nli, XWStreamCtxt* stream )
{
    /* Generate URL compatible with Android NetLaunchInfo.makeLaunchUri() */

    UrlParamState state = {};
    urlParamToStream( stream, &state, "iso", UPT_STRING, nli->isoCodeStr );

    XP_LangCode lc;
    if ( haveLocaleToLc(nli->isoCodeStr, &lc) ) {
        urlParamToStream( stream, &state, "lang", UPT_U8, lc );
    }
    
    /* Total players */
    urlParamToStream( stream, &state, "np", UPT_U8, nli->nPlayersT );

    /* Here players */
    urlParamToStream( stream, &state, "nh", UPT_U8, nli->nPlayersH );

    /* Game ID */
    urlParamToStream( stream, &state, "gid", UPT_U32, nli->gameID );

    /* Force channel */
    urlParamToStream( stream, &state, "fc", UPT_U8, nli->forceChannel );

    /* Connection types */
    urlParamToStream( stream, &state, "ad", UPT_U32, nli->_conTypes );

    /* Game name (URL encoded for special characters) */
    urlParamToStream( stream, &state, "nm", UPT_STRING, nli->gameName );

    /* Dictionary name (URL encoded for special characters) */
    urlParamToStream( stream, &state, "wl", UPT_STRING, nli->dict );

    /* Duplicate mode */
    if ( nli->inDuplicateMode ) {
        urlParamToStream( stream, &state, "du", UPT_U8, 1 );
    }

    /* Connection-specific parameters */
    if ( types_hasType( nli->_conTypes, COMMS_CONN_BT ) ) {
        urlParamToStream( stream, &state, "btn", UPT_STRING, nli->btName );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_SMS ) ) {
        urlParamToStream( stream, &state, "phone", UPT_STRING, nli->phone );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_MQTT ) ) {
        urlParamToStream( stream, &state, "r2id", UPT_STRING, nli->mqttDevID );
    }
}

XP_Bool
nli_fromInviteData( XW_DUtilCtxt* dutil, XWStreamCtxt* stream, NetLaunchInfo* nlip )
{
    NetLaunchInfo nli = {};
    XWStreamPos startPos = strm_getPos( stream, POS_READ );

    XP_Bool success = 
        urlParamFromStream( dutil, stream, "ad", UPT_U32, &nli._conTypes )
        && urlParamFromStream( dutil, stream, "np", UPT_U8, &nli.nPlayersT )
        && urlParamFromStream( dutil, stream, "nh", UPT_U8, &nli.nPlayersH )
        && urlParamFromStream( dutil, stream, "gid", UPT_U32, &nli.gameID )
        && urlParamFromStream( dutil, stream, "fc", UPT_U8, &nli.forceChannel )
        && urlParamFromStream( dutil, stream, "wl", UPT_STRING, &nli.dict, VSIZE(nli.dict) )
        ;

    if ( success ) {
        /* optional params */
        urlParamFromStream( dutil, stream, "nm", UPT_STRING, &nli.gameName, VSIZE(nli.gameName) );
        urlParamFromStream( dutil, stream, "du", UPT_U8, &nli.inDuplicateMode );

        /* Connection-specific parameters */
        if ( types_hasType( nli._conTypes, COMMS_CONN_BT ) ) { 
            success = urlParamFromStream( dutil, stream, "btn", UPT_STRING,
                                          &nli.btName, VSIZE(nli.btName) );
        }
        if ( success && types_hasType( nli._conTypes, COMMS_CONN_SMS ) ) {
            success = urlParamFromStream( dutil, stream, "phone", UPT_STRING,
                                          &nli.phone, VSIZE(nli.phone) );
        }
        if ( success && types_hasType( nli._conTypes, COMMS_CONN_MQTT ) ) {
            success = urlParamFromStream( dutil, stream, "r2id", UPT_STRING,
                                          &nli.mqttDevID, VSIZE(nli.mqttDevID) );
        }
    }

    if ( success ) {
        *nlip = nli;
    } else {
        strm_setPos( stream, POS_READ, startPos );
    }
    return success;
}

# ifdef DEBUG
void
logNLI( const NetLaunchInfo* nli, const char* callerFunc, const int callerLine )
{
    XP_LOGFF( "called by %s(), line %d", callerFunc, callerLine );

    XP_UCHAR conTypes[128] = {};
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

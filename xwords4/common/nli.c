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

/* Don't check in other than 0 for a few releases!!! */
#ifndef NLI_VERSION
// # define NLI_VERSION 0
# define NLI_VERSION 1          /* adds inDuplicateMode */
#endif

void
nli_init( NetLaunchInfo* nli, const CurGameInfo* gi, const CommsAddrRec* addr,
          XP_U16 nPlayersH, XP_U16 forceChannel )
{
    XP_MEMSET( nli, 0, sizeof(*nli) );
    nli->gameID = gi->gameID;
    XP_STRCAT( nli->dict, gi->dictName );
    nli->lang = gi->dictLang;
    nli->nPlayersT = gi->nPlayers;
    nli->nPlayersH = nPlayersH;
    nli->forceChannel = forceChannel;
    nli->inDuplicateMode = gi->inDuplicateMode;

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( addr, &typ, &st ); ) {
        types_addType( &nli->_conTypes, typ );
        switch ( typ ) {
        case COMMS_CONN_RELAY:
            XP_STRCAT( nli->room, addr->u.ip_relay.invite );
            break;
        case COMMS_CONN_SMS:
            XP_STRCAT( nli->phone, addr->u.sms.phone );
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
    stream_putU8( stream, NLI_VERSION );

    stream_putU16( stream, nli->_conTypes );
    stream_putU16( stream, nli->lang );
    stringToStream( stream, nli->dict );
    stringToStream( stream, nli->gameName );
    stream_putU8( stream, nli->nPlayersT );
    stream_putU8( stream, nli->nPlayersH );
    stream_putU32( stream, gameID( nli ) );
    stream_putU8( stream, nli->forceChannel );

    if ( types_hasType( nli->_conTypes, COMMS_CONN_RELAY ) ) {
        stringToStream( stream, nli->room );
        stringToStream( stream, nli->inviteID );
        if ( 0 == NLI_VERSION ) {
            stream_putU32( stream, nli->devID );
        }
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

    if ( NLI_VERSION > 0 ) {
        stream_putBits( stream, 1, nli->remotesAreRobots ? 1 : 0 );
        stream_putBits( stream, 1, nli->inDuplicateMode ? 1 : 0 );
    }
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
    nli->lang = stream_getU16( stream );
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

    LOG_RETURNF( "%s", boolToStr(success) );
    LOGNLI( nli );
    return success;
}

void
nli_makeAddrRec( const NetLaunchInfo* nli, CommsAddrRec* addr )
{
    CommsConnType type;
    for ( XP_U32 state = 0; types_iter( nli->_conTypes, &type, &state ); ) {
        addr_addType( addr, type );
        switch( type ) {
        case COMMS_CONN_RELAY:
            XP_STRCAT( addr->u.ip_relay.invite, nli->room );
            /* String relayName = XWPrefs.getDefaultRelayHost( context ); */
            /* int relayPort = XWPrefs.getDefaultRelayPort( context ); */
            /* result.setRelayParams( relayName, relayPort, room ); */
            break;
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

# ifdef DEBUG
void
logNLI( const NetLaunchInfo* nli, const char* callerFunc, const int callerLine )
{
    XP_LOGFF( "called by %s(), line %d", callerFunc, callerLine );

    XP_UCHAR buf[256];
    XP_SNPRINTF( buf, VSIZE(buf), "{ctyps: %x, nPlayersT: %d; nPlayersH: %d; "
                 "gameID: %d; inviteID: %s, mqttid: %s}", nli->_conTypes,
                 nli->nPlayersT, nli->nPlayersH, nli->gameID, nli->inviteID,
                 nli->mqttDevID );
    XP_LOGF( "%s", buf );
}
# endif
#endif

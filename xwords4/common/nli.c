/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2001 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

void
nli_init( NetLaunchInfo* nli, const CurGameInfo* gi, const CommsAddrRec* addr,
          XP_U16 nPlayers, XP_U16 forceChannel )
{
    XP_MEMSET( nli, 0, sizeof(*nli) );
    nli->gameID = gi->gameID;
    XP_STRCAT( nli->dict, gi->dictName );
    nli->lang = gi->dictLang;
    nli->nPlayersT = gi->nPlayers;
    nli->nPlayersH = nPlayers;
    nli->forceChannel = forceChannel;

    if ( addr_hasType( addr, COMMS_CONN_RELAY ) ) {
        types_addType( &nli->_conTypes, COMMS_CONN_RELAY );
        XP_STRCAT( nli->room, addr->u.ip_relay.invite );
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
nli_saveToStream( const NetLaunchInfo* nli, XWStreamCtxt* stream )
{
    LOG_FUNC();
    stream_putU8( stream, nli->version );
    stream_putU16( stream, nli->_conTypes );
    XP_LOGF( "%s: wrote _conTypes: %x", __func__, nli->_conTypes );
    stream_putU16( stream, nli->lang );
    stringToStream( stream, nli->dict );
    stringToStream( stream, nli->gameName );
    stream_putU8( stream, nli->nPlayersT );
    stream_putU8( stream, nli->nPlayersH );
    stream_putU32( stream, gameID( nli ) );
    stream_putU8( stream, nli->forceChannel );

    if ( types_hasType( nli->_conTypes, COMMS_CONN_RELAY ) ) {
        XP_LOGF( "%s: writing relay stuff", __func__ );
        stringToStream( stream, nli->room );
        XP_LOGF( "%s: writing room: %s", __func__, nli->room );
        stringToStream( stream, nli->inviteID );
        stream_putU32( stream, nli->devID );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_BT ) ) {
        XP_LOGF( "%s: writing bt stuff", __func__ );
        stringToStream( stream, nli->btName );
        stringToStream( stream, nli->btAddress );
    }
    if ( types_hasType( nli->_conTypes, COMMS_CONN_SMS ) ) {
        XP_LOGF( "%s: writing sms stuff", __func__ );
        stringToStream( stream, nli->phone );
        stream_putU8( stream, nli->isGSM );
        stream_putU8( stream, nli->osType );
        stream_putU32( stream, nli->osVers );
    }
    LOG_RETURN_VOID();
}

XP_Bool 
nli_makeFromStream( NetLaunchInfo* nli, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_MEMSET( nli, 0, sizeof(*nli) );
    nli->version = stream_getU8( stream );
    XP_Bool success = 0 == nli->version;
    if ( success ) {
        nli->_conTypes = stream_getU16( stream );
        XP_LOGF( "%s: read _conTypes: %x", __func__, nli->_conTypes );
        nli->lang = stream_getU16( stream );
        stringFromStreamHere( stream, nli->dict, sizeof(nli->dict) );
        stringFromStreamHere( stream, nli->gameName, sizeof(nli->gameName) );
        nli->nPlayersT = stream_getU8( stream );
        nli->nPlayersH = stream_getU8( stream );
        nli->gameID = stream_getU32( stream );
        nli->forceChannel = stream_getU8( stream );

        if ( types_hasType( nli->_conTypes, COMMS_CONN_RELAY ) ) {
            XP_LOGF( "%s: reading relay stuff", __func__ );
            stringFromStreamHere( stream, nli->room, sizeof(nli->room) );
            XP_LOGF( "%s: read room: %s", __func__, nli->room );
            stringFromStreamHere( stream, nli->inviteID, sizeof(nli->inviteID) );
            nli->devID = stream_getU32( stream );
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_BT ) ) {
            XP_LOGF( "%s: reading bt stuff", __func__ );
            stringFromStreamHere( stream, nli->btName, sizeof(nli->btName) );
            stringFromStreamHere( stream, nli->btAddress, sizeof(nli->btAddress) );
        }
        if ( types_hasType( nli->_conTypes, COMMS_CONN_SMS ) ) {
            XP_LOGF( "%s: reading sms stuff", __func__ );
            stringFromStreamHere( stream, nli->phone, sizeof(nli->phone) );
            nli->isGSM = stream_getU8( stream );
            nli->osType= stream_getU8( stream );
            nli->osVers = stream_getU32( stream );
        }
    }
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

void
nli_makeAddrRec( const NetLaunchInfo* nli, CommsAddrRec* addr )
{
    XP_U32 state = 0;
    CommsConnType type;
    while ( types_iter( nli->_conTypes, &type, &state ) ) {
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
            break;
        default:
            XP_ASSERT(0);
            break;
        }
    }
}
#endif

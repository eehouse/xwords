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

#include "invit.h"
#include "comms.h"
#include "strutils.h"

void
invit_init( InviteInfo* invit, const CurGameInfo* gi, const CommsAddrRec* addr,
          XP_U16 nPlayers, XP_U16 forceChannel )
{
    XP_MEMSET( invit, 0, sizeof(*invit) );
    invit->gameID = gi->gameID;
    XP_STRCAT( invit->dict, gi->dictName );
    invit->lang = gi->dictLang;
    invit->nPlayersT = gi->nPlayers;
    invit->nPlayersH = nPlayers;
    invit->forceChannel = forceChannel;

    if ( addr_hasType( addr, COMMS_CONN_RELAY ) ) {
        types_addType( &invit->_conTypes, COMMS_CONN_RELAY );
        XP_STRCAT( invit->room, addr->u.ip_relay.invite );
    }
    
}

static XP_U32 
gameID( const InviteInfo* invit )
{
    XP_U32 gameID = invit->gameID;
    if ( 0 == gameID ) {
        sscanf( invit->inviteID, "%X", &gameID );
    }
    return gameID;
}

void
invit_setDevID( InviteInfo* invit, XP_U32 devID )
{
    invit->devID = devID;
}

void 
invit_saveToStream( const InviteInfo* invit, XWStreamCtxt* stream )
{
    LOG_FUNC();
    stream_putU8( stream, invit->version );
    stream_putU16( stream, invit->_conTypes );
    stream_putU16( stream, invit->lang );
    stringToStream( stream, invit->dict );
    stringToStream( stream, invit->gameName );
    stream_putU8( stream, invit->nPlayersT );
    stream_putU8( stream, invit->nPlayersH );
    stream_putU32( stream, gameID( invit ) );
    stream_putU8( stream, invit->forceChannel );

    if ( types_hasType( invit->_conTypes, COMMS_CONN_RELAY ) ) {
        stringToStream( stream, invit->room );
        XP_LOGF( "%s: wrote room %s", __func__, invit->room );
        stringToStream( stream, invit->inviteID );
        stream_putU32( stream, invit->devID );
        XP_LOGF( "%s: wrote devID %d", __func__, invit->devID );
    }
    if ( types_hasType( invit->_conTypes, COMMS_CONN_BT ) ) {
        stringToStream( stream, invit->btName );
        stringToStream( stream, invit->btAddress );
    }
    if ( types_hasType( invit->_conTypes, COMMS_CONN_SMS ) ) {
        stringToStream( stream, invit->phone );
        stream_putU8( stream, invit->isGSM );
        stream_putU8( stream, invit->osType );
        stream_putU32( stream, invit->osVers );
    }
}

XP_Bool 
invit_makeFromStream( InviteInfo* invit, XWStreamCtxt* stream )
{
    LOG_FUNC();
    XP_MEMSET( invit, 0, sizeof(*invit) );
    invit->version = stream_getU8( stream );
    XP_Bool success = 0 == invit->version;
    if ( success ) {
        invit->_conTypes  = stream_getU16( stream );
        invit->lang = stream_getU16( stream );
        stringFromStreamHere( stream, invit->dict, sizeof(invit->dict) );
        stringFromStreamHere( stream, invit->gameName, sizeof(invit->gameName) );
        invit->nPlayersT = stream_getU8( stream );
        invit->nPlayersH = stream_getU8( stream );
        invit->gameID = stream_getU32( stream );
        invit->forceChannel = stream_getU8( stream );

        if ( types_hasType( invit->_conTypes, COMMS_CONN_RELAY ) ) {
            stringFromStreamHere( stream, invit->room, sizeof(invit->room) );
            XP_LOGF( "%s: read room %s", __func__, invit->room );
            stringFromStreamHere( stream, invit->inviteID, sizeof(invit->inviteID) );
            invit->devID = stream_getU32( stream );
            XP_LOGF( "%s: read devID %d", __func__, invit->devID );
        }
        if ( types_hasType( invit->_conTypes, COMMS_CONN_BT ) ) {
            stringFromStreamHere( stream, invit->btName, sizeof(invit->btName) );
            stringFromStreamHere( stream, invit->btAddress, sizeof(invit->btAddress) );
        }
        if ( types_hasType( invit->_conTypes, COMMS_CONN_SMS ) ) {
            stringFromStreamHere( stream, invit->phone, sizeof(invit->phone) );
            invit->isGSM = stream_getU8( stream );
            invit->osType= stream_getU8( stream );
            invit->osVers = stream_getU32( stream );
        }
    }
    return success;
}

void
invit_makeAddrRec( const InviteInfo* invit, CommsAddrRec* addr )
{
    XP_U32 state = 0;
    CommsConnType type;
    while ( types_iter( invit->_conTypes, &type, &state ) ) {
        addr_addType( addr, type );
        switch( type ) {
        case COMMS_CONN_RELAY:
            XP_STRCAT( addr->u.ip_relay.invite, invit->room );
            /* String relayName = XWPrefs.getDefaultRelayHost( context ); */
            /* int relayPort = XWPrefs.getDefaultRelayPort( context ); */
            /* result.setRelayParams( relayName, relayPort, room ); */
            break;
        case COMMS_CONN_BT:
            XP_STRCAT( addr->u.bt.btAddr.chars, invit->btAddress );
            XP_STRCAT( addr->u.bt.hostName, invit->btName );
            break;
        case COMMS_CONN_SMS:
            XP_STRCAT( addr->u.sms.phone, invit->phone );
            break;
        default:
            XP_ASSERT(0);
            break;
        }
    }
}

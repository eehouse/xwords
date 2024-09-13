/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2023 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

#include <sys/socket.h>
#include <sys/un.h>

#include "extcmds.h"
#include "device.h" 
#include "strutils.h"
#include "linuxmain.h"
#include "gamesdb.h"
#include "dbgutil.h"
#include "stats.h"
#include "knownplyr.h"

static XP_U32
castGid( cJSON* obj )
{
    XP_U32 gameID;
    sscanf( obj->valuestring, "%X", &gameID );
    return gameID;
}

static void
makeObjIfNot( cJSON** objp )
{
    if ( NULL == *objp ) {
        *objp = cJSON_CreateObject();
    }
}

static void
addStringToObject( cJSON** objp, const char* key, const char* value )
{
    makeObjIfNot( objp );
    cJSON_AddStringToObject( *objp, key, value );
}

static void
addGIDToObject( cJSON** objp, XP_U32 gid, const char* key )
{
    char buf[16];
    sprintf( buf, "%08X", gid );
    addStringToObject( objp, key, buf );
}

static void
addObjectToObject( cJSON** objp, const char* key, cJSON* value )
{
    makeObjIfNot( objp );
    cJSON_AddItemToObject( *objp,  key, value );
}

static void
addSuccessToObject( cJSON** objp, XP_Bool success )
{
    makeObjIfNot( objp );
    cJSON_AddBoolToObject( *objp, "success", success );
}

static XP_U32
gidFromObject( const cJSON* obj )
{
    cJSON* tmp = cJSON_GetObjectItem( obj, "gid" );
    XP_ASSERT( !!tmp );
    return castGid( tmp );
}


/* Invite can be via a known player or via */
static XP_Bool
inviteFromArgs( CmdWrapper* wr, cJSON* args )
{
    XW_DUtilCtxt* dutil = wr->params->dutil;
    XP_U32 gameID = gidFromObject( args );

    XP_Bool viaKnowns = XP_FALSE;
    cJSON* remotes = cJSON_GetObjectItem( args, "remotes" );
    if ( !remotes ) {
        remotes = cJSON_GetObjectItem( args, "kps" );
        viaKnowns = !!remotes;
    }

    int nRemotes = cJSON_GetArraySize(remotes);
    CommsAddrRec destAddrs[nRemotes];
    XP_MEMSET( destAddrs, 0, sizeof(destAddrs) );

    XP_Bool success = XP_TRUE;
    for ( int ii = 0; success && ii < nRemotes; ++ii ) {
        cJSON* item = cJSON_GetArrayItem( remotes, ii );

        if ( viaKnowns ) {
            /* item is just a name */
            XP_LOGFF( "found kplyr name: %s", item->valuestring );
            success = kplr_getAddr( dutil, NULL_XWE, item->valuestring,
                                    &destAddrs[ii], NULL );
        } else {
            cJSON* addr = cJSON_GetObjectItem( item, "addr" );
            XP_ASSERT( !!addr );
            cJSON* tmp = cJSON_GetObjectItem( addr, "mqtt" );
            if ( !!tmp ) {
                XP_LOGFF( "parsing mqtt: %s", tmp->valuestring );
                addr_addType( &destAddrs[ii], COMMS_CONN_MQTT );
                success =
                    strToMQTTCDevID( tmp->valuestring, &destAddrs[ii].u.mqtt.devID );
                XP_ASSERT( success );
            }
            tmp = cJSON_GetObjectItem( addr, "sms" );
            if ( !!tmp ) {
                XP_LOGFF( "parsing sms: %s", tmp->valuestring );
                addr_addType( &destAddrs[ii], COMMS_CONN_SMS );
                XP_STRCAT( destAddrs[ii].u.sms.phone, tmp->valuestring );
                destAddrs[ii].u.sms.port = 1;
            }
        }
    }

    if ( success ) {
        (*wr->procs.addInvites)( wr->closure, gameID, nRemotes, destAddrs );
    }

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

static XP_Bool
newGuestFromArgs( CmdWrapper* wr, cJSON* args )
{
    XP_U32 gameID = gidFromObject( args );
    cJSON* tmp = cJSON_GetObjectItem( args, "nPlayersT" );
    XP_U16 nPlayersT = tmp->valueint;
    NetLaunchInfo nli = {.gameID = gameID,
        .nPlayersT = nPlayersT,
        .nPlayersH = 1,
    };

    cJSON* addr = cJSON_GetObjectItem( args, "addr" );
    tmp = cJSON_GetObjectItem( addr, "mqtt" );
    if ( !!tmp ) {
        XP_STRCAT( nli.mqttDevID, tmp->valuestring );
        types_addType( &nli._conTypes, COMMS_CONN_MQTT );
    }
    tmp = cJSON_GetObjectItem( addr, "sms" );
    if ( !!tmp ) {
        XP_STRCAT( nli.phone, tmp->valuestring );
        types_addType( &nli._conTypes, COMMS_CONN_SMS );
    }

    tmp = cJSON_GetObjectItem( args, "dict" );
    if ( !!tmp ) {
        XP_STRCAT( nli.dict, tmp->valuestring );
    }

    (*wr->procs.newGuest)( wr->closure, &nli );
    return XP_TRUE;
}

static XP_Bool
moveifFromArgs( CmdWrapper* wr, cJSON* args )
{
    XP_U32 gameID = gidFromObject( args );
    cJSON* tmp = cJSON_GetObjectItem( args, "tryTrade" );
    XP_Bool tryTrade = !!tmp && cJSON_IsTrue( tmp );
    return (*wr->procs.makeMoveIf)( wr->closure, gameID, tryTrade );
}

static cJSON*
getStats( CmdWrapper* wr )
{
    XW_DUtilCtxt* dutil = wr->params->dutil;
    cJSON* json = sts_export( dutil, NULL_XWE );
    return json;
}

static XP_Bool
chatFromArgs( CmdWrapper* wr, cJSON* args )
{
    XP_U32 gameID = gidFromObject( args );
    cJSON* tmp = cJSON_GetObjectItem( args, "msg" );
    const char* msg = tmp->valuestring;
    return (*wr->procs.sendChat)( wr->closure, gameID, msg );
}

static XP_Bool
undoFromArgs( CmdWrapper* wr, cJSON* args )
{
    XP_U32 gameID = gidFromObject( args );
    return (*wr->procs.undoMove)( wr->closure, gameID );
}

static XP_Bool
resignFromArgs( CmdWrapper* wr, cJSON* args )
{
    XP_U32 gameID = gidFromObject( args );
    return (*wr->procs.resign)( wr->closure, gameID );
}
static cJSON*
knwnPlyrs( CmdWrapper* wr )
{
    return (*wr->procs.getKPs)( wr->closure );
}

/* Return 'gid' of new game */
static XP_U32
rematchFromArgs( CmdWrapper* wr, cJSON* args )
{
    XP_U32 result = 0;

    XP_U32 gameID = gidFromObject( args );

    cJSON* tmp = cJSON_GetObjectItem( args, "rematchOrder" );
    RematchOrder ro = roFromStr( tmp->valuestring );

    XP_U32 newGameID = 0;
    if ( (*wr->procs.makeRematch)( wr->closure, gameID, ro, &newGameID ) ) {
        result = newGameID;
    }
    return result;
}

static XP_Bool
getGamesStateForArgs( CmdWrapper* wr, cJSON* args, cJSON** states, cJSON** orders )
{
    LOG_FUNC();
    LaunchParams* params = wr->params;

    *states = cJSON_CreateArray();

    cJSON* gids = cJSON_GetObjectItem( args, "gids" );
    XP_Bool success = !!gids;
    for ( int ii = 0 ; success && ii < cJSON_GetArraySize(gids) ; ++ii ) {
        XP_U32 gameID = castGid( cJSON_GetArrayItem( gids, ii ) );

        GameInfo gib = {};
        if ( gdb_getGameInfoForGID( params->pDb, gameID, &gib ) ) {
            cJSON* item = NULL;
            addGIDToObject( &item, gameID, "gid" );
            cJSON_AddBoolToObject( item, "gameOver", gib.gameOver );
            cJSON_AddNumberToObject( item, "nPending", gib.nPending );
            cJSON_AddNumberToObject( item, "nMoves", gib.nMoves );
            cJSON_AddNumberToObject( item, "nTiles", gib.nTiles );

            cJSON_AddItemToArray( *states, item );
        }        
    }

    XP_LOGFF( "done with states" ); /* got here */

    if ( success && !!orders ) {
        cJSON* gids = cJSON_GetObjectItem( args, "orders" );
        if ( !gids ) {
            *orders = NULL;
        } else {
            *orders = cJSON_CreateArray();
            for ( int ii = 0 ; ii < cJSON_GetArraySize(gids) ; ++ii ) {
                XP_U32 gameID = castGid( cJSON_GetArrayItem( gids, ii ) );

                const CommonGlobals* cg =
                    (*wr->procs.getForGameID)( wr->closure, gameID );
                if ( !cg ) {
                    continue;
                }
                const XWGame* game = &cg->game;
                if ( server_getGameIsConnected( game->server ) ) {
                    const CurGameInfo* gi = cg->gi;
                    LOGGI( gi, __func__ );
                    cJSON* order = NULL;
                    addGIDToObject( &order, gameID, "gid" );
                    cJSON* players = cJSON_CreateArray();
                    for ( int jj = 0; jj < gi->nPlayers; ++jj ) {
                        XP_LOGFF( "looking at player %d", jj );
                        const LocalPlayer* lp = &gi->players[jj];
                        XP_LOGFF( "adding player %d: %s", jj, lp->name );
                        cJSON* cName = cJSON_CreateString( lp->name );
                        cJSON_AddItemToArray( players, cName);
                    }
                    cJSON_AddItemToObject( order, "players", players );
                    cJSON_AddItemToArray( *orders, order );
                }
            }
        }
    }

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

/* Return for each gid and array of player names, in play order, and including
   for each whether it's the host and if a robot. For now let's try by opening
   each game (yeah! yuck!) to read the info directly. Later add to a the db
   accessed by gamesdb.c
*/
static cJSON*
getPlayersForArgs( CmdWrapper* wr, cJSON* args )
{
    cJSON* result = cJSON_CreateArray();
    cJSON* gids = cJSON_GetObjectItem( args, "gids" );
    for ( int ii = 0 ; ii < cJSON_GetArraySize(gids) ; ++ii ) {
        XP_U32 gameID = castGid( cJSON_GetArrayItem( gids, ii ) );

        const CommonGlobals* cg = (*wr->procs.getForGameID)( wr->closure, gameID );
        const CurGameInfo* gi = cg->gi;
        LOGGI( gi, __func__ );
        const XWGame* game = &cg->game;

        cJSON* players = cJSON_CreateArray();
        for ( int jj = 0; jj < gi->nPlayers; ++jj ) {
            cJSON* playerObj = NULL;
            const LocalPlayer* lp = &gi->players[jj];
            XP_LOGFF( "adding player %d: %s", jj, lp->name );
            addStringToObject( &playerObj, "name", lp->name );
            XP_Bool isLocal = lp->isLocal;
            cJSON_AddBoolToObject( playerObj, "isLocal", isLocal );

            /* Roles: I don't think a guest in a 3- or 4-device game knows
               which of the other players is host. Host is who it sends its
               moves to, but is there an order there? */
            XP_Bool isHost = game_getIsHost( game );
            isHost = isHost && isLocal;
            cJSON_AddBoolToObject( playerObj, "isHost", isHost );
            
            cJSON_AddItemToArray( players, playerObj );
        }
        cJSON_AddItemToArray( result, players );
    }
    return result;
}

static XP_U32
makeGameFromArgs( CmdWrapper* wr, cJSON* args )
{
    LaunchParams* params = wr->params;
    CurGameInfo gi = {};
    gi_copy( MPPARM(params->mpool) &gi, &params->pgi );
    gi.boardSize = 15;
    gi.traySize = 7;

    cJSON* tmp = cJSON_GetObjectItem( args, "nPlayers" );
    XP_ASSERT( !!tmp );
    gi.nPlayers = tmp->valueint;

    tmp = cJSON_GetObjectItem( args, "boardSize" );
    if ( !!tmp ) {
        gi.boardSize = tmp->valueint;
    }
    tmp = cJSON_GetObjectItem( args, "traySize" );
    if ( !!tmp ) {
        gi.traySize = tmp->valueint;
    }

    tmp = cJSON_GetObjectItem( args, "allowSub7" );
    gi.tradeSub7 = !!tmp && cJSON_IsTrue( tmp );

    tmp = cJSON_GetObjectItem( args, "isSolo" );
    XP_ASSERT( !!tmp );
    XP_Bool isSolo = cJSON_IsTrue( tmp );

    tmp = cJSON_GetObjectItem( args, "timerSeconds" );
    if ( !!tmp ) {
        gi.gameSeconds = tmp->valueint;
        if ( 0 != gi.gameSeconds ) {
            gi.timerEnabled = XP_TRUE;
        }
    }

    tmp = cJSON_GetObjectItem( args, "hostPosn" );
    XP_ASSERT( !!tmp );
    int hostPosn = tmp->valueint;
    replaceStringIfDifferent( params->mpool, &gi.players[hostPosn].name,
                              params->localName );
    for ( int ii = 0; ii < gi.nPlayers; ++ii ) {
        LocalPlayer* lp = &gi.players[ii];
        lp->isLocal = isSolo || ii == hostPosn;
        if ( isSolo ) {
            lp->robotIQ = ii == hostPosn ? 0 : 1;
        }
    }

    gi.serverRole = isSolo ? SERVER_STANDALONE : SERVER_ISHOST;

    tmp = cJSON_GetObjectItem( args, "dict" );
    XP_ASSERT( tmp );
    replaceStringIfDifferent( params->mpool, &gi.dictName, tmp->valuestring );

    /* cb_dims dims; */
    /* figureDims( aGlobals, &dims ); */

    XP_U32 newGameID;
#ifdef DEBUG
    bool success =
#endif
        (*wr->procs.newGame)( wr->closure, &gi, &newGameID );
    XP_ASSERT( success );

    gi_disposePlayerInfo( MPPARM(params->mpool) &gi );
    return newGameID;
}

static gchar*
readPacket(GInputStream* istream)
{
    gchar* result = NULL;
    for ( gsize siz = 0; ; ) {
        gchar buf[4*1024];
        gssize nread = g_input_stream_read( istream, buf, sizeof(buf), NULL, NULL );
        result = g_realloc( result, siz + nread + 1 );
        memcpy( &result[siz], buf, nread );
        siz += nread;
        if ( nread < sizeof(buf) ) {
            result[siz] = '\0';
            break;
        }
    }
    return result;
}

static gboolean
on_incoming_signal( GSocketService* XP_UNUSED(service),
                    GSocketConnection* connection,
                    GObject* XP_UNUSED(source_object), gpointer user_data )
{
    CmdWrapper* wr = (CmdWrapper*)user_data;
    // CursesAppGlobals* aGlobals = (CursesAppGlobals*)user_data;
    LaunchParams* params = wr->params;
    XP_U32 startTime = dutil_getCurSeconds( params->dutil, NULL_XWE );

    GInputStream* istream = g_io_stream_get_input_stream( G_IO_STREAM(connection) );

    gchar* buf = readPacket( istream );
    if ( !!buf ) {
        XP_LOGFF( "Message: \"%s\"\n", buf );

        cJSON* reply = cJSON_CreateArray();

        cJSON* cmds = cJSON_Parse( buf );
        g_free( buf );
        XP_LOGFF( "got msg with array of len %d", cJSON_GetArraySize(cmds) );
        for ( int ii = 0 ; ii < cJSON_GetArraySize(cmds) ; ++ii ) {
            cJSON* item = cJSON_GetArrayItem( cmds, ii );
            cJSON* cmd = cJSON_GetObjectItem( item, "cmd" );
            cJSON* key = cJSON_GetObjectItem( item, "key" );
            cJSON* args = cJSON_GetObjectItem( item, "args" );
            const char* cmdStr = cmd->valuestring;

            cJSON* response = NULL;
            XP_Bool success = XP_TRUE;

            if ( 0 == strcmp( cmdStr, "quit" ) ) {
                cJSON* gids;
                if ( getGamesStateForArgs( wr, args, &gids, NULL ) ) {
                    addObjectToObject( &response, "states", gids );
                }
                (*wr->procs.quit)( wr->closure );
            } else if ( 0 == strcmp( cmdStr, "getMQTTDevID" ) ) {
                MQTTDevID devID;
                dvc_getMQTTDevID( params->dutil, NULL_XWE, &devID );
                char buf[64];
                formatMQTTDevID( &devID, buf, sizeof(buf) );
                cJSON* devid = cJSON_CreateString( buf );
                addObjectToObject( &response, "mqtt", devid );
            } else if ( 0 == strcmp( cmdStr, "makeGame" ) ) {
                XP_U32 newGameID = makeGameFromArgs( wr, args );
                success = 0 != newGameID;
                if ( success ) {
                    addGIDToObject( &response, newGameID, "newGid" );
                }
            } else if ( 0 == strcmp( cmdStr, "invite" ) ) {
                success = inviteFromArgs( wr, args );
            } else if ( 0 == strcmp( cmdStr, "inviteRcvd" ) ) {
                success = newGuestFromArgs( wr, args );
            } else if ( 0 == strcmp( cmdStr, "moveIf" ) ) {
                success = moveifFromArgs( wr, args );
            } else if ( 0 == strcmp( cmdStr, "rematch" ) ) {
                XP_U32 newGameID = rematchFromArgs( wr, args );
                success = 0 != newGameID;
                if ( success ) {
                    addGIDToObject( &response, newGameID, "newGid" );
                }
            } else if ( 0 == strcmp( cmdStr, "stats" ) ) {
                cJSON* stats = getStats( wr );
                success = !!stats;
                if ( success ) {
                    addObjectToObject( &response, "stats", stats );
                }
            } else if ( 0 == strcmp( cmdStr, "getStates" ) ) {
                cJSON* gids;
                cJSON* orders;
                success = getGamesStateForArgs( wr, args, &gids, &orders );
                if ( success ) {
                    addObjectToObject( &response, "states", gids );
                    addObjectToObject( &response, "orders", orders );
                }
            } else if ( 0 == strcmp( cmdStr, "getPlayers" ) ) {
                cJSON* players = getPlayersForArgs( wr, args );
                addObjectToObject( &response, "players", players );
            } else if ( 0 == strcmp( cmdStr, "sendChat" ) ) {
                success = chatFromArgs( wr, args );
            } else if ( 0 == strcmp( cmdStr, "undoMove" ) ) {
                success = undoFromArgs( wr, args );
            } else if ( 0 == strcmp( cmdStr, "resign" ) ) {
                success = resignFromArgs( wr, args );
            } else if ( 0 == strcmp( cmdStr, "getKPs" ) ) {
                cJSON* result = knwnPlyrs( wr );
                success = !!result;
                if ( success ) {
                    addObjectToObject( &response, "kps", result );
                }
            } else {
                success = XP_FALSE;
                XP_ASSERT(0);
            }

            addSuccessToObject( &response, success );

            cJSON* tmp = cJSON_CreateObject();
            cJSON_AddStringToObject( tmp, "cmd", cmdStr );
            cJSON_AddNumberToObject( tmp, "key", key->valueint );
            cJSON_AddItemToObject( tmp,  "response", response );

            /*(void)*/cJSON_AddItemToArray( reply, tmp );
        }
        cJSON_Delete( cmds );   /* this apparently takes care of all children */

        char* replyStr = cJSON_PrintUnformatted( reply );
        short replyStrLen = strlen(replyStr);

        GOutputStream* ostream = g_io_stream_get_output_stream( G_IO_STREAM(connection) );
        gsize nwritten;
#ifdef DEBUG
        gboolean wroteall =
#endif
            g_output_stream_write_all( ostream, replyStr, replyStrLen,
                                                       &nwritten, NULL, NULL );
        XP_ASSERT( wroteall && nwritten == replyStrLen );
        GError* error = NULL;
        g_output_stream_close( ostream, NULL, &error );
        if ( !!error ) {
            XP_LOGFF( "g_output_stream_close()=>%s", error->message );
            g_error_free( error );
        }
        cJSON_Delete( reply );
        XP_LOGFF( "=> %s", replyStr );
        free( replyStr );
    }

    XP_U32 consumed = dutil_getCurSeconds( params->dutil, NULL_XWE ) - startTime;
    if ( 0 < consumed ) {
        XP_LOGFF( "took %d seconds", consumed );
    }
    // LOG_RETURN_VOID();
    return FALSE;
}

GSocketService*
cmds_addCmdListener( const CmdWrapper* wr )
{
    LOG_FUNC();
    XP_ASSERT( !!wr && !!wr->params );
    const XP_UCHAR* cmdsSocket = wr->params->cmdsSocket;
    GSocketService* service = NULL;
    if ( !!cmdsSocket ) {
        service = g_socket_service_new();

        struct sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        strncpy( addr.sun_path, cmdsSocket, sizeof(addr.sun_path) - 1);
        GSocketAddress* gsaddr
            = g_socket_address_new_from_native (&addr, sizeof(addr) );
        GError* error = NULL;
        if ( g_socket_listener_add_address( (GSocketListener*)service, gsaddr,
                                            G_SOCKET_TYPE_STREAM,
                                            G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL,
                                            &error ) ) {
        } else {
            XP_LOGFF( "g_socket_listener_add_address() failed: %s", error->message );
        }
        g_object_unref( gsaddr );

        g_signal_connect( service, "incoming", G_CALLBACK(on_incoming_signal),
                          (void*)wr );
    } else {
        XP_LOGFF( "cmdsSocket not set" );
    }
    LOG_RETURNF( "%p", service );
    return service;
}

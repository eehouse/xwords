/* -*- compile-command: "make -k -j3"; -*- */

/* 
 * Copyright 2010-2012 by Eric House (xwords@eehouse.org).  All rights
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <glib.h>

#include "dbmgr.h"
#include "strwpf.h"
#include "mlock.h"
#include "configs.h"
#include "xwrelay_priv.h"

#define GAMES_TABLE "games"
#define MSGS_TABLE "msgs"
#define DEVICES_TABLE "devices"

#define ARRAYSUM "sum_array(nPerDevice)"

static DBMgr* s_instance = NULL;

#define DELIM "\1"
#define MAX_NUM_PLAYERS 4

static void formatParams( char* paramValues[], int nParams, const char* fmt, 
                          char* buf, int bufLen, ... );
static int here_less_seed( const char* seeds, int perDeviceSum, 
                           unsigned short seed );
static void destr_function( void* conn );

/* static */ DBMgr*
DBMgr::Get() 
{
    if ( s_instance == NULL ) {
        s_instance = new DBMgr();
    }
    return s_instance;
} /* Get */

DBMgr::DBMgr()
{
    int tmp;
    RelayConfigs::GetConfigs()->GetValueFor( "USE_B64", &tmp );
    m_useB64 = tmp != 0;
    logf( XW_LOGINFO, "%s: m_useB64=%d", __func__, m_useB64 );

    pthread_key_create( &m_conn_key, destr_function );

    /* Now figure out what the largest cid currently is.  There must be a way
       to get postgres to do this for me.... */
    /* const char* query = "SELECT cid FROM games ORDER BY cid DESC LIMIT 1"; */
    /* PGresult* result = PQexec( m_pgconn, query ); */
    /* if ( 0 == PQntuples( result ) ) { */
    /*     m_nextCID = 1; */
    /* } else { */
    /*     char* value = PQgetvalue( result, 0, 0 ); */
    /*     m_nextCID = 1 + atoi( value ); */
    /* } */
    /* PQclear(result); */
    /* logf( XW_LOGINFO, "%s: m_nextCID=%d", __func__, m_nextCID ); */

    // I've seen rand returning the same series several times....
    srand( time( NULL ) );
}
 
DBMgr::~DBMgr()
{
    assert( s_instance == this );
    s_instance = NULL;

    int err = pthread_key_delete( m_conn_key );
    logf( XW_LOGINFO, "%s: pthread_key_delete=>%d", __func__, err );
}

void
DBMgr::AddNew( const char* cookie, const char* connName, CookieID cid, 
               int langCode, int nPlayersT, bool isPublic )
{         
    if ( !cookie ) cookie = "";
    if ( !connName ) connName = "";
 
    const char* command = "INSERT INTO " GAMES_TABLE
        " (cid, room, connName, nTotal, lang, pub)"
        " VALUES( $1, $2, $3, $4, $5, $6 )";
    int nParams = 6;
    char* paramValues[nParams];
    char buf[512];
    formatParams( paramValues, nParams,
                  "%d"DELIM"%s"DELIM"%s"DELIM"%d"DELIM"%d"DELIM"%s", 
                  buf, sizeof(buf), cid, cookie, connName, nPlayersT, 
                  langCode, isPublic?"TRUE":"FALSE" );

    PGresult* result = PQexecParams( getThreadConn(), command,
                                     nParams, NULL,
                                     paramValues, 
                                     NULL, NULL, 0 );
    if ( PGRES_COMMAND_OK != PQresultStatus(result) ) {
        logf( XW_LOGERROR, "PQexec=>%s;%s", PQresStatus(PQresultStatus(result)), 
              PQresultErrorMessage(result) );
    }
    PQclear( result );
}

/* Grab the row for a connname.  If the params don't check out, return false.
 */
bool
DBMgr::FindGameFor( const char* connName, char* cookieBuf, int bufLen,
                    unsigned short seed, HostID hid,
                    int nPlayersH, int nPlayersS,
                    int* langP, bool* isDead, CookieID* cidp )
{
    bool found = false;

    const char* fmt = "SELECT cid, room, lang, nPerDevice, dead FROM " 
        GAMES_TABLE " WHERE connName = '%s' AND nTotal = %d "
        "AND %d = seeds[%d] AND 'A' = ack[%d] "
        ;
    StrWPF query;
    query.printf( fmt, connName, nPlayersS, seed, hid, hid );
    logf( XW_LOGINFO, "query: %s", query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 >= PQntuples( result ) );
    found = 1 == PQntuples( result );
    if ( found ) {
        *cidp = atoi( PQgetvalue( result, 0, 0 ) );
        snprintf( cookieBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
        *langP = atoi( PQgetvalue( result, 0, 2 ) );
        *isDead = 't' == PQgetvalue( result, 0, 4 )[0];
    }
    PQclear( result );

    logf( XW_LOGINFO, "%s(%s)=>%d", __func__, connName, found );
    return found;
} /* FindGameFor */

CookieID
DBMgr::FindGame( const char* connName, char* cookieBuf, int bufLen,
                 int* langP, int* nPlayersTP, int* nPlayersHP, bool* isDead )
{
    CookieID cid = 0;

    const char* fmt = "SELECT cid, room, lang, nTotal, nPerDevice, dead FROM " 
        GAMES_TABLE " WHERE connName = '%s'"
        // " LIMIT 1"
        ;
    StrWPF query;
    query.printf( fmt, connName );
    logf( XW_LOGINFO, "query: %s", query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 >= PQntuples( result ) );
    if ( 1 == PQntuples( result ) ) {
        cid = atoi( PQgetvalue( result, 0, 0 ) );
        snprintf( cookieBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
        *langP = atoi( PQgetvalue( result, 0, 2 ) );
        *nPlayersTP = atoi( PQgetvalue( result, 0, 3 ) );
        *nPlayersHP = atoi( PQgetvalue( result, 0, 4 ) );
        *isDead = 't' == PQgetvalue( result, 0, 5 )[0];
    }
    PQclear( result );

    logf( XW_LOGINFO, "%s(%s)=>%d", __func__, connName, cid );
    return cid;
} /* FindGame */

bool
DBMgr::FindPlayer( DevIDRelay relayID, AddrInfo::ClientToken token, 
                   string& connName, HostID* hidp, unsigned short* seed )
{
    int nSuccesses = 0;

    const char* fmt = 
        "SELECT connName FROM %s WHERE %d = ANY(devids) AND %d = ANY(tokens)";
    StrWPF query;
    query.printf( fmt, GAMES_TABLE, relayID, token );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    int nTuples = PQntuples( result );
    vector<string> names(nTuples);
    for ( int ii = 0; ii < nTuples; ++ii ) {
        string name( PQgetvalue( result, ii, 0 ) );
        names.push_back( name );
    }
    PQclear( result );

    for ( vector<string>::const_iterator iter = names.begin();
          iter != names.end(); ++iter ) {
        const char* name = iter->c_str();
        for ( HostID hid = 1; hid <= MAX_NUM_PLAYERS; ++hid ) {
            fmt = "SELECT seeds[%d] FROM %s WHERE connname = '%s' "
                "AND devids[%d] = %d AND tokens[%d] = %d";
            StrWPF query;
            query.printf( fmt, hid, GAMES_TABLE, name,
                          hid, relayID, hid, token );
            result = PQexec( getThreadConn(), query.c_str() );
            int nTuples2 = PQntuples( result );
            for ( int jj = 0; jj < nTuples2; ++jj ) {
                connName = name;
                *hidp = hid;
                *seed = atoi( PQgetvalue( result, 0, 0 ) );
                ++nSuccesses;
            }
            PQclear( result );
        }
    }

    if ( 1 < nSuccesses ) {
        logf( XW_LOGERROR, "%s found %d matches!!!", __func__, nSuccesses );
    }

    return nSuccesses >= 1;
} // FindPlayer

bool
DBMgr::FindRelayIDFor( const char* connName, HostID hid, 
                       unsigned short seed, const DevID* host,
                       DevIDRelay* devIDP )
{
    DevIDRelay devID = DEVID_NONE;
    StrWPF query;
    query.printf( "SELECT devids[%d] FROM " GAMES_TABLE " WHERE "
                  "connname = '%s' AND seeds[%d] = %d", hid, 
                  connName, hid, seed );
    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    int nTuples = PQntuples( result );
    assert( nTuples <= 1 );
    bool found = nTuples == 1;
    if ( found ) {
        devID = (DevIDRelay)strtoul( PQgetvalue( result, 0, 0 ), NULL, 10 );
        *devIDP = devID;
        ReregisterDevice( devID, host, NULL, 0, NULL, NULL );
    }
    PQclear( result );
    if ( !found ) {
        logf( XW_LOGERROR, "%s: relayid not found for slot %d of %s)", 
              __func__, hid, connName );
    }
    return found;
}

bool
DBMgr::SeenSeed( const char* cookie, unsigned short seed,
                 int langCode, int nPlayersT, bool wantsPublic, 
                 char* connNameBuf, int bufLen, int* nPlayersHP, 
                 CookieID* cid )
{
    int nParams = 5;
    char* paramValues[nParams];
    char buf[512];
    formatParams( paramValues, nParams,
                  "%s"DELIM"%d"DELIM"%d"DELIM"%d"DELIM"%s", buf, sizeof(buf),
                  cookie, langCode, nPlayersT, seed, 
                  wantsPublic?"TRUE":"FALSE" );

    const char* cmd = "SELECT cid, connName, seeds, sum_array(nPerDevice) FROM "
        GAMES_TABLE
        " WHERE NOT dead"
        " AND room ILIKE $1"
        " AND lang = $2"
        " AND nTotal = $3"
        " AND $4 = ANY(seeds)"
        " AND $5 = pub"
        " ORDER BY ctime DESC"
        " LIMIT 1";

    PGresult* result = PQexecParams( getThreadConn(), cmd,
                                     nParams, NULL,
                                     paramValues, 
                                     NULL, NULL, 0 );
    bool found = 1 == PQntuples( result );
    if ( found ) {
        *cid = atoi( PQgetvalue( result, 0, 0 ) );
        *nPlayersHP = here_less_seed( PQgetvalue( result, 0, 2 ),
                                      atoi( PQgetvalue( result, 0, 3 ) ),
                                      seed );
        snprintf( connNameBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
    }
    PQclear( result );
    logf( XW_LOGINFO, "%s(%4X)=>%s", __func__, seed, found?"true":"false" );
    return found;
}

CookieID
DBMgr::FindOpen( const char* cookie, int lang, int nPlayersT, int nPlayersH,
                 bool wantsPublic, char* connNameBuf, int bufLen,
                 int* nPlayersHP )
{
    CookieID cid = 0;

    int nParams = 5;
    char* paramValues[nParams];
    char buf[512];
    formatParams( paramValues, nParams,
                  "%s"DELIM"%d"DELIM"%d"DELIM"%d"DELIM"%s", buf, sizeof(buf),
                  cookie, lang, nPlayersT, nPlayersH, wantsPublic?"TRUE":"FALSE" );

    /* NOTE: ILIKE, for case-insensitive comparison, is a postgres extension
       to SQL. */
    const char* cmd = "SELECT cid, connName, sum_array(nPerDevice) FROM "
        GAMES_TABLE
        " WHERE NOT dead"
        " AND room ILIKE $1"
        " AND lang = $2"
        " AND nTotal = $3"
        " AND $4 <= nTotal-sum_array(nPerDevice)"
        " AND $5 = pub"
        " LIMIT 1";

    PGresult* result = PQexecParams( getThreadConn(), cmd,
                                     nParams, NULL,
                                     paramValues, 
                                     NULL, NULL, 0 );
    if ( 1 == PQntuples( result ) ) {
        cid = atoi( PQgetvalue( result, 0, 0 ) );
        snprintf( connNameBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
        *nPlayersHP = atoi( PQgetvalue( result, 0, 2 ) );
        /* cid may be 0, but should use game anyway  */
    }
    PQclear( result );
    logf( XW_LOGINFO, "%s=>%d", __func__, cid );
    return cid;
} /* FindOpen */

bool
DBMgr::AllDevsAckd( const char* const connName )
{
    const char* cmd = "SELECT ntotal=sum_array(nperdevice) AND 'A'=ALL(ack) from " GAMES_TABLE
        " WHERE connName='%s'";
    StrWPF query;
    query.printf( cmd, connName );
    logf( XW_LOGINFO, "query: %s", query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    int nTuples = PQntuples( result );
    assert( nTuples <= 1 );
    bool full = nTuples == 1 && 't' == PQgetvalue( result, 0, 0 )[0];
    PQclear( result );
    return full;
}

// Return DevIDRelay for device, adding it to devices table IFF it's not
// already there.
DevIDRelay
DBMgr::RegisterDevice( const DevID* host, int clientVersion, 
                       const char* const desc, const char* const model,
                       const char* const osVers )
{
    DevIDRelay devID;
    assert( host->m_devIDType != ID_TYPE_NONE );
    int ii;
    bool success;

    // if it's already present, just return
    devID = getDevID( host );

    // If it's not present *and* of type ID_TYPE_RELAY, we can do nothing.
    // Otherwise proceed.
    if ( DEVID_NONE != devID ) {
        (void)UpdateDevice( devID );
    } else if ( ID_TYPE_RELAY < host->m_devIDType ) {
        // loop until we're successful inserting the unique key.  Ship with this
        // coming from random, but test with increasing values initially to make
        // sure duplicates are detected.
        const char* devidStr = host->m_devIDString.c_str();
        for ( success = false, ii = 0; !success; ++ii ) {
            assert( 10 > ii );  // better to check that we're looping BECAUSE
                                // of uniqueness problem.
            do {
                devID = (DevIDRelay)random();
            } while ( DEVID_NONE == devID );

            const char* command = "INSERT INTO " DEVICES_TABLE
                " (id, devType, devid, clntVers, versDesc, model, osvers)"
                " VALUES( $1, $2, $3, $4, $5, $6, $7 )";
            int nParams = 7;
            char* paramValues[nParams];
            char buf[1024];
            formatParams( paramValues, nParams,
                          "%d"DELIM"%d"DELIM"%s"DELIM"%d"DELIM"%s"DELIM"%s"DELIM"%s", 
                          buf, sizeof(buf), devID, host->m_devIDType, 
                          devidStr, clientVersion, desc, model, osVers );

            PGresult* result = PQexecParams( getThreadConn(), command,
                                             nParams, NULL,
                                             paramValues, 
                                             NULL, NULL, 0 );
            success = PGRES_COMMAND_OK == PQresultStatus(result);
            if ( !success ) {
                logf( XW_LOGERROR, "%s: PQexec=>%s;%s", __func__, 
                      PQresStatus(PQresultStatus(result)), 
                      PQresultErrorMessage(result) );
            }
            PQclear( result );
        }
    }
    return devID;
}

DevIDRelay 
DBMgr::RegisterDevice( const DevID* host )
{
    return RegisterDevice( host, 0, NULL, NULL, NULL );
}

void
DBMgr::ReregisterDevice( DevIDRelay relayID, const DevID* host, 
                         const char* const desc, int clientVersion, 
                         const char* const model, const char* const osVers )
{
    // First update the existing
    StrWPF query;
    query.printf( "UPDATE " DEVICES_TABLE " SET "
                  "rrcount = rrcount + 1, devType = %d, devid = '%s', ", 
                  host->m_devIDType, host->m_devIDString.c_str() );

    formatUpdate( query, desc, clientVersion, model, osVers, relayID );
    
    execSql( query );
}

// Return true if the relayID exists in the DB already
bool
DBMgr::UpdateDevice( DevIDRelay relayID, const char* const desc, 
                     int clientVersion, const char* const model,
                     const char* const osVers, bool check )
{
    bool exists = !check;
    if ( !exists ) {
        StrWPF test;
        test.printf( "id = %d", relayID );
        exists = 1 <= getCountWhere( DEVICES_TABLE, test );
    }

    if ( exists ) {
        StrWPF query;
        query.printf( "UPDATE " DEVICES_TABLE " SET " );
        formatUpdate( query, desc, clientVersion, model, osVers, relayID );
        execSql( query );
    }
    return exists;
}

bool
DBMgr::UpdateDevice( DevIDRelay relayID )
{
    return UpdateDevice( relayID, NULL, 0, NULL, NULL, false );
}

void
DBMgr::formatUpdate( StrWPF& query, const char* const desc, int clientVersion, 
                     const char* const model, const char* const osVers, 
                     DevIDRelay relayID )
{
    query.printf( "mtime='now'" );
    if ( NULL != desc && '\0' != desc[0] ) {
        query.printf( ", clntVers=%d, versDesc='%s'", clientVersion, desc );
    }
    if ( NULL != model && '\0' != model[0] ) {
        query.printf( ", model='%s'", model );
    }
    if ( NULL != osVers && '\0' != osVers[0] ) {
        query.printf( ", osvers='%s'", osVers );
    }
    query.printf( " WHERE id = %d", relayID );
}

HostID
DBMgr::AddToGame( const char* connName, HostID curID, int clientVersion, 
                  int nToAdd, unsigned short seed, const AddrInfo* addr,
                  DevIDRelay devID, bool ackd )
{
    HostID newID = curID;

    if ( newID == HOST_ID_NONE ) {
        int ackArr[4] = {0};
        int seedArr[4] = {0};
        readArray( connName, "nPerDevice", ackArr );
        readArray( connName, "seeds", seedArr );

        // If our seed's already there, grab that slot.  Otherwise grab the
        // first empty one.
        HostID firstEmpty = HOST_ID_NONE;
        for ( newID = HOST_ID_SERVER; newID <= 4; ++newID ) {
            if ( seedArr[newID-1] == seed ) {
                break;
            } else if ( HOST_ID_NONE == firstEmpty && 0 == ackArr[newID-1] ) {
                firstEmpty = newID;
            }
        }

        if ( 4 < newID && HOST_ID_NONE != firstEmpty ) {
            newID = firstEmpty;
        }
        logf( XW_LOGINFO, "%s: set newID = %d", __func__, newID );
    }
    assert( newID <= 4 );

    StrWPF query;
    query.printf( "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = %d,"
                   " clntVers[%d] = %d, seeds[%d] = %d, addrs[%d] = \'%s\', ", 
                   newID, nToAdd, newID, clientVersion, newID, seed, newID, 
                   inet_ntoa( addr->sin_addr() ) );
    if ( DEVID_NONE != devID ) {
        query.printf( "devids[%d] = %d, ", newID, devID );
    }
    query.printf( " tokens[%d] = %d, mtimes[%d]='now', ack[%d]=\'%c\'"
                  " WHERE connName = '%s'", newID, addr->clientToken(), 
                  newID, newID, ackd?'A':'a', connName );

    // Update the devices table too.  Eventually the clntVers field of the
    // games table should go away.
    if ( DEVID_NONE != devID ) {
        query.printf( "; UPDATE " DEVICES_TABLE " SET clntVers = %d"
                       " WHERE id = %d", clientVersion, devID );
    }

    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    execSql( query );

    return newID;
} /* AddToGame */

void
DBMgr::NoteAckd( const char* const connName, HostID id )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET ack[%d]='A'"
        " WHERE connName = '%s'";
    StrWPF query;
    query.printf( fmt, id, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

bool
DBMgr::RmDeviceByHid( const char* connName, HostID hid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = 0, "
        "seeds[%d] = 0, ack[%d]='-', mtimes[%d]='now' WHERE connName = '%s'";
    StrWPF query;
    query.printf( fmt, hid, hid, hid, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    return execSql( query );
}

HostID
DBMgr::HIDForSeed( const char* const connName, unsigned short seed )
{
    HostID hid = HOST_ID_NONE;
    char seeds[128] = {0};
    const char* fmt = "SELECT seeds FROM " GAMES_TABLE
        " WHERE connName = '%s'"
        " AND %d = ANY(seeds)";
    StrWPF query;
    query.printf( fmt, connName, seed );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    if ( 1 == PQntuples( result ) ) {
        snprintf( seeds, sizeof(seeds), "%s", PQgetvalue( result, 0, 0 ) );
    }
    PQclear( result );

    if ( 0 != seeds[0] ) {
        char *saveptr = NULL;
        int ii;
        char* str;
        for ( str = seeds, ii = 0; ; str = NULL, ++ii ) {
            char* tok = strtok_r( str, "{},", &saveptr );
            if ( NULL == tok ) {
                break;
            } else {
                int asint = atoi( tok );
                if ( asint == seed ) {
                    hid = ii + 1;
                    break;
                }
            }
        }
    } else {
        assert(0);              /* but don't ship with this!!!! */
    }

    return hid;
}

void
DBMgr::RmDeviceBySeed( const char* const connName, unsigned short seed )
{
    HostID hid = HIDForSeed( connName, seed );
    if ( hid != HOST_ID_NONE ) {
        RmDeviceByHid( connName, hid );
    }
} /* RmDeviceSeed */

bool
DBMgr::HaveDevice( const char* connName, HostID hid, int seed )
{
    bool found = false;
    const char* fmt = "SELECT * from " GAMES_TABLE 
        " WHERE connName = '%s' AND seeds[%d] = %d";
    StrWPF query;
    query.printf( fmt, connName, hid, seed );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    found = 1 == PQntuples( result );
    PQclear( result );
    return found;
}

bool
DBMgr::AddCID( const char* const connName, CookieID cid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET cid = %d "
        " WHERE connName = '%s' AND cid IS NULL";
    StrWPF query;
    query.printf( fmt, cid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    bool result = execSql( query );
    logf( XW_LOGINFO, "%s(cid=%d)=>%d", __func__, cid, result );
    return result;
}

void
DBMgr::ClearCID( const char* connName )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET cid = null "
        "WHERE connName = '%s'";
    StrWPF query;
    query.printf( fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

void
DBMgr::RecordSent( const char* const connName, HostID hid, int nBytes )
{
    assert( hid >= 0 && hid <= 4 );
    const char* fmt = "UPDATE " GAMES_TABLE " SET"
        " nsents[%d] = nsents[%d] + %d, mtimes[%d] = 'now'"
        " WHERE connName = '%s'";
    StrWPF query;
    query.printf( fmt, hid, hid, nBytes, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

void
DBMgr::RecordSent( const int* msgIDs, int nMsgIDs )
{
    if ( nMsgIDs > 0 ) {
        StrWPF query;
        query.printf( "SELECT connname,hid,sum(msglen)"
                      " FROM " MSGS_TABLE " WHERE id IN (" );
        for ( int ii = 0; ; ) {
            query.printf( "%d", msgIDs[ii] );
            if ( ++ii == nMsgIDs ) {
                break;
            } else {
                query.append( "," );
            }
        }
        query.append( ") GROUP BY connname,hid" );

        PGresult* result = PQexec( getThreadConn(), query.c_str() );
        if ( PGRES_TUPLES_OK == PQresultStatus( result ) ) {
            int ntuples = PQntuples( result );
            for ( int ii = 0; ii < ntuples; ++ii ) {
                RecordSent( PQgetvalue( result, ii, 0 ),
                            atoi( PQgetvalue( result, ii, 1 ) ),
                            atoi( PQgetvalue( result, ii, 2 ) ) );
            }
        }
        PQclear( result );
    }
}

void
DBMgr::RecordAddress( const char* const connName, HostID hid, 
                      const AddrInfo* addr )
{
    assert( hid >= 0 && hid <= 4 );
    const char* fmt = "UPDATE " GAMES_TABLE " SET addrs[%d] = \'%s\'"
        " WHERE connName = '%s'";
    StrWPF query;
    char* ntoa = inet_ntoa( addr->sin_addr() );
    query.printf( fmt, hid, ntoa, connName );
    logf( XW_LOGVERBOSE0, "%s: query: %s", __func__, query.c_str() );

    execSql( query );
}

void
DBMgr::GetPlayerCounts( const char* const connName, int* nTotal, int* nHere )
{
    const char* fmt = "SELECT ntotal, sum_array(nperdevice) FROM " GAMES_TABLE
        " WHERE connName = '%s'";
    StrWPF query;
    query.printf( fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 == PQntuples( result ) );
    *nTotal = atoi( PQgetvalue( result, 0, 0 ) );
    *nHere = atoi( PQgetvalue( result, 0, 1 ) );
    PQclear( result );
}

void
DBMgr::KillGame( const char* const connName, int hid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET dead = TRUE,"
        " nperdevice[%d] = - nperdevice[%d]"
        " WHERE connName = '%s'";
    StrWPF query;
    query.printf( fmt, hid, hid, connName );
    execSql( query );
}

void
DBMgr::ClearCIDs( void )
{
    execSql( "UPDATE " GAMES_TABLE " set cid = null" );
}

void
DBMgr::PublicRooms( int lang, int nPlayers, int* nNames, string& names )
{
    const char* fmt = "SELECT room, nTotal-sum_array(nPerDevice),"
        " round( extract( epoch from age('now', ctime)))"
        " FROM " GAMES_TABLE
        " WHERE NOT dead"
        " AND pub = TRUE"
        " AND lang = %d"
        " AND nTotal>sum_array(nPerDevice)"
        " AND nTotal = %d";

    StrWPF query;
    query.printf( fmt, lang, nPlayers );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    int nTuples = PQntuples( result );
    for ( int ii = 0; ii < nTuples; ++ii ) {
        names.append( PQgetvalue( result, ii, 0 ) );
        names.append( "/" );
        names.append( PQgetvalue( result, ii, 1 ) );
        names.append( "/" );
        names.append( PQgetvalue( result, ii, 2 ) );
        names.append( "\n" );
    }
    PQclear( result );
    *nNames = nTuples;
}

bool 
DBMgr::TokenFor( const char* const connName, int hid, DevIDRelay* devid,
                 AddrInfo::ClientToken* token )
{
    bool found = false;
    const char* fmt = "SELECT tokens[%d], devids[%d] FROM " GAMES_TABLE
        " WHERE connName='%s'";
    StrWPF query;
    query.printf( fmt, hid, hid, connName );
    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    if ( 1 == PQntuples( result ) ) {
        AddrInfo::ClientToken token_tmp = atoi( PQgetvalue( result, 0, 0 ) );
        DevIDRelay devid_tmp = atoi( PQgetvalue( result, 0, 1 ) );
        if ( AddrInfo::NULL_TOKEN != token_tmp && 0 != devid_tmp ) {
            *token = token_tmp;
            *devid = devid_tmp;
            found = true;
        }
    }
    PQclear( result );

    if ( found ) {
        logf( XW_LOGINFO, "%s(%s,%d)=>true (%d, %d)", __func__, connName, hid, 
              *devid, *token );
    } else {
        logf( XW_LOGINFO, "%s(%s,%d)=>false", __func__, connName, hid );
    }

    return found;
}

bool
DBMgr::execSql( const string& query )
{
    return execSql( query.c_str() );
}

bool
DBMgr::execSql( const char* const query )
{
    bool ok = false;
    for ( int ii = 0; !ok && ii < 3; ++ii ) {
        PGresult* result = PQexec( getThreadConn(), query );
        ok = PGRES_COMMAND_OK == PQresultStatus(result);
        if ( !ok ) {
            logf( XW_LOGERROR, "%s: PQexec=>%s;%s", __func__,
                  PQresStatus(PQresultStatus(result)), 
                  PQresultErrorMessage(result) );
            clearThreadConn();
            usleep( 20000 );
        }
        PQclear( result );
    }
    assert( ok );
    return ok;
}

void
DBMgr::readArray( const char* const connName, const char* column, int arr[]  ) /* len 4 */
{
    const char* fmt = "SELECT %s FROM " GAMES_TABLE " WHERE connName='%s'";

    StrWPF query;
    query.printf( fmt, column, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 == PQntuples( result ) );
    const char* arrStr = PQgetvalue( result, 0, 0 );
    logf( XW_LOGINFO, "%s: arrStr=\"%s\"", __func__, arrStr );
    sscanf( arrStr, "{%d,%d,%d,%d}", &arr[0], &arr[1], &arr[2], &arr[3] );
    PQclear( result );
}

DevIDRelay 
DBMgr::getDevID( const char* connName, int hid )
{
    DevIDRelay devID = DEVID_NONE;
    const char* fmt = "SELECT devids[%d] FROM " GAMES_TABLE " WHERE connName='%s'";
    StrWPF query;
    query.printf( fmt, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    if ( 1 == PQntuples( result ) ) {
        devID = (DevIDRelay)strtoul( PQgetvalue( result, 0, 0 ), NULL, 10 );
    }
    PQclear( result );
    return devID;
}

DevIDRelay 
DBMgr::getDevID( const DevID* devID )
{
    DevIDRelay rDevID = DEVID_NONE;
    DevIDType devIDType = devID->m_devIDType;
    StrWPF query;
    assert( ID_TYPE_NONE < devIDType );
    if ( ID_TYPE_RELAY == devIDType ) {
        // confirm it's there
        DevIDRelay cur = devID->asRelayID();
        if ( DEVID_NONE != cur ) {
            const char* fmt = "SELECT id FROM " DEVICES_TABLE " WHERE id=%d";
            query.printf( fmt, cur );
        }
    } else if ( 0 < devID->m_devIDString.size() ) {
        query.printf( "SELECT id FROM " DEVICES_TABLE 
                      " WHERE devtype=%d and devid = '%s'"
                      " ORDER BY ctime DESC LIMIT 1", 
                      devIDType, devID->m_devIDString.c_str() );
    }

    if ( 0 < query.size() ) {
        logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
        PGresult* result = PQexec( getThreadConn(), query.c_str() );
        int nTuples = PQntuples( result );
        assert( 1 >= nTuples );
        if ( 1 == nTuples ) {
            rDevID = (DevIDRelay)strtoul( PQgetvalue( result, 0, 0 ), NULL, 10 );
        }
        PQclear( result );
    }
    logf( XW_LOGINFO, "%s(in=%s)=>%d (0x%.8X)", __func__, 
          devID->m_devIDString.c_str(), rDevID, rDevID );
    return rDevID;
}

/*
 id | connname  | hid |   msg   
----+-----------+-----+---------
  1 | abcd:1234 |   2 | xyzzx
  2 | abcd:1234 |   2 | xyzzxxx
  3 | abcd:1234 |   3 | xyzzxxx
*/

int
DBMgr::CountStoredMessages( const char* const connName, int hid )
{
    StrWPF test;
    test.printf( "connname = '%s'", connName );
#ifdef HAVE_STIME
    test.printf( " AND stime IS NULL" );
#endif
    if ( hid != -1 ) {
        test.printf( " AND hid = %d", hid );
    }

    return getCountWhere( MSGS_TABLE, test );
}

int
DBMgr::CountStoredMessages( const char* const connName )
{
    return CountStoredMessages( connName, -1 );
} /* CountStoredMessages */

int
DBMgr::CountStoredMessages( DevIDRelay relayID )
{
    StrWPF test;
    test.printf( "devid = %d", relayID );
#ifdef HAVE_STIME
    test.printf( "AND stime IS NULL" );
#endif

    return getCountWhere( MSGS_TABLE, test );
}

void
DBMgr::StoreMessage( DevIDRelay devID, const uint8_t* const buf,
                     int len )
{
    clearHasNoMessages( devID );

    size_t newLen;
    const char* fmt = "INSERT INTO " MSGS_TABLE " "
        "(devid, %s, msglen) VALUES(%d, %s'%s', %d)";
    
    StrWPF query;
    if ( m_useB64 ) {
        gchar* b64 = g_base64_encode( buf, len );
        query.printf( fmt, "msg64", devID, "", b64, len );
        g_free( b64 );
    } else {
        uint8_t* bytes = PQescapeByteaConn( getThreadConn(), buf, 
                                                  len, &newLen );
        assert( NULL != bytes );
        query.printf( fmt, "msg",  devID, "E", bytes, len );
        PQfreemem( bytes );
    }

    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    execSql( query );
}

void
DBMgr::StoreMessage( const char* const connName, int hid, 
                     const uint8_t* buf, int len )
{
    DevIDRelay devID = getDevID( connName, hid );
    if ( DEVID_NONE == devID ) {
        logf( XW_LOGERROR, "%s: warning: devid not found for connName=%s, "
              "hid=%d", __func__, connName, hid );
    } else {
        clearHasNoMessages( devID );
    }

    size_t newLen;
    const char* fmt = "INSERT INTO " MSGS_TABLE " "
        "(connname, hid, devid, token, %s, msglen) "
        "VALUES( '%s', %d, %d, "
        "(SELECT tokens[%d] from " GAMES_TABLE " where connname='%s'), "
        "%s'%s', %d)";
    
    StrWPF query;
    if ( m_useB64 ) {
        gchar* b64 = g_base64_encode( buf, len );
        query.printf( fmt, "msg64", connName, hid, devID, hid, connName, 
                      "", b64, len );
        g_free( b64 );
    } else {
        uint8_t* bytes = PQescapeByteaConn( getThreadConn(), buf, 
                                                  len, &newLen );
        assert( NULL != bytes );
    
        query.printf( fmt, "msg", connName, hid, devID, hid, connName, 
                      "E", bytes, len );
        PQfreemem( bytes );
    }

    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    execSql( query );
}

void
DBMgr::decodeMessage( PGresult* result, bool useB64, int rowIndx, int b64indx, 
                      int byteaIndex, uint8_t* buf, size_t* buflen )
{
    const char* from = NULL;
    if ( useB64 ) {
        from = PQgetvalue( result, rowIndx, b64indx );
    }
    if ( NULL == from || '\0' == from[0] ) {
        useB64 = false;
        from = PQgetvalue( result, rowIndx, byteaIndex );
    }

    size_t to_length;
    if ( useB64 ) {
        gsize out_len;
        guchar* txt = g_base64_decode( (const gchar*)from, &out_len );
        to_length = out_len;
        assert( to_length <= *buflen );
        memcpy( buf, txt, to_length );
        g_free( txt );
    } else {
        uint8_t* bytes = PQunescapeBytea( (const uint8_t*)from, 
                                                &to_length );
        assert( to_length <= *buflen );
        memcpy( buf, bytes, to_length );
        PQfreemem( bytes );
    }
    *buflen = to_length;
}

void
DBMgr::storedMessagesImpl( string test, vector<DBMgr::MsgInfo>& msgs, 
                           bool nullConnnameOK )
{
    StrWPF query;
    query.printf( "SELECT id, msg64, msg, msglen, token, connname FROM "
                  MSGS_TABLE " WHERE %s "
#ifdef HAVE_STIME 
                   " AND stime IS NULL "
#endif
                   " AND (connname IN (SELECT connname FROM " GAMES_TABLE
                   " WHERE NOT " GAMES_TABLE ".dead)", test.c_str() );

    if ( nullConnnameOK ) {
        query.printf( " OR connname IS NULL ");
    }
    query.printf( ") ORDER BY id" );

    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    PGresult* result = PQexec( getThreadConn(), query.c_str() );

    int nTuples = PQntuples( result );
    for ( int ii = 0; ii < nTuples; ++ii ) {
        int id = atoi( PQgetvalue( result, ii, 0 ) );
        AddrInfo::ClientToken token = atoi( PQgetvalue( result, ii, 4 ) );
        const char* connname = PQgetvalue( result, ii, 5 );
        bool hasConnname = connname != NULL && '\0' != connname[0];
        MsgInfo msg( id, token, hasConnname );

        uint8_t buf[1024];
        size_t buflen = sizeof(buf);
        decodeMessage( result, m_useB64, ii, 1, 2, buf, &buflen );
        size_t msglen = atoi( PQgetvalue( result, ii, 3 ) );
        assert( 0 == msglen || buflen == msglen );
        msg.msg.insert( msg.msg.end(), buf, &buf[buflen] );
        msgs.push_back( msg );
    }
    PQclear( result );
}

void
DBMgr::GetStoredMessages( DevIDRelay relayID, vector<MsgInfo>& msgs )
{
    if ( !hasNoMessages( relayID ) ) {
        StrWPF query;
        query.printf( "devid=%d", relayID );
        storedMessagesImpl( query, msgs, true );

        if ( 0 == msgs.size() ) {
            setHasNoMessages( relayID );
        }
    }
}

void
DBMgr::GetStoredMessages( const char* const connName, HostID hid, 
                          vector<DBMgr::MsgInfo>& msgs )
{
    StrWPF query;
    query.printf( "hid = %d AND connname = '%s'", hid, connName );
    storedMessagesImpl( query, msgs, false );
}

void
DBMgr::RemoveStoredMessages( string& msgids )
{
    const char* fmt = 
#ifdef HAVE_STIME
        "UPDATE " MSGS_TABLE " SET stime='now' "
#else
        "DELETE FROM " MSGS_TABLE 
#endif
        " WHERE id IN (%s)";
    StrWPF query;
    query.printf( fmt, msgids.c_str() );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query.c_str() );
    execSql( query );
}

void
DBMgr::RemoveStoredMessages( const int* msgIDs, int nMsgIDs )
{
    if ( nMsgIDs > 0 ) {
        StrWPF ids;
        size_t len = 0;
        int ii;
        for ( ii = 0; ; ) {
            ids.printf( "%d", msgIDs[ii] );
            assert( len < sizeof(ids) );
            if ( ++ii == nMsgIDs ) {
                break;
            } else {
                ids.append( "," );
            }
        }
        RemoveStoredMessages( ids );
    }
}

void 
DBMgr::RemoveStoredMessages( vector<int>& idv )
{
    if ( 0 < idv.size() ) {
        StrWPF ids;
        vector<int>::const_iterator iter = idv.begin();
        for ( ; ; ) {
            ids.printf( "%d", *iter );
            if ( ++iter == idv.end() ) {
                break;
            }
            ids.printf( "," );
        }
        RemoveStoredMessages( ids );
    }
}

void
DBMgr::RemoveStoredMessage( const int msgID )
{
    RemoveStoredMessages( &msgID, 1 );
}

int
DBMgr::getCountWhere( const char* table, string& test )
{
    StrWPF query;
    query.printf( "SELECT count(*) FROM %s WHERE %s", table, test.c_str() );

    PGresult* result = PQexec( getThreadConn(), query.c_str() );
    assert( 1 == PQntuples( result ) );
    int count = atoi( PQgetvalue( result, 0, 0 ) );
    PQclear( result );
    return count;
}

bool DBMgr::hasNoMessages( DevIDRelay devid )
{
    MutexLock ml( &m_haveNoMessagesMutex );
    bool result = m_haveNoMessages.find(devid) != m_haveNoMessages.end();
    logf( XW_LOGINFO, "%s(devid=%d)=>%d", __func__, devid, result );
    return result;
}

void DBMgr::setHasNoMessages( DevIDRelay devid )
{
    logf( XW_LOGINFO, "%s(devid=%d)", __func__, devid );
    {
        MutexLock ml( &m_haveNoMessagesMutex );
        m_haveNoMessages.insert( devid );
    }
    assert( hasNoMessages( devid ) );
}

void DBMgr::clearHasNoMessages( DevIDRelay devid )
{
    logf( XW_LOGINFO, "%s(devid=%d)", __func__, devid );
    {
        MutexLock ml( &m_haveNoMessagesMutex );
        m_haveNoMessages.erase( devid );
    }
    assert( !hasNoMessages( devid ) );
}

static void
formatParams( char* paramValues[], int nParams, const char* fmt, char* buf, 
              int bufLen, ... )
{
    va_list ap;
    va_start( ap, bufLen );

    int len = vsnprintf( buf, bufLen, fmt, ap );
    assert( buf[len] == '\0' );

    int pnum;
    char* ptr = buf;
    for ( pnum = 0; pnum < nParams; ++pnum ) {
        paramValues[pnum] = ptr;
        for ( ; *ptr != '\0' && *ptr != DELIM[0]; ++ptr ) {
            // do nothing
            assert( ptr < &buf[bufLen] );
        }
        // we've found an end
        *ptr = '\0';
        ++ptr;
    }
    va_end(ap);
}

static int
here_less_seed( const char* seeds, int sumPerDevice, unsigned short seed )
{
    logf( XW_LOGINFO, "%s: find %x(%d) in \"%s\", sub from \"%d\"", __func__, 
          seed, seed, seeds, sumPerDevice );
    return sumPerDevice - 1;    /* FIXME */
}

static void
destr_function( void* conn )
{
    logf( XW_LOGINFO, "%s()", __func__ );
    PGconn* pgconn = (PGconn*)conn;
    PQfinish( pgconn );
}

void
DBMgr::clearThreadConn()
{
    logf( XW_LOGERROR, "%s called()", __func__ );
    PGconn* conn = (PGconn*)pthread_getspecific( m_conn_key );
    if ( NULL != conn ) {
        PQfinish( conn );
        int result = pthread_setspecific( m_conn_key, NULL );
        assert( 0 == result );
    }
}

PGconn* 
DBMgr::getThreadConn( void )
{
    PGconn* conn = (PGconn*)pthread_getspecific( m_conn_key );

    if ( NULL == conn ) {
        char buf[128];
        int port;
        if ( !RelayConfigs::GetConfigs()->GetValueFor( "DB_NAME", buf, 
                                                       sizeof(buf) ) ) {
            assert( 0 );
        }
        if ( !RelayConfigs::GetConfigs()->GetValueFor( "DB_PORT", &port ) ) {
            assert( 0 );
        }
        StrWPF params;
        params.printf( "dbname = %s ", buf );
        params.printf( "port = %d ", port );

        conn = PQconnectdb( params.c_str() );
        pthread_setspecific( m_conn_key, conn );
    }
    return conn;
}

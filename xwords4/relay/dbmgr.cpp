/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "dbmgr.h"
#include "mlock.h"
#include "xwrelay_priv.h"

#define DB_NAME "xwgames"
#define GAMES_TABLE "games"
#define MSGS_TABLE "msgs"

#define ARRAYSUM "sum_array(nPerDevice)"

static DBMgr* s_instance = NULL;

#define DELIM "\1"

static void formatParams( char* paramValues[], int nParams, const char* fmt, 
                          char* buf, int bufLen, ... );

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
    logf( XW_LOGINFO, "%s called", __func__ );

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
}
 
DBMgr::~DBMgr()
{
    logf( XW_LOGINFO, "%s called", __func__ );

    assert( s_instance == this );
    s_instance = NULL;
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

CookieID
DBMgr::FindGame( const char* connName, char* cookieBuf, int bufLen,
                 int* langP, int* nPlayersTP, int* nPlayersHP, bool* isDead )
{
    CookieID cid = 0;

    const char* fmt = "SELECT cid, room, lang, nTotal, nPerDevice, dead FROM " 
        GAMES_TABLE " WHERE connName = '%s'"
        " LIMIT 1";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "query: %s", query );

    PGresult* result = PQexec( getThreadConn(), query );
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
DBMgr::GameFull( const char* const connName )
{
    const char* cmd = "SELECT ntotal=sum_array(nperdevice) from " GAMES_TABLE
        " WHERE connName='%s'";
    char query[256];
    snprintf( query, sizeof(query), cmd, connName );
    logf( XW_LOGINFO, "query: %s", query );

    PGresult* result = PQexec( getThreadConn(), query );
    int nTuples = PQntuples( result );
    assert( nTuples <= 1 );
    bool full = 't' == PQgetvalue( result, 0, 0 )[0];
    PQclear( result );
    return full;
}

HostID
DBMgr::AddDevice( const char* connName, HostID curID, int nToAdd, 
                  unsigned short seed )
{
    HostID newID = curID;

    if ( newID == HOST_ID_NONE ) {
        int arr[4] = {0};
        readArray( connName, arr );
        for ( newID = HOST_ID_SERVER; newID <= 4; ++newID ) {
            if ( arr[newID-1] == 0 ) {
                break;
            }
        }
    }
    assert( newID <= 4 );

    const char* fmt = "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = %d,"
        " seeds[%d] = %d, mtimes[%d]='now'"
        " WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, newID, nToAdd, newID, seed, newID, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );

    return newID;
}

bool
DBMgr::RmDevice( const char* connName, HostID hid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = 0, "
        "seeds[%d] = 0, mtimes[%d]='now' WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, hid, hid, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    return execSql( query );
}

bool
DBMgr::HaveDevice( const char* connName, HostID hid, int seed )
{
    bool found = false;
    const char* fmt = "SELECT * from " GAMES_TABLE 
        " WHERE connName = '%s' AND seeds[%d] = %d";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName, hid, seed );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );
    PGresult* result = PQexec( getThreadConn(), query );
    found = 1 == PQntuples( result );
    PQclear( result );
    return found;
}

void
DBMgr::AddCID( const char* const connName, CookieID cid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET cid = %d "
        " WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, cid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

void
DBMgr::ClearCID( const char* connName )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET cid = null "
        "WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

void
DBMgr::RecordSent( const char* const connName, HostID hid, int nBytes )
{
    assert( hid >= 0 && hid <= 4 );
    const char* fmt = "UPDATE " GAMES_TABLE " SET"
        " nsent = nsent + %d, mtimes[%d] = 'now'"
        " WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, nBytes, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

void
DBMgr::GetPlayerCounts( const char* const connName, int* nTotal, int* nHere )
{
    const char* fmt = "SELECT ntotal, sum_array(nperdevice) FROM " GAMES_TABLE
        " WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( getThreadConn(), query );
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
    char query[256];
    snprintf( query, sizeof(query), fmt, hid, hid, connName );
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

    char query[256];
    snprintf( query, sizeof(query), fmt, lang, nPlayers );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( getThreadConn(), query );
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

int
DBMgr::PendingMsgCount( const char* connName, int hid )
{
    int count = 0;
    const char* fmt = "SELECT COUNT(*) FROM " MSGS_TABLE
        " WHERE connName = '%s' AND hid = %d";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName, hid );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( getThreadConn(), query );
    if ( 1 == PQntuples( result ) ) {
        count = atoi( PQgetvalue( result, 0, 0 ) );
    }
    PQclear( result );
    return count;
}

bool
DBMgr::execSql( const char* query )
{
    PGresult* result = PQexec( getThreadConn(), query );
    bool ok = PGRES_COMMAND_OK == PQresultStatus(result);
    if ( !ok ) {
        logf( XW_LOGERROR, "PQexec=>%s;%s", PQresStatus(PQresultStatus(result)), PQresultErrorMessage(result) );
    }
    PQclear( result );
    return ok;
}

void
DBMgr::readArray( const char* const connName, int arr[]  ) /* len 4 */
{
    const char* fmt = "SELECT nPerDevice FROM " GAMES_TABLE " WHERE connName='%s'";

    char query[256];
    snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( getThreadConn(), query );
    assert( 1 == PQntuples( result ) );
    const char* arrStr = PQgetvalue( result, 0, 0 );
    sscanf( arrStr, "{%d,%d,%d,%d}", &arr[0], &arr[1], &arr[2], &arr[3] );
    PQclear( result );
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
    const char* fmt = "SELECT count(*) FROM " MSGS_TABLE 
        " WHERE connname = '%s' ";

    char query[256];
    int len = snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    if ( hid != -1 ) {
        snprintf( &query[len], sizeof(query)-len, "AND hid = %d",
                  hid );
    }

    PGresult* result = PQexec( getThreadConn(), query );
    assert( 1 == PQntuples( result ) );
    int count = atoi( PQgetvalue( result, 0, 0 ) );
    PQclear( result );
    return count;
}

int
DBMgr::CountStoredMessages( const char* const connName )
{
    return CountStoredMessages( connName, -1 );
} /* CountStoredMessages */

void
DBMgr::StoreMessage( const char* const connName, int hid, 
                     const unsigned char* buf, int len )
{
    size_t newLen;
    const char* fmt = "INSERT INTO " MSGS_TABLE " (connname, hid, msg, msglen)"
        " VALUES( '%s', %d, E'%s', %d)";

    unsigned char* bytes = PQescapeByteaConn( getThreadConn(), buf, len, &newLen );
    assert( NULL != bytes );
    
    char query[newLen+128];
    unsigned int siz = snprintf( query, sizeof(query), fmt, connName, hid, 
                                 bytes, len );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );
    PQfreemem( bytes );
    assert( siz < sizeof(query) );

    execSql( query );
}

bool
DBMgr::GetNthStoredMessage( const char* const connName, int hid, 
                            int nn, unsigned char* buf, size_t* buflen, 
                            int* msgID )
{
    const char* fmt = "SELECT id, msg, msglen FROM " MSGS_TABLE
        " WHERE connName = '%s' AND hid = %d ORDER BY id LIMIT 1 OFFSET %d";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName, hid, nn );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( getThreadConn(), query );
    int nTuples = PQntuples( result );
    assert( nTuples <= 1 );

    bool found = nTuples == 1;
    if ( found ) {
        if ( NULL != msgID ) {
            *msgID = atoi( PQgetvalue( result, 0, 0 ) );
        }
        size_t msglen = atoi( PQgetvalue( result, 0, 2 ) );

        /* int len = PQgetlength( result, 0, 1 ); */
        const unsigned char* from =
            (const unsigned char* )PQgetvalue( result, 0, 1 );
        size_t to_length;
        unsigned char* bytes = PQunescapeBytea( from, &to_length );
        assert( to_length <= *buflen );
        memcpy( buf, bytes, to_length );
        PQfreemem( bytes );
        *buflen = to_length;
        assert( 0 == msglen || to_length == msglen );
    }
    PQclear( result );
    return found;
}

bool
DBMgr::GetStoredMessage( const char* const connName, int hid, 
                         unsigned char* buf, size_t* buflen, int* msgID )
{
    return GetNthStoredMessage( connName, hid, 0, buf, buflen, msgID );
}

void
DBMgr::RemoveStoredMessage( int msgID )
{
    const char* fmt = "DELETE from " MSGS_TABLE " WHERE id = %d";
    char query[256];
    snprintf( query, sizeof(query), fmt, msgID );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

static void
formatParams( char* paramValues[], int nParams, const char* fmt, char* buf, 
              int bufLen, ... )
{
    va_list ap;
    va_start( ap, bufLen );

    int len = vsnprintf( buf, bufLen, fmt, ap );

    int ii, pnum;
    for ( pnum = 0, ii = 0; ii < len && pnum < nParams; ++pnum ) {
        paramValues[pnum] = &buf[ii];
        for ( ; ii < len; ++ii ) {
            if ( buf[ii] == DELIM[0] ) {
                buf[ii] = '\0';
                ++ii;
                break;
            }
        }
    }
    va_end(ap);
} 

static void
destr_function( void* conn )
{
    logf( XW_LOGINFO, "%s()", __func__ );
    PGconn* pgconn = (PGconn*)conn;
    PQfinish( pgconn );
}

static pthread_key_t s_conn_key;

static void conn_key_alloc()
{
    logf( XW_LOGINFO, "%s()", __func__ );
    pthread_key_create( &s_conn_key, destr_function );
}

PGconn* 
DBMgr::getThreadConn( void )
{
    PGconn* conn = NULL;
    
    static pthread_once_t key_once = PTHREAD_ONCE_INIT;
    pthread_once( &key_once, conn_key_alloc );
    conn = (PGconn*)pthread_getspecific( s_conn_key );

    if ( NULL == conn ) {
        conn = PQconnectdb( "dbname = " DB_NAME );
        pthread_setspecific( s_conn_key, conn );
    }
    return conn;
}

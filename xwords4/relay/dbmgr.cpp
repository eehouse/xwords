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

#include "dbmgr.h"
#include "mlock.h"
#include "xwrelay_priv.h"

#define DB_NAME "xwgames"
#define GAMES_TABLE "games"
#define MSGS_TABLE "msgs"

#define ARRAYSUM "sum_array(nPerDevice)"

static DBMgr* s_instance = NULL;

static int sumArray( const char* const str );

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

    const char* fmt = "INSERT INTO " GAMES_TABLE
        " (cid, room, connName, nTotal, nPerDevice, lang, pub)"
        " VALUES( %d, '%s', '%s', %d, ARRAY[0,0,0,0], %d, %s )";
    char buf[256];
    snprintf( buf, sizeof(buf), fmt, cid/*m_nextCID++*/, cookie, connName, 
              nPlayersT, langCode, isPublic?"TRUE":"FALSE" );
    logf( XW_LOGINFO, "passing %s", buf );
    execSql( buf );
}

CookieID
DBMgr::FindGame( const char* connName, char* cookieBuf, int bufLen,
                 int* langP, int* nPlayersTP, int* nPlayersHP )
{
    CookieID cid = 0;

    const char* fmt = "SELECT cid, room, lang, nTotal, nPerDevice FROM " 
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

    /* NOTE: ILIKE, for case-insensitive comparison, is a postgres extension
       to SQL. */
    const char* fmt = "SELECT cid, connName, nPerDevice FROM " GAMES_TABLE
        " WHERE room ILIKE '%s'"
        " AND lang = %d"
        " AND nTotal = %d"
        " AND %d <= nTotal-sum_array(nPerDevice)"
        " AND %s = pub"
        " LIMIT 1";
    char query[256];
    snprintf( query, sizeof(query), fmt,
              cookie, lang, nPlayersT, nPlayersH, wantsPublic?"TRUE":"FALSE" );
    logf( XW_LOGINFO, "query: %s", query );

    PGresult* result = PQexec( getThreadConn(), query );
    if ( 1 == PQntuples( result ) ) {
        cid = atoi( PQgetvalue( result, 0, 0 ) );
        snprintf( connNameBuf, bufLen, "%s", PQgetvalue( result, 0, 1 ) );
        *nPlayersHP = sumArray( PQgetvalue( result, 0, 2 ) );
        /* cid may be 0, but should use game anyway  */
    }
    PQclear( result );
    logf( XW_LOGINFO, "%s=>%d", __func__, cid );
    return cid;
} /* FindOpen */

HostID
DBMgr::AddDevice( const char* connName, HostID curID, int nToAdd, 
                  unsigned short seed )
{
    HostID newID = curID;

    if ( newID == HOST_ID_NONE ) {
        int arr[4];
        readArray( connName, arr );
        for ( newID = HOST_ID_SERVER; newID <= 4; ++newID ) {
            if ( arr[newID-1] == 0 ) {
                break;
            }
        }
    }
    assert( newID <= 4 );

    const char* fmt = "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = %d,"
        " seeds[%d] = %d, mtime='now'"
        " WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, newID, nToAdd, newID, seed, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );

    return newID;
}

void
DBMgr::RmDevice( const char* connName, HostID hid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET nPerDevice[%d] = 0, "
        "seeds[%d] = 0, mtime='now' WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, hid, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

void
DBMgr::AddCID( const char* const connName, CookieID cid )
{
    const char* fmt = "UPDATE " GAMES_TABLE " SET cid = %d, "
        " mtime='now' WHERE connName = '%s'";
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
DBMgr::RecordSent( const char* const connName, int nBytes )
{
   const char* fmt = "UPDATE " GAMES_TABLE " SET"
       " nsent = nsent + %d, mtime = 'now'"
       " WHERE connName = '%s'";
   char query[256];
   snprintf( query, sizeof(query), fmt, nBytes, connName );
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
        " WHERE pub = TRUE"
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
DBMgr::PendingMsgCount( const char* connNameIDPair )
{
    int count = 0;
    const char* hid = strrchr( connNameIDPair, '/' );
    if ( NULL != hid ) {
        char name[MAX_CONNNAME_LEN];
        int connNameLen = hid - connNameIDPair;
        strncpy( name, connNameIDPair, connNameLen );
        name[connNameLen] = '\0';

        const char* fmt = "SELECT COUNT(*) FROM " MSGS_TABLE
            " WHERE connName = '%s' AND hid = %s";
        char query[256];
        snprintf( query, sizeof(query), fmt, name, hid+1 );
        logf( XW_LOGINFO, "%s: query: %s", __func__, query );

        PGresult* result = PQexec( getThreadConn(), query );
        if ( 1 == PQntuples( result ) ) {
            count = atoi( PQgetvalue( result, 0, 0 ) );
        }
        PQclear( result );
    }
    return count;
}

void
DBMgr::execSql( const char* query )
{
    PGresult* result = PQexec( getThreadConn(), query );
    if ( PGRES_COMMAND_OK != PQresultStatus(result) ) {
        logf( XW_LOGERROR, "PQexec=>%s;%s", PQresStatus(PQresultStatus(result)), PQresultErrorMessage(result) );
    }
    PQclear( result );
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

static int
sumArray( const char* const arrStr )
{
    int arr[4];
    sscanf( arrStr, "{%d,%d,%d,%d}", &arr[0], &arr[1], &arr[2], &arr[3] );
    int sum = 0;
    int ii;
    for ( ii = 0; ii < 4; ++ii ) {
        sum += arr[ii];
    }
    return sum;
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
    const char* fmt = "INSERT INTO " MSGS_TABLE " (connname, hid, msg)"
        " VALUES( '%s', %d, E'%s')";

    unsigned char* bytes = PQescapeByteaConn( getThreadConn(), buf, len, &newLen );
    assert( NULL != bytes );
    
    char query[newLen+128];
    unsigned int siz = snprintf( query, sizeof(query), fmt, connName, hid, bytes );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );
    PQfreemem( bytes );
    assert( siz < sizeof(query) );

    execSql( query );
}

bool
DBMgr::GetStoredMessage( const char* const connName, int hid, 
                         unsigned char* buf, size_t* buflen, int* msgID )
{
    const char* fmt = "SELECT id, msg FROM " MSGS_TABLE
        " WHERE connName = '%s' AND hid = %d ORDER BY id LIMIT 1";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName, hid );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( getThreadConn(), query );
    int nTuples = PQntuples( result );
    assert( nTuples <= 1 );

    bool found = nTuples == 1;
    if ( found ) {
        *msgID = atoi( PQgetvalue( result, 0, 0 ) );

        /* int len = PQgetlength( result, 0, 1 ); */
        const unsigned char* from =
            (const unsigned char* )PQgetvalue( result, 0, 1 );
        size_t to_length;
        unsigned char* bytes = PQunescapeBytea( from, &to_length );
        assert( to_length <= *buflen );
        memcpy( buf, bytes, to_length );
        PQfreemem( bytes );
        *buflen = to_length;
    }
    PQclear( result );
    return found;
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

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

#include "dbmgr.h"
#include "mlock.h"
#include "xwrelay_priv.h"

#define DB_NAME "xwgames"
#define TABLE_NAME "games"

#define ARRAYSUM "(nPerDevice[1]+nPerDevice[2]+nPerDevice[3]+nPerDevice[4])"

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
    m_pgconn = PQconnectdb( "dbname = " DB_NAME );
    logf( XW_LOGINFO, "%s:, m_pgconn: %p", __func__, m_pgconn );        

    ConnStatusType status = PQstatus( m_pgconn );
    if ( CONNECTION_OK != status ) {
        fprintf( stderr, "%s: unable to open db; does it exist?\n", __func__ );
        exit( 1 );
    }

    pthread_mutex_init( &m_dbMutex, NULL );

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
    PQfinish( m_pgconn );

    assert( s_instance == this );
    s_instance = NULL;
}

void
DBMgr::AddNew( const char* cookie, const char* connName, CookieID cid, 
               int langCode, int nPlayersT, bool isPublic )
{         
    if ( !cookie ) cookie = "";
    if ( !connName ) connName = "";

    const char* fmt = "INSERT INTO " TABLE_NAME
        "(cid, room, connName, nTotal, nPerDevice, lang, ispublic, ctime) "
        "VALUES( %d, '%s', '%s', %d, ARRAY[0,0,0,0], %d, %s, 'now' )";
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

    const char* fmt = "SELECT cid, room, lang, nTotal, nPerDevice FROM " TABLE_NAME
        " where connName = '%s' "
        "LIMIT 1";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "query: %s", query );

    MutexLock ml( &m_dbMutex );

    PGresult* result = PQexec( m_pgconn, query );
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

    const char* fmt = "SELECT cid, connName, nPerDevice FROM " TABLE_NAME " "
        " WHERE room = '%s'"
        " AND lang = %d"
        " AND nTotal = %d"
        " AND %d <= nTotal-" ARRAYSUM
        " AND %s = ispublic"
        " LIMIT 1";
    char query[256];
    snprintf( query, sizeof(query), fmt,
              cookie, lang, nPlayersT, nPlayersH, wantsPublic?"TRUE":"FALSE" );
    logf( XW_LOGINFO, "query: %s", query );

    MutexLock ml( &m_dbMutex );

    PGresult* result = PQexec( m_pgconn, query );
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
DBMgr::AddDevice( const char* connName, int nToAdd )
{
    HostID newID = HOST_ID_NONE;
    int arr[4];

    MutexLock ml( &m_dbMutex );

    readArray_locked( connName, arr );
    for ( newID = HOST_ID_SERVER; newID <= 4; ++newID ) {
        if ( arr[newID-1] == 0 ) {
            break;
        }
    }
    assert( newID <= 4 );

    const char* fmt = "UPDATE " TABLE_NAME " SET nPerDevice[%d] = %d "
        "WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, newID, nToAdd, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql_locked( query );

    return newID;
}

void
DBMgr::RmDevice( const char* connName, HostID hid )
{
    const char* fmt = "UPDATE " TABLE_NAME " SET nPerDevice[%d] = 0 "
        "WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, hid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

void
DBMgr::AddCID( const char* const connName, CookieID cid )
{
    const char* fmt = "UPDATE " TABLE_NAME " SET cid = %d "
        "WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, cid, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

void
DBMgr::ClearCID( const char* connName )
{
    const char* fmt = "UPDATE " TABLE_NAME " SET cid = null "
        "WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    execSql( query );
}

void
DBMgr::ClearCIDs( void )
{
    execSql( "UPDATE " TABLE_NAME " set cid = null" );
}

void
DBMgr::PublicRooms( int lang, int nPlayers, int* nNames, string& names )
{
    int ii;
    int nTuples;
    
    const char* fmt = "SELECT room, nTotal-" ARRAYSUM " FROM " TABLE_NAME 
        " WHERE ispublic = TRUE AND lang = %d AND ntotal =% d";

    char query[256];
    snprintf( query, sizeof(query), fmt, lang, nPlayers );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    MutexLock ml( &m_dbMutex );

    PGresult* result = PQexec( m_pgconn, query );
    nTuples = PQntuples( result );
    for ( ii = 0; ii < nTuples; ++ii ) {
        names.append( PQgetvalue( result, ii, 0 ) );
        names.append( "/" );
        names.append( PQgetvalue( result, ii, 1 ) );
    }
    PQclear( result );
    *nNames = nTuples;
}

void
DBMgr::execSql( const char* query )
{
    MutexLock ml( &m_dbMutex );
    execSql_locked( query );
}

void
DBMgr::execSql_locked( const char* query )
{
    PGresult* result = PQexec( m_pgconn, query );
    if ( PGRES_COMMAND_OK != PQresultStatus(result) ) {
        logf( XW_LOGERROR, "PQexec=>%s", PQresStatus(PQresultStatus(result) ));
        logf( XW_LOGERROR, "PQexec=>%s", PQresultErrorMessage(result) );
    }
    PQclear( result );
}

void
DBMgr::readArray_locked( const char* const connName, int arr[]  ) /* len 4 */
{
    const char* fmt = "SELECT nPerDevice FROM " TABLE_NAME " WHERE connName='%s'";

    char query[256];
    snprintf( query, sizeof(query), fmt, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( m_pgconn, query );
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
  Schema:
  CREATE TABLE games ( 
  cid integer,
  room VARCHAR(32),
  connName VARCHAR(64) UNIQUE PRIMARY KEY,
  nTotal INTEGER,
  nJoined INTEGER, 
  lang INTEGER,
  ctime TIMESTAMP,
  mtime TIMESTAMP
);

  May also want
  seeds INTEGER ARRAY,
  ipAddresses INTEGER ARRAY,

        
 */

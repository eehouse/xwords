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
#include "xwrelay_priv.h"

#define DB_NAME "xwgames"

static DBMgr* s_instance = NULL;

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
    assert( status == CONNECTION_OK );

    /* Now figure out what the largest cid currently is.  There must be a way
       to get postgres to do this for me.... */
    /* const char* query = "SELECT cid FROM games ORDER BY - cid LIMIT 1"; */
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
}

void
DBMgr::AddNew( const char* cookie, const char* connName, CookieID cid, 
               int langCode, int nPlayersT, int nPlayersH )
{         

#if 1
    if ( !cookie ) cookie = "";
    if ( !connName ) connName = "";

    const char* fmt = "INSERT INTO games "
        "(cid, cookie, connName, nTotal, nHere, lang) "
        "VALUES( %d, '%s', '%s', %d, %d, %d )";
    char buf[256];
    snprintf( buf, sizeof(buf), fmt, cid/*m_nextCID++*/, cookie, connName, 
              nPlayersT, nPlayersH, langCode );
    logf( XW_LOGINFO, "passing %s", buf );
    PGresult* result = PQexec( m_pgconn, buf );
    PQclear( result );
#else
    const char* command = "INSERT INTO games (cookie, connName, ntotal, nhere, lang) "
        "VALUES( $1, $2, $3, $4, $5 )";
    char nPlayersHBuf[4];
    char nPlayersTBuf[4];
    char langBuf[4];

    snprintf( nPlayersHBuf, sizeof(nPlayersHBuf), "%d", nPlayersH );
    snprintf( nPlayersTBuf, sizeof(nPlayersTBuf), "%d", nPlayersT );
    snprintf( langBuf, sizeof(langBuf), "%d", langCode );

    const char * const paramValues[] = { cookie, connName, nPlayersTBuf, nPlayersHBuf, langBuf };

    PGresult* result = PQexecParams( m_pgconn, command,
                                     sizeof(paramValues)/sizeof(paramValues[0]),
                                     NULL, /*const Oid *paramTypes,*/
                                     paramValues,
                                     NULL, /*const int *paramLengths,*/
                                     NULL, /*const int *paramFormats,*/
                                     0 /*int resultFormat*/ );
#endif
    logf( XW_LOGINFO, "PQexecParams=>%d", result );
}

CookieID
DBMgr::FindOpen( const char* cookie, int lang, int nPlayersT, int nPlayersH )
{
    CookieID cid = 0;

    const char* fmt = "SELECT cid from games where cookie = '%s' "
        "AND lang = %d "
        "AND nTotal = %d "
        "AND %d <= nTotal-nHere "
        "LIMIT 1";
    char query[256];
    snprintf( query, sizeof(query), fmt,
              cookie, lang, nPlayersT, nPlayersH );
    logf( XW_LOGINFO, "query: %s", query );

    PGresult* result = PQexec( m_pgconn, query );
    if ( 1 == PQntuples( result ) ) {
        cid = atoi( PQgetvalue( result, 0, 0 ) );
        assert( cid > 0 );
    }
    PQclear( result );
    logf( XW_LOGINFO, "%s=>%d", __func__, cid );
    return cid;
}

void
DBMgr::AddPlayers( const char* connName, int nToAdd )
{
    const char* fmt = "UPDATE games SET nHere = nHere+%d "
        "WHERE connName = '%s'";
    char query[256];
    snprintf( query, sizeof(query), fmt, nToAdd, connName );
    logf( XW_LOGINFO, "%s: query: %s", __func__, query );

    PGresult* result = PQexec( m_pgconn, query );
    PQclear( result );
}

/*
  Schema:
  CREATE TABLE games ( 
  cid integer,
  cookie VARCHAR(32),
  connName VARCHAR(64) UNIQUE PRIMARY KEY,
  nTotal INTEGER,
  nHere INTEGER, 
  lang INTEGER,
  ctime TIMESTAMP,
  mtime TIMESTAMP
);
        
 */

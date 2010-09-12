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

#include "dbmgr.h"
#include "xwrelay_priv.h"

#define DB_NAME "xwgames"

DBMgr::DBMgr()
{
    logf( XW_LOGINFO, "%s called", __func__ );
    m_pgconn = PQconnectdb( "dbname = " DB_NAME );
    logf( XW_LOGINFO, "%s:, m_pgconn: %p", __func__, m_pgconn );        

    ConnStatusType status = PQstatus( m_pgconn );
    assert( status == CONNECTION_OK );
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

    const char* fmt = "INSERT INTO games (cookie, connName, nTotal, nHere, lang) "
        "VALUES( '%s', '%s', %d, %d, %d )";
    char buf[256];
    snprintf( buf, sizeof(buf), fmt, cookie, connName, nPlayersT, nPlayersH, langCode );
    logf( XW_LOGINFO, "passing %s", buf );
    PGresult* result = PQexec( m_pgconn, buf );
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

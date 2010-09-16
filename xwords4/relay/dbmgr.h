/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option.
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

#ifndef _DBMGR_H_
#define _DBMGR_H_

#include "xwrelay.h"
#include <libpq-fe.h>

class DBMgr {
 public:
    static DBMgr* Get();

    ~DBMgr();

    void ClearCIDs( void );

    void AddNew( const char* cookie, const char* connName, CookieID cid, 
                 int langCode, int nPlayersT, bool isPublic );

    CookieID FindGame( const char* connName, char* cookieBuf, int bufLen,
                       int* langP, int* nPlayersTP );
    CookieID FindOpen( const char* cookie, int lang, int nPlayersT, 
                       int nPlayersH, bool wantsPublic, 
                       char* connNameBuf, int bufLen );

    void AddPlayers( const char* const connName, int nToAdd );
    void RmPlayers( const char* const connName, int nToAdd );
    void AddCID( const char* connName, CookieID cid );
    void ClearCID( const char* connName );

 private:
    DBMgr();
    void execSql( const char* query ); /* no-results query */
    PGconn* m_pgconn;
    //int m_nextCID;
}; /* DBMgr */


#endif

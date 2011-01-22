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

#include <string>

#include "xwrelay.h"
#include "xwrelay_priv.h"
#include <libpq-fe.h>

using namespace std;

class DBMgr {
 public:
    static DBMgr* Get();

    ~DBMgr();

    void ClearCIDs( void );

    void AddNew( const char* cookie, const char* connName, CookieID cid, 
                 int langCode, int nPlayersT, bool isPublic );

    CookieID FindGame( const char* connName, char* cookieBuf, int bufLen,
                       int* langP, int* nPlayersTP, int* nPlayersHP,
                       bool* isDead );
    CookieID FindOpen( const char* cookie, int lang, int nPlayersT, 
                       int nPlayersH, bool wantsPublic, 
                       char* connNameBuf, int bufLen, int* nPlayersHP );
    bool GameFull( const char* const connName );

    HostID AddDevice( const char* const connName, HostID curID,
                      int nToAdd, unsigned short seed );
    bool RmDevice( const char* const connName, HostID id );
    bool HaveDevice( const char* const connName, HostID id, int seed );
    void AddCID( const char* const connName, CookieID cid );
    void ClearCID( const char* connName );
    void RecordSent( const char* const connName, HostID hid, int nBytes );
    void GetPlayerCounts( const char* const connName, int* nTotal,
                          int* nHere );

    void KillGame( const char* const connName, int hid );

    /* Return list of roomName/playersStillWanted/age for open public games
       matching this language and total game size. Will probably want to cache
       lists locally and only update them every few seconds to avoid to many
       queries.*/
    void PublicRooms( int lang, int nPlayers, int* nNames, string& names );

    /* Return number of messages pending for connName:hostid pair passed in */
    int PendingMsgCount( const char* const connName, int hid );

    /* message storage -- different DB */
    int CountStoredMessages( const char* const connName );
    int CountStoredMessages( const char* const connName, int hid );
    void StoreMessage( const char* const connName, int hid, 
                       const unsigned char* const buf, int len );
    bool GetStoredMessage( const char* const connName, int hid, 
                           unsigned char* buf, size_t* buflen, int* msgID );
    bool GetNthStoredMessage( const char* const connName, int hid, int nn,
                              unsigned char* buf, size_t* buflen, int* msgID );
    void RemoveStoredMessage( int msgID );

 private:
    DBMgr();
    bool execSql( const char* query ); /* no-results query */
    void readArray( const char* const connName, int arr[] );

    PGconn* getThreadConn( void );
}; /* DBMgr */


#endif

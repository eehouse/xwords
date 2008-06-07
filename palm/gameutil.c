/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <DataMgr.h>
#include <TimeMgr.h>

#include "comtypes.h"
#include "comms.h"
#include "strutils.h"
#include "gameutil.h"
#include "xwords4defines.h"
#include "xwstream.h"
#include "palmmain.h"

XP_U16
countGameRecords( PalmAppGlobals* globals )
{
    LocalID id;
    DmOpenRef dbP;
    UInt16 numRecs;

    id = DMFINDDATABASE( globals, CARD_0, XW_GAMES_DBNAME );
    dbP = DMOPENDATABASE( globals, CARD_0, id, dmModeWrite );
    numRecs = DmNumRecords( dbP );
    DMCLOSEDATABASE( dbP );
    return numRecs;
}

void
deleteGameRecord( PalmAppGlobals* globals, XP_S16 index )
{
    LocalID id;
    DmOpenRef dbP;

    id = DMFINDDATABASE( globals, CARD_0, XW_GAMES_DBNAME );
    dbP = DMOPENDATABASE( globals, CARD_0, id, dmModeWrite );
    DmRemoveRecord( dbP, index );
    DMCLOSEDATABASE( dbP );
} /* deleteGameRecord */

XP_S16
duplicateGameRecord( PalmAppGlobals* globals, XP_S16 fromIndex )
{
    LocalID id;
    DmOpenRef dbP;
    MemHandle newRecord, curRecord;
    XP_U16 size;
    XP_S16 newIndex = fromIndex + 1;

    id = DMFINDDATABASE( globals, CARD_0, XW_GAMES_DBNAME );
    dbP = DMOPENDATABASE( globals, CARD_0, id, dmModeWrite );
    XP_ASSERT( fromIndex < countGameRecords(globals) );
    curRecord = DmQueryRecord( dbP, fromIndex );
    size = MemHandleSize( curRecord );
    newRecord = DmNewRecord( dbP, (XP_U16*)&newIndex, size );

    DmWrite( MemHandleLock(newRecord), 0, MemHandleLock(curRecord), size );

    MemHandleUnlock( curRecord );
    MemHandleUnlock( newRecord );

    DmReleaseRecord( dbP, newIndex, true );

    DMCLOSEDATABASE( dbP );

    return newIndex;
} /* duplicateGameRecord */

void
streamToGameRecord( PalmAppGlobals* globals, XWStreamCtxt* stream, 
                    XP_S16 index )
{
    LocalID id;
    DmOpenRef dbP;
    MemHandle handle;
    MemPtr tmpPtr, ptr;
    Err err;

    XP_U32 size = stream_getSize( stream );

    id = DMFINDDATABASE( globals, CARD_0, XW_GAMES_DBNAME );
    dbP = DMOPENDATABASE( globals, CARD_0, id, dmModeWrite );
    XP_ASSERT( !!dbP );

    if ( index == DmNumRecords(dbP) ) {
        handle = DmNewRecord( dbP, (XP_U16*)&index, size );
    } else {
        XP_ASSERT( index < countGameRecords(globals) );
        handle = DmGetRecord( dbP, index );
        MemHandleResize( handle, size );
    }

    tmpPtr = MemPtrNew(size);
    stream_getBytes( stream, tmpPtr, size );
    ptr = MemHandleLock( handle );
    err = DmWrite( ptr, 0, tmpPtr, size );
    XP_ASSERT( err == 0 );
    MemPtrFree( tmpPtr );

    MemHandleUnlock(handle);
    err = DmReleaseRecord( dbP, index, true );
    XP_ASSERT( err == 0 );
    DMCLOSEDATABASE( dbP );
} /* streamToGameRecord */

void
writeNameToGameRecord( PalmAppGlobals* globals, XP_S16 index, 
                       char* newName, XP_U16 len )
{
    LocalID id;
    DmOpenRef dbP;
    MemHandle handle;
    char name[MAX_GAMENAME_LENGTH];

    XP_ASSERT( len == XP_STRLEN(newName) );
    XP_ASSERT( len > 0 );
    XP_MEMSET( name, 0, sizeof(name) );
    if ( len >= MAX_GAMENAME_LENGTH ) {
        len = MAX_GAMENAME_LENGTH - 1;
    }
    XP_MEMCPY( name, newName, len+1 );

    id = DMFINDDATABASE( globals, CARD_0, XW_GAMES_DBNAME );
    dbP = DMOPENDATABASE( globals, CARD_0, id, dmModeWrite );

    if ( index == DmNumRecords(dbP) ) {
        handle = DmNewRecord(dbP, (XP_U16*)&index, MAX_GAMENAME_LENGTH);
    } else {
        XP_ASSERT( index < DmNumRecords(dbP) );
        handle = DmGetRecord( dbP, index );
    }
    XP_ASSERT( !!handle );
    DmWrite( MemHandleLock(handle), 0, name, MAX_GAMENAME_LENGTH );
    MemHandleUnlock( handle );
    (void)DmReleaseRecord( dbP, index, true );
    DMCLOSEDATABASE( dbP );
} /* writeNameToGameRecord */

void
nameFromRecord( PalmAppGlobals* globals, XP_S16 index, char* buf )
{
    LocalID id;
    DmOpenRef dbP;
    MemHandle handle;

    buf[0] = '\0';     /* init to empty string */

    if ( index < countGameRecords(globals) ) {

        id = DMFINDDATABASE( globals, CARD_0, XW_GAMES_DBNAME );
        if ( id != 0 ) {
            dbP = DMOPENDATABASE( globals, CARD_0, id, dmModeWrite );
            if ( dbP != 0 ) {
                handle = DmQueryRecord( dbP, index );

                XP_MEMCPY( buf, MemHandleLock(handle), MAX_GAMENAME_LENGTH );
                buf[MAX_GAMENAME_LENGTH-1] = '\0';

                MemHandleUnlock( handle );
                DMCLOSEDATABASE( dbP );
            }
        }
    } 
} /* nameFromRecord */

/*****************************************************************************
 * Later this will provide a default name based on a timestamp.
 *****************************************************************************/
#ifndef TIME_FORMAT
#define TIME_FORMAT tfColonAMPM
#endif
#ifndef DATE_FORMAT
#define DATE_FORMAT dfMDYLongWithComma
#endif
void
makeDefaultGameName( char* buf ) 
{
    char timeBuf[timeStringLength+1]; /* add 1 to be safe */
    char dateBuf[longDateStrLength+1];
    DateTimeType timeType;

    TimSecondsToDateTime( TimGetSeconds(), &timeType );
    TimeToAscii( timeType.hour, timeType.minute, TIME_FORMAT, timeBuf );

    DateToAscii( timeType.month, timeType.day, timeType.year, 
                 DATE_FORMAT, dateBuf );

    StrPrintF( buf, "%s, %s", dateBuf, timeBuf );
} /* makeDefaultGameName */


/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <ebm_object.h>
#include <string.h>
#include "frankgamesdb.h"
#include "frankids.h"

extern "C" {
# include "xptypes.h"
}

CGamesDB::CGamesDB(MPFORMAL const XP_UCHAR* fileName)
{
    this->nRecordsUsed = 0;
    this->nRecordsAllocated = 0;
    this->lockedIndex = -1;
    MPASSIGN( this->mpool, mpool );

    fRecords = (DBRecord*)NULL;

    // create or load file
    memset( &this->xwfile, 0, sizeof(xwfile) );
    strcpy( (char*)this->xwfile.name, (char*)fileName );
    strcpy( this->xwfile.publisher, PUB_ERICHOUSE );
    strcpy( this->xwfile.extension, EXT_XWORDSGAMES );

    size_t size = EBO_BLK_SIZE;
    int result = ebo_new( &this->xwfile, size, FALSE );
    BOOL exists = result < 0;

    XP_DEBUGF( "exists=%d", exists );

    if ( exists ) {
	readFromFile( &xwfile );
    }
} // CGamesDBn

CGamesDB::~CGamesDB()
{
    this->writeToFile();

    U16 nRecords = this->nRecordsUsed;
    for ( U16 i = 0; i < nRecords; ++i ) {
	DBRecord* record = &fRecords[i];
	XP_FREE( mpool, record->datum );
	if ( !!record->name ) {
	    XP_FREE( mpool, record->name );
	}
    }

    XP_FREE( mpool, fRecords );
} // ~CGamesDB

void* 
CGamesDB::getNthRecord( U16 index, U16* recordLen )
{
    XP_ASSERT( index < this->nRecordsUsed );
    XP_ASSERT( this->lockedIndex < 0 );
    this->lockedIndex = index;
    *recordLen = fRecords[index].length;

    XP_DEBUGF( "getNthRecord(%d) returning len=%d", index, *recordLen );

    return fRecords[index].datum;
} // getNthRecord

XP_UCHAR*
CGamesDB::getNthName( U16 index )
{
    XP_ASSERT( index < this->nRecordsUsed );
    XP_DEBUGF( "returning %dth name: %s", index, fRecords[index].name );
    return fRecords[index].name;
} /* getNthName */

void
CGamesDB::recordRelease( U16 index )
{
    XP_ASSERT( this->lockedIndex == index );
    this->lockedIndex = -1;
} // recordRelease

void
CGamesDB::putNthRecord( U16 index, void* ptr, U16 len )
{
    XP_DEBUGF( "putNthRecord(%d,ptr,%d)", index, len );
    XP_ASSERT( index <= nRecordsUsed );

    if ( index == nRecordsAllocated ) {	/* need a new one */
	ensureSpace( nRecordsUsed+1 );
    } else if ( !!fRecords[index].datum ) {
	XP_FREE(mpool, fRecords[index].datum );
    }

    void* datap = XP_MALLOC( mpool, len );
    XP_MEMCPY( datap, ptr, len );
    fRecords[index].datum = datap;
    fRecords[index].length = len;

    if ( index == this->nRecordsUsed ) {
	++this->nRecordsUsed;
    }
} // putNthRecord

void
CGamesDB::putNthName( U16 index, XP_UCHAR* name )
{
    XP_ASSERT( index <= this->nRecordsUsed );

    if ( index == nRecordsAllocated ) {	/* need a new one */
	ensureSpace( nRecordsUsed+1 );
    } else if ( !!fRecords[index].name ) {
	XP_FREE( mpool, fRecords[index].name );
    }

    fRecords[index].name = frankCopyStr( MPPARM(mpool) name);
    if ( index == this->nRecordsUsed ) {
	++this->nRecordsUsed;
    }
} /* putNthName */

void
CGamesDB::removeNthRecord( U16 index )
{
    XP_ASSERT( index < this->nRecordsUsed );
    
    if ( !!fRecords[index].datum ) {
	XP_FREE( mpool, fRecords[index].datum );
	if ( !!fRecords[index].name ) {
	    XP_FREE( mpool, fRecords[index].name );
	}
    }

    U16 nRecords = --this->nRecordsUsed;
    for ( U16 i = index; i < nRecords; ++i ) {
	fRecords[i] = fRecords[i+1];
    }
    fRecords[nRecords].datum = NULL;
    fRecords[nRecords].length = 0;
} // removeNthRecord

U16
CGamesDB::duplicateNthRecord( U16 index )
{
    XP_ASSERT( index < this->nRecordsUsed );

    ensureSpace( nRecordsUsed + 1 );

    U16 newIndex = index + 1;
    for ( U16 i = this->nRecordsUsed; i > newIndex; --i ) {
	fRecords[i] = fRecords[i-1];
    }
    ++this->nRecordsUsed;

    U16 len = fRecords[newIndex].length = fRecords[index].length;
    void* data = XP_MALLOC( mpool, len );
    XP_MEMCPY( data, fRecords[index].datum, len );
    fRecords[newIndex].datum = data;
    fRecords[newIndex].name = 
	frankCopyStr( MPPARM(mpool) fRecords[index].name );

    XP_DEBUGF( "done with duplicateNthRecord; returning %d", newIndex );

    return newIndex;
} /* duplicateNthRecord */

U16 
CGamesDB::countRecords()
{
    return this->nRecordsUsed;
} // countRecords

void 
CGamesDB::readFromFile( const ebo_name_t* xwfile )
{
    size_t size = EBO_BLK_SIZE;
    void* vmbase = (void*)ebo_roundup(OS_availaddr+GAMES_DB_OFFSET);
#ifdef DEBUG
    int result = ebo_mapin( xwfile, 0, vmbase, &size, 1 );
    XP_ASSERT( result >= 0 );
#else
    (void)ebo_mapin( xwfile, 0, vmbase, &size, 1 );
#endif
    XP_ASSERT( ((unsigned long)vmbase & 0x01) == 0 );

    U16 nRecords;
    unsigned char* ptr = (unsigned char*)vmbase;
    nRecords = *((U16*)ptr)++;
    XP_DEBUGF( "nRecords = %d", nRecords );
    this->nRecordsUsed = this->nRecordsAllocated = nRecords;

    fRecords = (DBRecord*)XP_MALLOC( mpool, nRecords * sizeof(fRecords[0]) );
    
    for ( U16 i = 0; i < nRecords; ++i ) {
	DBRecord* record = &fRecords[i];
	U8 nameLen = *ptr++;
	XP_UCHAR* name = (XP_UCHAR*)NULL;

	if ( nameLen > 0 ) {
	    name = (XP_UCHAR*)XP_MALLOC(mpool, nameLen+1);
	    XP_MEMCPY( name, ptr, nameLen );
	    name[nameLen] = '\0';
	    ptr += nameLen;
	} 
	record->name = name;

	/* fix alignment */
	while ( ((unsigned long)ptr & 0x00000001) != 0 ) {
	    ++ptr;
	}

	U16 len = record->length = *((U16*)ptr)++;
	XP_DEBUGF( "read len %d from offset %ld", len,
		   (unsigned long)ptr - (unsigned long)vmbase - 2 );

	record->datum = XP_MALLOC( mpool, len );
	XP_MEMCPY( record->datum, ptr, len );
	ptr += len;

	XP_DEBUGF( "Read %dth record", i );
	XP_ASSERT( ((unsigned char*)vmbase) + size >= ptr );
    }

    (void)ebo_unmap( vmbase, size );

    XP_DEBUGF( "read %d records from file", nRecords );
} // readFromFile

void
CGamesDB::writeToFile()
{
    size_t size = EBO_BLK_SIZE;
    void* vmbase = (void*)ebo_roundup(OS_availaddr+GAMES_DB_OFFSET);
#ifdef DEBUG
    int result = ebo_mapin( &this->xwfile, 0, vmbase, &size, 1 );
    XP_ASSERT( result >= 0 );
#else
    (void)ebo_mapin( &this->xwfile, 0, vmbase, &size, 1 );
#endif
    XP_ASSERT( ((unsigned long)vmbase & 0x01) == 0 );

    U16 nRecords = this->nRecordsUsed;
    unsigned char* ptr = (unsigned char*)vmbase;

    *(U16*)ptr = nRecords;
    ptr += sizeof(U16);

    for ( U16 i = 0; i < nRecords; ++i ) {

	XP_UCHAR* name = fRecords[i].name;
	U16 slen = !!name? XP_STRLEN( name ): 0;
	*ptr++ = slen;
	if ( slen > 0 ) {
	    XP_ASSERT( slen < 0x00FF );
	    XP_MEMCPY( ptr, name, slen );
	    ptr += slen;
	}

	/* fix alignment */
	while ( ((unsigned long)ptr & 0x00000001) != 0 ) {
	    ++ptr;
	}

	U16 len = fRecords[i].length;
	XP_DEBUGF( "writing len %d at offset %ld", len,
		   (unsigned long)ptr - (unsigned long)vmbase );
	*((U16*)ptr)++ = len;

	XP_MEMCPY( ptr, fRecords[i].datum, len );
	ptr += len;
    }

    ebo_unmap( vmbase, size );

    XP_WARNF( "finished writing %d recs to file", nRecords );
} // writeToFile

void
CGamesDB::ensureSpace( U16 nNeeded )
{
    if ( this->nRecordsAllocated < nNeeded ) {
	U16 newLen = nNeeded * sizeof(fRecords[0]);
	if ( !!fRecords ) {
	    fRecords = (DBRecord*)XP_REALLOC( mpool, fRecords, newLen );
	} else {
	    fRecords = (DBRecord*)XP_MALLOC( mpool, newLen );

	}

	U16 sizeAdded = (nNeeded - this->nRecordsAllocated) * sizeof(*fRecords);
	XP_MEMSET( fRecords + this->nRecordsAllocated, 0x00, sizeAdded );

	this->nRecordsAllocated = nNeeded;
	XP_DEBUGF( "ensureSpace: increasing nRecordsAllocated to %d (len=%d)", 
		   nRecordsAllocated, newLen );
    }
} /* ensureSpace */

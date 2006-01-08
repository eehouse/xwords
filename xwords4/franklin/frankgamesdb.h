/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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

#ifndef _FRANKGAMESDB_H_
#define _FRANKGAMESDB_H_

#include <assert.h>
#include <ctype.h>
#include "sys.h"
#include "gui.h"
#include "comtypes.h"
#include "mempool.h"

/* #include "ebm_object.h" */

struct DBRecord {
    U16 length;
    void* datum;
    XP_UCHAR* name;
};

class CGamesDB {

 private:
  U16 nRecordsUsed;
  U16 nRecordsAllocated;
  DBRecord* fRecords;
  S16 lockedIndex;
  ebo_name_t xwfile;

  void readFromFile( const ebo_name_t* xwfile );
  void writeToFile();
  void ensureSpace( U16 nNeeded );

  MPSLOT

 public:
  /* create or open if already exists */
  CGamesDB(MPFORMAL const XP_UCHAR* fileName);

  /* commit optimized in-memory representation to a flat "file" */
  ~CGamesDB();

  /* Return a ptr (read-only by convention!!) to the beginning of the
     record's data.  The ptr's valid until recordRelease is called. */
  void* getNthRecord( U16 index, U16* recordLen );
  void recordRelease( U16 index );
  XP_UCHAR* getNthName( U16 index );

  /* put data into a record, adding a new one, replacing or appending to an
     existing one */
  void putNthRecord( U16 index, void* ptr, U16 len );
  void appendNthRecord( U16 index, void* ptr, U16 len );
  void putNthName( U16 index, XP_UCHAR* name );

  U16 duplicateNthRecord( U16 index );
  void removeNthRecord( U16 index );

  /* how many records do I have.  indices passed by getter methods that are
     >= this number are illegal.  setters can pass a number = to this, in
     which case we create a new record. */
  U16 countRecords();
};

#endif

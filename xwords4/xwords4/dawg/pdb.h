
/* 
 * Copyright 1997 - 2002 by Eric House (fixin@peak.org).  All rights reserved.
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
 
typedef unsigned long DWORD;
typedef unsigned short WORD;

// all numbers in these structs are big-endian, MAC format
typedef struct DocHeader {
    char sName[32];	// 0x00
    DWORD dwUnknown1;	// 0x20 bytes
    DWORD dwTime1;	// 0x24 bytes
    DWORD dwTime2;	// 0x28
    DWORD dwTime3;	// 0x2C
    DWORD dwLastSync;	// 0x30
    DWORD ofsSort;	// 0x34
    DWORD ofsCatagories;// 0x38
    DWORD dwType;	// 0x3C
    DWORD dwCreator;	// 0x40
    DWORD dwUnknown2;	// 0x44
    DWORD dwUnknown3;	// 0x48
    WORD  wNumRecs;	// 0x4C
} DocHeader;

#define DOCHEADSZ 78

typedef struct RecordHeader {
    // <eeh> added type in experimentally! on 4/14
/*     char type[4]; */
    DWORD offset;
    DWORD bits; // high byte is flags, remaining three are a unique id
} RecordHeader;

#define RECHEADSZ 8

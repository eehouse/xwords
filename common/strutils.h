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

#ifndef _STRUTILS_H_
#define _STRUTILS_H_

#include "comtypes.h"
#include "model.h"

#ifdef CPLUS
extern "C" {
#endif

#define TILE_NBITS 6		/* 32 tiles plus the blank */

XP_U16 bitsForMax( XP_U32 n );

void traySetToStream( XWStreamCtxt* stream, TrayTileSet* ts );
void traySetFromStream( XWStreamCtxt* stream, TrayTileSet* ts );

XP_S32 signedFromStream( XWStreamCtxt* stream, XP_U16 nBits );
void signedToStream( XWStreamCtxt* stream, XP_U16 nBits, XP_S32 num );

XP_UCHAR* stringFromStream( MPFORMAL XWStreamCtxt* stream );
XP_U16 stringFromStreamHere( XWStreamCtxt* stream, XP_UCHAR* buf, XP_U16 len );
void stringToStream( XWStreamCtxt* stream, XP_UCHAR* str );

XP_UCHAR* copyString( MPFORMAL const XP_UCHAR* instr );
void replaceStringIfDifferent( MPFORMAL XP_UCHAR** curLoc, 
                               const XP_UCHAR* newStr );

XP_UCHAR* emptyStringIfNull( XP_UCHAR* str );

#ifdef CPLUS
}
#endif

#endif /* _STRUTILS_H_ */

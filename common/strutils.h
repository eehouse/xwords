/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#define TILE_NBITS 6 /* 32 tiles plus the blank */

XP_U16 bitsForMax( XP_U32 n );

void traySetToStream( XWStreamCtxt* stream, const TrayTileSet* ts );
void traySetFromStream( XWStreamCtxt* stream, TrayTileSet* ts );

XP_S32 signedFromStream( XWStreamCtxt* stream, XP_U16 nBits );
void signedToStream( XWStreamCtxt* stream, XP_U16 nBits, XP_S32 num );

XP_UCHAR* p_stringFromStream( MPFORMAL XWStreamCtxt* stream
#ifdef MEM_DEBUG
                              , const char* file, const char* func, 
                              XP_U32 lineNo 
#endif
                              );
#ifdef MEM_DEBUG
# define stringFromStream( p, in ) \
    p_stringFromStream( (p), (in), __FILE__,__func__,__LINE__ )
#else
# define stringFromStream( p, in ) p_stringFromStream( in )
#endif

XP_U16 stringFromStreamHere( XWStreamCtxt* stream, XP_UCHAR* buf, XP_U16 len );
void stringToStream( XWStreamCtxt* stream, const XP_UCHAR* str );

XP_UCHAR* p_copyString( MPFORMAL const XP_UCHAR* instr 
#ifdef MEM_DEBUG
                        , const char* file, const char* func, XP_U32 lineNo 
#endif
                      );
#ifdef MEM_DEBUG
# define copyString( p, in ) \
    p_copyString( (p), (in), __FILE__, __func__, __LINE__ )
#else
# define copyString( p, in ) p_copyString( in )
#endif


void p_replaceStringIfDifferent( MPFORMAL XP_UCHAR** curLoc, 
                                 const XP_UCHAR* newStr
#ifdef MEM_DEBUG
                                 , const char* file, const char* func, XP_U32 lineNo 
#endif
                               );
#ifdef MEM_DEBUG
# define replaceStringIfDifferent(p, sp, n) \
    p_replaceStringIfDifferent( (p), (sp), (n), __FILE__, __func__, __LINE__ )
#else
# define replaceStringIfDifferent(p, sp, n) p_replaceStringIfDifferent((sp),(n))
#endif


XP_UCHAR* emptyStringIfNull( XP_UCHAR* str );

/* Produce an array of ints 0..count-1, juggled */
XP_Bool randIntArray( XP_U16* rnums, XP_U16 count );

#ifdef XWFEATURE_SMS
void binToSms( XP_UCHAR* out, XP_U16* outlen, const XP_U8* in, XP_U16 inlen );
XP_Bool smsToBin( XP_U8* out, XP_U16* outlen, const XP_UCHAR* in, XP_U16 inlen );
#endif

#ifdef DEBUG
void log_hex(  const XP_U8*memp, XP_U16 len, const char* tag );
# define LOG_HEX(m,l,t) log_hex((const XP_U8*)(m),(l),(t))
#else
# define LOG_HEX(m,l,t)
#endif

#ifdef CPLUS
}
#endif

#endif /* _STRUTILS_H_ */

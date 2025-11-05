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
void sortTiles( TrayTileSet* dest, const TrayTileSet* src, XP_U16 skip );
void removeTile( TrayTileSet* tiles, XP_U16 index );

void scoresToStream( XWStreamCtxt* stream, XP_U16 nScores, const XP_U16* scores );
void scoresFromStream( XWStreamCtxt* stream, XP_U16 nScores, XP_U16* scores );

void moveInfoToStream( XWStreamCtxt* stream, const MoveInfo* mi,
                       XP_U16 bitsPerTile );
void moveInfoFromStream( XWStreamCtxt* stream, MoveInfo* mi,
                         XP_U16 bitsPerTile );

/* XP_S32 signedFromStream( XWStreamCtxt* stream, XP_U16 nBits ); */
/* void signedToStream( XWStreamCtxt* stream, XP_U16 nBits, XP_S32 num ); */

typedef enum {
    UPT_U8,
    UPT_U32,
    UPT_STRING,
    UPT_STREAM,
} UrlParamType;

typedef struct _UrlParamState {
    XP_Bool firstDone;
#ifdef DEBUG
    XP_U32 guard1;
    XP_U32 guard2;
#endif
} UrlParamState;
void urlParamToStream( XWStreamCtxt* stream, UrlParamState* state,
                       const XP_UCHAR* key, UrlParamType typ, ... );

typedef struct _UrlDecodeIter {
    const XP_UCHAR* key;
    int count;
    XWStreamPos pos;
} UrlDecodeIter;
XP_Bool urlDecodeFromStream( XW_DUtilCtxt* duc, XWStreamCtxt* stream,
                             UrlDecodeIter* iter, UrlParamType typ, ... );
XP_Bool urlParamFromStream( XW_DUtilCtxt* dutil, XWStreamCtxt* stream,
                            const XP_UCHAR* key, UrlParamType typ, ... );

typedef struct _URLKey {
    XP_UCHAR txt[16];
} URLKey;
typedef struct _URLParamIter {
    const XP_UCHAR* key;
} URLParamIter;
XP_Bool urlParamsFromStream( XWStreamCtxt* stream, URLParamIter* iter,
                             URLKey* key, XWStreamCtxt** val );

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

XP_U16 stringFromStreamHereImpl( XWStreamCtxt* stream, XP_UCHAR* buf, XP_U16 len
#ifdef DEBUG
                                 ,const char* func, int line
#endif
                                 );
#ifdef DEBUG
# define stringFromStreamHere( stream, buf, len )        \
    stringFromStreamHereImpl( (stream), (buf), (len), __func__, __LINE__ )
#else
# define stringFromStreamHere( stream, buf, len )        \
    stringFromStreamHereImpl( (stream), (buf), (len))
#endif

void stringToStream( XWStreamCtxt* stream, const XP_UCHAR* str );
XP_Bool matchFromStream( XWStreamCtxt* stream, const XP_U8* bytes, XP_U16 nBytes );

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

#define str2ChrArray( BUF, STR ) {              \
    size_t len = VSIZE(BUF);                    \
    XP_SNPRINTF( BUF, len, "%s", (STR) );       \
}

void insetRect( XP_Rect* rect, XP_S16 byWidth, XP_S16 byHeight );

XP_U32 augmentHash( XP_U32 hash, const XP_U8* ptr, XP_U16 len );
XP_U32 finishHash( XP_U32 hash );

XP_U16 tilesNBits( const XWStreamCtxt* stream );

void destroyStreamIf( XWStreamCtxt** stream );

const XP_UCHAR* lcToLocale( XP_LangCode lc );
XP_Bool haveLocaleToLc( const XP_UCHAR* isoCode, XP_LangCode* lc );

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


const XP_UCHAR* emptyStringIfNull( const XP_UCHAR* str );

/* Produce an array of ints 0..count-1, juggled */
XP_Bool randIntArray( XP_U16* rnums, XP_U16 count );

XP_U16 countBits( XP_U32 mask );

#ifdef XWFEATURE_BASE64
void binToB64( XP_UCHAR* b64, XP_U16* b64Len, const XP_U8* bin, XP_U16 binLen );
void binToB64Streams( XWStreamCtxt* out, XWStreamCtxt* in );
/* return false on malformed base64 data. Malformed includes input length not
   being a multiple of 4. */
XP_Bool b64ToBin( XP_U8* bin, XP_U16* binLen, XP_UCHAR b64[], XP_U16 b64Len );
XP_Bool b64ToBinStreams( XWStreamCtxt* out, XWStreamCtxt* in );
#endif

GameRef formatGR( XP_U32 gameID, DeviceRole role );

XP_UCHAR* formatMQTTDevTopic( const MQTTDevID* devid, XP_UCHAR* buf,
                                    XP_U16 bufLen );
XP_UCHAR* formatMQTTCtrlTopic( const MQTTDevID* devid, XP_UCHAR* buf,
                                     XP_U16 bufLen );
XP_UCHAR* formatMQTTDevID( const MQTTDevID* devid, XP_UCHAR* buf,
                                 XP_U16 bufLen );
XP_Bool strToMQTTCDevID( const XP_UCHAR* str, MQTTDevID* result );

#ifdef DEBUG
void assertSorted( const MoveInfo* mi );
void log_hex( const XP_U8* memp, XP_U16 len, const char* tag );
void log_devid( const MQTTDevID* devID, const XP_UCHAR* tag );
# define LOG_HEX(m,l,t) log_hex((const XP_U8*)(m),(l),(t))
# define LOG_DEVID(ID, TAG) log_devid((ID), (TAG))
#else
# define assertSorted(mi)
# define LOG_HEX(m,l,t)
# define LOG_DEVID(id, tag)
#endif


#ifdef CPLUS
}
#endif

#endif /* _STRUTILS_H_ */

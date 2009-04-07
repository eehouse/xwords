/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999-2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _XPTYPES_H_
#define _XPTYPES_H_

#include <PalmTypes.h>
#include <MemoryMgr.h>
#include <StringMgr.h>
#include <SysUtils.h>

#define XP_TRUE ((XP_Bool)(1==1))
#define XP_FALSE ((XP_Bool)(1==0))

typedef unsigned char XP_U8;
typedef signed char XP_S8;
typedef unsigned char XP_UCHAR;

typedef UInt16 XP_U16;
typedef Int16 XP_S16;

typedef unsigned long XP_U32;
typedef signed long XP_S32;

typedef signed short XP_FontCode; /* not sure how I'm using this yet */
typedef Boolean XP_Bool;
typedef XP_U32 XP_Time;

void palm_debugf(char*, ...);
void palm_assert(Boolean b, int line, const char* func, const char* file );
void palm_warnf( char* format, ... );
void palm_logf( char* format, ... );
XP_U16 palm_snprintf( XP_UCHAR* buf, XP_U16 len, const XP_UCHAR* format, ... );
XP_S16 palm_memcmp( XP_U8* p1, XP_U8* p2, XP_U16 nBytes );
XP_U8* palm_realloc(XP_U8* in, XP_U16 size);

#define XP_CR "\n"
#define XP_S "%s"

#define XP_RANDOM() SysRandom(0)

#ifdef MEM_DEBUG
# define XP_PLATMALLOC(nbytes)       MemPtrNew(nbytes)
# define XP_PLATREALLOC(p,s)         palm_realloc((p),(s))
# define XP_PLATFREE(p)              MemChunkFree(p)
#else
# define XP_MALLOC(p,nbytes)           MemPtrNew(nbytes)
# define XP_REALLOC(p,ptr,nbytes)        palm_realloc((ptr),(nbytes))
# define XP_FREE(pool,p)             MemChunkFree(p)
#endif


#define XP_MEMSET(src, val, nbytes)     MemSet( (src), (nbytes), (val) )
#define XP_MEMCPY(d,s,l)                MemMove((d),(s),(l))
#define XP_MEMCMP( a1, a2, l )          palm_memcmp((XP_U8*)(a1),(XP_U8*)(a2),(l))
/* MemCmp is reputed not to work on some versions of PalmOS */
/* #define XP_MEMCMP( a1, a2, l )  MemCmp((a1),(a2),(l)) */
#define XP_STRLEN(s)             StrLen((const char*)s)
#define XP_STRNCMP(s1,s2,l)      StrNCompare((const char*)(s1), \
                                            (const char*)(s2),(l))
#define XP_STRCMP(s1,s2)         StrCompare((s1),(s2))
#define XP_STRCAT(d,s)           StrCat((d),(s))
#define XP_SNPRINTF palm_snprintf
#define XP_STRNCPY( out, in, len ) StrCopy( out, in )

#define XP_MIN(a,b) ((a)<(b)?(a):(b))
#define XP_MAX(a,b) ((a)>(b)?(a):(b))
#define XP_ABS(a)   ((a)>=0?(a):-(a))

#ifdef DEBUG
#define XP_ASSERT(b) palm_assert(b, __LINE__, __func__, __FILE__)
#else
#define XP_ASSERT(b)
#endif

#ifdef DEBUG
# define XP_DEBUGF(...) palm_debugf(__VA_ARGS__)
#else
# define XP_DEBUGF(...)
#endif

#define XP_STATUSF XP_LOGF      /* for now */

#ifdef DEBUG
#define XP_LOGF(...) palm_logf(__VA_ARGS__)
#define XP_WARNF(...) palm_warnf(__VA_ARGS__)
#else
#define XP_WARNF(...)
#define XP_LOGF(...)
#endif

/* Assumes big-endian, of course */
#if defined __LITTLE_ENDIAN
# include "pace_man.h"
# define XP_NTOHL(l) Byte_Swap32(l)
# define XP_NTOHS(s) Byte_Swap16(s)
# define XP_HTONL(l) Byte_Swap32(l)
# define XP_HTONS(s) Byte_Swap16(s)
#elif defined __BIG_ENDIAN
# define XP_NTOHL(l) (l)
# define XP_NTOHS(s) (s)
# define XP_HTONL(l) (l)
# define XP_HTONS(s) (s)
#else
# error "pick one!!!"
#endif

#define XP_LD "%ld"
#define XP_P "%lx"

#ifdef FEATURE_SILK
# define XP_UNUSED_SILK(x) x
#else
# define XP_UNUSED_SILK(x) XP_UNUSED(x)
#endif

#ifndef XWFEATURE_IR
# define XP_UNUSED_IR(x) x
#else
# define XP_UNUSED_IR(x) XP_UNUSED(x)
#endif

#endif


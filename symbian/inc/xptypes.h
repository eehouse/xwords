/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999-2004 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <e32def.h>
#include <stdlib.h>

/* #include <coeccntx.h> */

/* #include <eikenv.h> */
/* #include <eikappui.h> */
/* #include <eikapp.h> */
/* #include <eikdoc.h> */
/* #include <eikmenup.h> */

/* #include <eikon.hrh> */

#ifdef CPLUS
extern "C" {
#endif

#define XP_TRUE ((XP_Bool)(1==1))
#define XP_FALSE ((XP_Bool)(1==0))

typedef TUint16 XP_U16;
typedef TInt16 XP_S16;

typedef TUint32 XP_U32;
typedef TInt32 XP_S32;

typedef TUint8 XP_U8;
typedef TInt8 XP_S8;
typedef unsigned char XP_UCHAR;
//typedef TText XP_UCHAR;         /* native two-byte char */

typedef signed short XP_FontCode; /* not sure how I'm using this yet */
typedef TBool XP_Bool;
typedef XP_U32 XP_Time;

#define SC(t,val) static_cast<t>(val)

#define XP_CR "\n"

#ifndef DEBUG
void p_ignore( char* fmt, ... );
#endif

void sym_debugf( char* aFmt, ...);
int sym_snprintf( XP_UCHAR* buf, XP_U16 len, const XP_UCHAR* format, ... );

XP_U32 sym_flip_long( unsigned long l );
XP_U16 sym_flip_short(unsigned short s);
void* sym_malloc(XP_U32 nbytes );
void* sym_realloc(void* p, XP_U32 nbytes);
void sym_free( void* p );
void sym_assert(XP_Bool b, XP_U32 line, const char* file );
void sym_memset( void* dest, XP_UCHAR val, XP_U32 nBytes );
XP_S16 sym_strcmp( XP_UCHAR* str1, XP_UCHAR* str2 );
XP_U32 sym_strlen( XP_UCHAR* str );
XP_S16 sym_strncmp( XP_UCHAR* str1, XP_UCHAR* str2, XP_U32 len );
void sym_memcpy( void* dest, const void* src, XP_U32 nbytes );
XP_S16 sym_memcmp( void* m1, void* m2, XP_U32 nbytes );
char* sym_strcat( XP_UCHAR* dest, const XP_UCHAR* src );

#define XP_RANDOM() rand()

#ifdef MEM_DEBUG
# define XP_PLATMALLOC(nbytes) sym_malloc(nbytes)
# define XP_PLATREALLOC(p,s)   sym_realloc((p), (s))
# define XP_PLATFREE(p)        sym_free(p)
#else
# define XP_MALLOC(pool, nbytes)       sym_malloc(nbytes)
# define XP_REALLOC(pool, p, bytes)    sym_realloc((p), (bytes))
# define XP_FREE(pool, p)              sym_free(p)
#endif

#define XP_MEMSET(src, val, nbytes) \
                                sym_memset( (src), (val), (nbytes) )
#define XP_MEMCPY(d,s,l)        sym_memcpy((d),(s),(l))
#define XP_MEMCMP( a1, a2, l )  sym_memcmp( (a1),(a2),(l))
#define XP_STRLEN(s)            sym_strlen((unsigned char*)(s))
#define XP_STRCMP(s1,s2)        sym_strcmp((XP_UCHAR*)(s1),(XP_UCHAR*)(s2))
#define XP_STRNCMP(s1,s2,l)     sym_strncmp((char*)(s1),(char*)(s2),(l))
#define XP_STRCAT(d,s)          sym_strcat((d),(s))

#define XP_SNPRINTF sym_snprintf

#define XP_MIN(a,b) ((a)<(b)?(a):(b))
#define XP_MAX(a,b) ((a)>(b)?(a):(b))

#ifdef DEBUG
# define XP_ASSERT(b) sym_assert((XP_Bool)(b), __LINE__, __FILE__ )
#else
# define XP_ASSERT(b)
#endif

#define XP_STATUSF XP_DEBUGF
#define XP_WARNF XP_DEBUGF

#ifdef DEBUG
# define XP_LOGF sym_debugf
# define XP_DEBUGF sym_debugf
#else
# define XP_LOGF if(0)p_ignore
# define XP_DEBUGF if(0)p_ignore
#endif

#define XP_NTOHL(l) sym_flip_long(l)
#define XP_NTOHS(s) sym_flip_short(s)
#define XP_HTONL(l) sym_flip_long(l)
#define XP_HTONS(s) sym_flip_short(s)

#define XP_LD "%d"

#ifdef CPLUS
}
#endif

#endif

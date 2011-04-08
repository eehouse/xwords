/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <android/log.h>

typedef unsigned char XP_U8;
typedef signed char XP_S8;

typedef unsigned short XP_U16;
typedef signed short XP_S16;

typedef unsigned long XP_U32;
typedef signed long XP_S32;

typedef char XP_UCHAR;

typedef signed short XP_FontCode; /* not sure how I'm using this yet */
typedef bool XP_Bool;
typedef XP_U32 XP_Time;

#define XP_TRUE ((XP_Bool)(1==1))
#define XP_FALSE ((XP_Bool)(1==0))

#define XP_S "%s"
#define XP_P "%p"
#define XP_CR "\n"
#define XP_LD "%ld"

# define XP_RANDOM() rand()

#ifdef MEM_DEBUG
# define XP_PLATMALLOC(nbytes) malloc(nbytes)
# define XP_PLATREALLOC(p,s)   realloc((p), (s))
# define XP_PLATFREE(p)        free(p)
#else
# define XP_MALLOC(pool, nbytes)       malloc(nbytes)
# define XP_REALLOC(pool, p, bytes)    realloc((p), (bytes))
# define XP_CALLOC( pool, bytes )      calloc( 1, (bytes) )
# define XP_FREE(pool, p)              free(p)
void and_freep( void** ptrp );
# define XP_FREEP(pool, p)             and_freep((void**)p)
#endif

#define XP_MEMSET(src, val, nbytes)     memset( (src), (val), (nbytes) )
#define XP_MEMCPY(d,s,l) memcpy((d),(s),(l))
#define XP_MEMMOVE(d,s,l) memmove((d),(s),(l))
#define XP_MEMCMP( a1, a2, l )  memcmp((a1),(a2),(l))
#define XP_STRLEN(s) strlen((s))
#define XP_STRCAT(d,s) strcat((d),(s))
#define XP_STRCMP(s1,s2)        strcmp((char*)(s1),(char*)(s2))
#define XP_STRNCMP(s1,s2,l)     strncmp((char*)(s1),(char*)(s2),(l))
#define XP_STRNCPY(s,d,l) strncpy((s),(d),(l))
#define XP_SNPRINTF snprintf

#define XP_MIN(a,b) ((a)<(b)?(a):(b))
#define XP_MAX(a,b) ((a)>(b)?(a):(b))

#ifdef DEBUG
void and_assert( const char* test, int line, const char* file, const char* func );
#define XP_ASSERT(b) if(!(b)) { and_assert(#b, __LINE__, __FILE__, __func__); }
#else
# define XP_ASSERT(b)
#endif

//#define XP_STATUSF if(0)p_ignore
#define XP_STATUSF XP_DEBUGF 

#ifdef ENABLE_LOGGING
#define XP_DEBUGF(...) __android_log_print( ANDROID_LOG_DEBUG, "tag", __VA_ARGS__)
#define XP_LOGF(...) __android_log_print( ANDROID_LOG_DEBUG, "tag", __VA_ARGS__)
#define XP_WARNF(...)  __android_log_print( ANDROID_LOG_DEBUG, "tag", __VA_ARGS__)
#else
#define XP_DEBUGF(...)
#define XP_LOGF(...)
#define XP_WARNF(...)
#endif

XP_U32 and_ntohl(XP_U32 l);
XP_U16 and_ntohs(XP_U16 s);
XP_U32 and_htonl(XP_U32 l);
XP_U16 and_htons(XP_U16 s);

#define XP_NTOHL(l) and_ntohl(l)
#define XP_NTOHS(s) and_ntohs(s)
#define XP_HTONL(l) and_htonl(l)
#define XP_HTONS(s) and_htons(s)

#ifdef CPLUS
extern "C" {
#endif

#ifdef CPLUS
}
#endif

#endif


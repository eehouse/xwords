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

#include "stdafx.h"
#include <stdlib.h>
#include "xwords4.h"
#include <commctrl.h>
#include <winuser.h>
#include <winsock2.h>

typedef unsigned char XP_U8;
typedef signed char XP_S8;

typedef unsigned short XP_U16;
typedef signed short XP_S16;

typedef unsigned long XP_U32;
typedef signed long XP_S32;

typedef char XP_UCHAR;

typedef signed short XP_FontCode; /* not sure how I'm using this yet */
typedef BOOL XP_Bool;
typedef XP_U32 XP_Time;

#define XP_TRUE ((XP_Bool)(1==1))
#define XP_FALSE ((XP_Bool)(1==0))

#define XP_S "%s"

#ifdef _WIN32_WCE
# define XP_RANDOM() Random()
#else
# define XP_RANDOM() rand()
#endif

#ifdef MEM_DEBUG
# define XP_PLATMALLOC(nbytes) malloc(nbytes)
# define XP_PLATREALLOC(p,s)   realloc((p), (s))
# define XP_PLATFREE(p)        free(p)
#else
# define XP_MALLOC(pool, nbytes)       malloc(nbytes)
# define XP_REALLOC(pool, p, bytes)    realloc((p), (bytes))
# define XP_FREE(pool, p)              free(p)
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
#define XP_SNPRINTF wince_snprintf

#define XP_MIN(a,b) ((a)<(b)?(a):(b))
#define XP_MAX(a,b) ((a)>(b)?(a):(b))

#ifdef DEBUG

#define XP_ASSERT(b) if(!(b)) { wince_assert(#b, __LINE__, __FILE__, __func__); }
#else
# define XP_ASSERT(b)
#endif

//#define XP_STATUSF if(0)p_ignore
#define XP_STATUSF XP_DEBUGF 

#ifdef ENABLE_LOGGING
#define XP_DEBUGF(...) wince_debugf(__VA_ARGS__)
#define XP_LOGF(...) wince_debugf(__VA_ARGS__)
#define XP_WARNF(...) wince_warnf(__VA_ARGS__)
#else
#define XP_DEBUGF(...)
#define XP_LOGF(...)
#define XP_WARNF(...)
#endif

#ifdef CPLUS
extern "C" {
#endif

void wince_assert(XP_UCHAR* s, int line, const char* fileName, const char* func );
void wince_debugf(const XP_UCHAR*, ...)
    __attribute__ ((format (printf, 1, 2)));
void wince_warnf(const XP_UCHAR*, ...)
    __attribute__ ((format (printf, 1, 2)));
void p_ignore(XP_UCHAR*, ...);
XP_U16 wince_snprintf( XP_UCHAR* buf, XP_U16 len, 
                       const XP_UCHAR* format, ... );

#define XP_NTOHL(l) ntohl(l)
#define XP_NTOHS(s) ntohs(s)
#define XP_HTONL(l) htonl(l)
#define XP_HTONS(s) htons(s)

#define XP_LD "%ld"
#define XP_P "%p"

/* The pocketpc sdk on linux renames certain functions to avoid conflicts
   with same-named posix symbols. */
/* #if defined  __GNUC__ && defined _WIN32_WCE */
/* # define MS(func) M$_##func */
/* #else */
# define MS(func) func
/* #endif */

#ifdef _WIN32_WCE
# undef CALLBACK
# define CALLBACK
#endif

#ifdef _WIN32_WCE
# define XP_UNUSED_CE(x) XP_UNUSED(x)
# define XP_UNUSED_32(x) x
#else
# define XP_UNUSED_32(x) XP_UNUSED(x)
# define XP_UNUSED_CE(x) x
#endif

#ifdef CPLUS
}
#endif

#endif


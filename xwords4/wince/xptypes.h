/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999-2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include <Winsock.h>

/* #include <stddef.h> */
/* #include <stdio.h> */
/* #include <Heaps.h> */
/* #include <string.h> */
/* #include <stdlib.h> */
/* #include <assert.h> */

typedef unsigned char XP_U8;
typedef signed char XP_S8;

typedef unsigned short XP_U16;
typedef signed short XP_S16;

typedef unsigned long XP_U32;
typedef signed long XP_S32;

typedef unsigned char XP_UCHAR;

typedef signed short XP_FontCode; /* not sure how I'm using this yet */
typedef BOOL XP_Bool;
typedef XP_U32 XP_Time;

#define XP_TRUE ((XP_Bool)(1==1))
#define XP_FALSE ((XP_Bool)(1==0))

#define XP_CR "\015\012" /* 'Doze expects a carraige return followed by a linefeed */

#define XP_RANDOM() Random()

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
#define XP_MEMCMP( a1, a2, l )  memcmp((a1),(a2),(l))
#define XP_STRLEN(s) strlen((char*)(s))
#define XP_STRCAT(d,s) strcat((d),(s))
#define XP_STRCMP(s1,s2)        strcmp((char*)(s1),(char*)(s2))
#define XP_STRNCMP(s1,s2,l)     strncmp((char*)(s1),(char*)(s2),(l))
#define XP_SNPRINTF wince_snprintf

#define XP_MIN(a,b) ((a)<(b)?(a):(b))
#define XP_MAX(a,b) ((a)>(b)?(a):(b))

#ifdef DEBUG

#define XP_ASSERT(b) if(!(b)) { wince_assert(#b, __LINE__, __FILE__); }
#else
# define XP_ASSERT(b)
#endif

//#define XP_STATUSF if(0)p_ignore
#define XP_STATUSF XP_DEBUGF 
#define XP_WARNF XP_DEBUGF

#ifdef DEBUG
#define XP_DEBUGF wince_debugf
#define XP_LOGF wince_debugf
#else
#define XP_DEBUGF if(0)p_ignore
#define XP_LOGF if(0)p_ignore
#endif

#ifdef CPLUS
extern "C" {
#endif

void wince_assert(XP_UCHAR* s, int line, char* fileName );
void wince_debugf(XP_UCHAR*, ...);
void p_ignore(XP_UCHAR*, ...);
XP_U16 wince_snprintf( XP_UCHAR* buf, XP_U16 len, XP_UCHAR* format, ... );

#define XP_NTOHL(l) ntohl(l)
#define XP_NTOHS(s) ntohs(s)
#define XP_HTONL(l) htonl(l)
#define XP_HTONS(s) htons(s)

#ifdef CPLUS
}
#endif

#endif


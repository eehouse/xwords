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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gui_types.h>
#include <assert.h>

#ifdef CPLUS
extern "C" {
#endif

#define XP_TRUE ((XP_Bool)(1==1))
#define XP_FALSE ((XP_Bool)(1==0))

typedef U8 XP_U8;
typedef S8 XP_S8;
typedef unsigned char XP_UCHAR;

typedef U16 XP_U16;
typedef S16 XP_S16;

typedef U32 XP_U32;
/* typedef S32 XP_A32; */
typedef S32 XP_S32;

typedef signed short XP_FontCode; /* not sure how I'm using this yet */
typedef BOOL XP_Bool;
typedef U32 XP_Time;

#define XP_CR "\n"

void frank_insetRect( RECT* r, short byWhat );
void frank_debugf(char*, ...);
void p_ignore(char*, ...);
int frank_snprintf( XP_UCHAR* buf, XP_U16 len, XP_UCHAR* format, ... );
unsigned long frank_flipLong( unsigned long l );
unsigned short frank_flipShort(unsigned short s);

#define XP_RANDOM() rand()

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
#define XP_STRCMP(s1,s2)        strcmp((char*)(s1),(char*)(s2))
#define XP_STRNCMP(s1,s2,l)     strncmp((char*)(s1),(char*)(s2),(l))
#define XP_STRCAT(d,s) strcat((d),(s))

#define XP_SNPRINTF frank_snprintf

#define XP_MIN(a,b) ((a)<(b)?(a):(b))
#define XP_MAX(a,b) ((a)>(b)?(a):(b))

#ifdef DEBUG
#define XP_ASSERT(b) assert(b)
#else
#define XP_ASSERT(b)
#endif

#define XP_STATUSF XP_DEBUGF
#define XP_WARNF XP_DEBUGF

#ifdef DEBUG
#define XP_LOGF frank_debugf
#define XP_DEBUGF frank_debugf
#else
#define XP_LOGF if(0)p_ignore
#define XP_DEBUGF if(0)p_ignore
#endif

#define XP_NTOHL(l) frank_flipLong(l)
#define XP_NTOHS(s) frank_flipShort(s)
#define XP_HTONL(l) frank_flipLong(l)
#define XP_HTONS(s) frank_flipShort(s)

#ifdef CPLUS
}
#endif

#endif

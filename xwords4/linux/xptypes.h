/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2009 by Eric House (xwords@eehouse.org).  All rights
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>		/* memset */
#include <assert.h>		/* memset */
#include <unistd.h>
#include <netinet/in.h>

#include <glib.h>
#ifdef PLATFORM_GTK
# include <gdk/gdk.h>
# include <gtk/gtk.h>
#endif

#define XP_TRUE  ((XP_Bool)(1==1))
#define XP_FALSE ((XP_Bool)(1==0))

typedef unsigned char XP_U8;
typedef signed char XP_S8;

#ifdef XWFEATURE_UNICODE
typedef gchar XP_UCHAR;
# define XP_L(s) s
# define XP_S XP_L("%s")
#else
/* This doesn't work.  Turn on UNICODE for now... */
typedef char XP_UCHAR;
# define XP_L(s) ##s
#endif

typedef uint16_t XP_U16;
typedef int16_t XP_S16;

typedef uint32_t XP_U32;
typedef int32_t XP_S32;

typedef signed short XP_FontCode; /* not sure how I'm using this yet */
typedef unsigned char XP_Bool;

#ifdef PLATFORM_GTK
typedef guint32 XP_Time;
#else
typedef unsigned long XP_Time;
#endif

typedef void* XWEnv;

#define XP_CR XP_L("\n")

#define XP_STATUSF XP_DEBUGF
#define XP_LOGF XP_DEBUGF

#ifdef DEBUG
extern void linux_debugf(const char*, ...)
    __attribute__ ((format (printf, 1, 2)));
# define XP_DEBUGF(...) linux_debugf(__VA_ARGS__)


#ifndef SHORTFILE
# define SHORTFILE __FILE__
#endif

extern void linux_gid_debugff(XP_U32 gid, const char* func, const char* file, int line, const char* fmt, ...)
    __attribute__ ((format (printf, 5, 6)));
extern void linux_debugff(const char* func, const char* file, int line, const char* fmt, ...)
    __attribute__ ((format (printf, 4, 5)));
# define XP_GID_LOGFF( GID, FMT, ... )                                  \
    linux_gid_debugff( (GID), __func__, SHORTFILE, __LINE__, FMT, ##__VA_ARGS__ )
# define XP_LOGFF( FMT, ... ) \
    linux_debugff( __func__, SHORTFILE, __LINE__, FMT, ##__VA_ARGS__ )
#define XP_LOG(STR) \
    linux_debugff( __func__, SHORTFILE, "%s", STR )
#define XP_RAW_LOGFF XP_LOGFF

#else
# define XP_DEBUGF(ch,...)
# define XP_LOGFF(fmt,...)
# define XP_LOG(fmt)
#define XP_RAW_LOGFF(...)
#endif  /* #ifdef DEBUG */

#define XP_WARNF XP_DEBUGF

#ifdef MEM_DEBUG

# define XP_PLATMALLOC(nbytes)       malloc(nbytes)
# define XP_PLATREALLOC(p,s)         realloc((p),(s))
# define XP_PLATFREE(p)              free(p)

#else

# define XP_MALLOC(pool,nbytes)       malloc(nbytes)
# define XP_CALLOC(pool,nbytes)       calloc(1,nbytes)
# define XP_REALLOC(pool,p,s)         realloc((p),(s))
# define XP_FREE(pool,p)              free(p)
void linux_freep( void** ptrp );
# define XP_FREEP(pool,p)             linux_freep((void**)p)
#endif

#define XP_MEMSET(src, val, nbytes)     memset( (src), (val), (nbytes) )
#define XP_MEMCPY(d,s,l) memcpy((d),(s),(l))
#define XP_MEMMOVE(d,s,l) memmove((d),(s),(l))
#define XP_MEMCMP( a1, a2, l )  memcmp((a1),(a2),(l))
#define XP_STRLEN(s) strlen(s)
#define XP_STRCAT(d,s) strcat((d),(s))
#define XP_STRNCMP(s1,s2,len) strncmp((s1),(s2),(len))
#define XP_STRNCPY(s1,s2,len) strncpy((s1),(s2),(len))
#define XP_STRCMP(s1,s2)       strcmp((s1),(s2))
void linux_lowerstr( XP_UCHAR* str );
#define XP_LOWERSTR(str)       linux_lowerstr(str)
#define XP_RANDOM() random()
#define XP_SNPRINTF snprintf

#define XP_MIN(a,b) ((a)<(b)?(a):(b))
#define XP_MAX(a,b) ((a)>(b)?(a):(b))
#define XP_ABS(a)   ((a)>=0?(a):-(a))

#ifdef DEBUG
# define XP_ASSERT(B) do { if (!(B)) { XP_LOGFF( "firing assert"); } assert(B); } while (0)
void linux_backtrace( void );
# define XP_BACKTRACE linux_backtrace
#else
# define XP_ASSERT(b)
# define XP_BACKTRACE
#endif

#define DGRAM_TYPE SOCK_DGRAM
/* #define DGRAM_TYPE SOCK_STREAM */

#define XP_NTOHL(l) ntohl(l)
#define XP_NTOHS(s) ntohs(s)
#define XP_HTONL(l) htonl(l)
#define XP_HTONS(s) htons(s)

#define XP_LD "%d"
#define XP_P "%p"

#endif


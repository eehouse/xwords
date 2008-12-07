/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */

/* Copyright 2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CEDEBUG_H_
#define _CEDEBUG_H_

#include "cemain.h"

const char* messageToStr( UINT message );
void logRect( const char* comment, const RECT* rect );

#ifdef DEBUG
void XP_LOGW( const XP_UCHAR* prefix, const wchar_t* arg );
void logLastError( const char* comment );
void messageToBuf( UINT message, char* buf, int bufSize );
#else
# define XP_LOGW( prefix, arg )
# define logLastError(c)
#endif

#ifdef DEBUG
# define assertOnTop( hWnd ) { \
        XP_ASSERT( (hWnd) == GetForegroundWindow() ); \
    }
#else
# define assertOnTop( w )
#endif

#endif  /* _CEDEBUG_H_ */

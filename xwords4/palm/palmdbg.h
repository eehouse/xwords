/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2006-2007 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _PALMDBG_H_
#define _PALMDBG_H_

#include "comtypes.h"
#include <Event.h>

const char* frmObjId_2str( XP_U16 id );
const char* eType_2str( eventsEnum eType );

/* Useful for writing pace_man functions. */
#define LOG_OFFSET( s, f ) \
    { s _s; \
    XP_LOGF( "offset of " #f " in " #s \
    ": %d (size: %ld)", OFFSET_OF( s, f ), \
                   sizeof(_s.f) ); \
    }

#endif

/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2008 by Eric House (xwords@eehouse.org).  All rights
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


#ifndef _LINUXUTL_H_
#define _LINUXUTL_H_

#include "xptypes.h"
#include "dictnry.h"
#include "util.h"

#ifdef DEBUG
void linux_debugf(const char*, ...)
    __attribute__ ((format (printf, 1, 2)));
#endif

DictionaryCtxt* linux_dictionary_make( MPFORMAL const char* dictFileName );

void linux_util_vt_init( MPFORMAL XW_UtilCtxt* util );
void linux_util_vt_destroy( XW_UtilCtxt* util );

const XP_UCHAR* linux_getErrString( UtilErrID id, XP_Bool* silent );

#endif

/* -*- compile-command: "make -k -j3"; -*- */

/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include "strwpf.h"

/* From stack overflow: snprintf with an expanding buffer.
 */

bool
StrWPF::catfap( const char* fmt, va_list ap )
{
    bool success = false;
    const int origsiz = size();
    resize( origsiz + m_addsiz );

    int len = vsnprintf( (char*)c_str() + origsiz, m_addsiz, fmt, ap );

    if ( len >= m_addsiz ) {   // needs more space
        m_addsiz = len + 1;
        resize( origsiz );
    } else if ( -1 == len ) {
        assert(0);          // should be impossible
    } else {
        resize( origsiz + len );
        m_addsiz = 100;
        success = true;
    }

    return success;
}

void 
StrWPF::catf( const char* fmt, ... )
{
    bool done;
    do {
        va_list ap;
        va_start( ap, fmt );
        done = catfap( fmt, ap );
        va_end( ap );
    } while ( !done );
}

/* -*- compile-command: "cd ../wasm && make main.html -j3"; -*- */
/*
 * Copyright 2021 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "wasmutls.h"

void 
wasm_debugf( const char* format, ... )
{
    va_list ap;
    va_start(ap, format);
    vfprintf( stderr, format, ap );
    va_end( ap );
    fprintf( stderr, "\n" );
}

void
wasm_debugff( const char* func, const char* file, const char* fmt, ...)
{
    char buf[1024];
    snprintf( buf, sizeof(buf), "%s:%s(): %s", file, func, fmt );
    va_list ap;
    va_start( ap, fmt );
    vfprintf( stderr, buf, ap );
    va_end( ap );
    fprintf( stderr, "\n" );
}

#ifndef MEM_DEBUG
void
wasm_freep( void** ptrp )
{
    if ( !!*ptrp ) {
        free( *ptrp );
        *ptrp = NULL;
    }
}
#endif

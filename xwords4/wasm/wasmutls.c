/* -*- compile-command: "cd ../wasm && make MEMDEBUG=TRUE install -j3"; -*- */
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

#include <emscripten.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "wasmutls.h"

EM_JS(void, console_log, (const char* msg), {
        let jsmsg = UTF8ToString(msg);
        console.log(jsmsg);
    });

void 
wasm_debugf( const char* format, ... )
{
    va_list ap;
    va_start(ap, format);
    char buf[1024];
    vsnprintf(buf, sizeof(buf)-1, format, ap);
    buf[sizeof(buf)-1] = '\0';  /* to be safe */
    va_end( ap );
    console_log(buf);
}

void
wasm_debugff( const char* func, const char* file, const char* fmt, ...)
{
    char buf[1024];
    snprintf( buf, sizeof(buf), "%s:%s(): %s", file, func, fmt );
    buf[sizeof(buf)-1] = '\0';  /* to be safe */

    va_list ap;
    va_start( ap, fmt );
    char buf2[1024];
    vsnprintf( buf2, sizeof(buf2), buf, ap );
    va_end( ap );
    buf2[sizeof(buf2)-1] = '\0';  /* to be safe */
    console_log( buf2 );
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

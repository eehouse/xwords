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

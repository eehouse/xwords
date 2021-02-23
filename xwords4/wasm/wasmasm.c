/* -*- compile-command: "cd ../wasm && make -j3 MEMDEBUG=TRUE install"; -*- */
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
#include <string.h>
#include <stdlib.h>

#include "wasmasm.h"
#include "comtypes.h"

EM_JS(void, _get_stored_value, (const char* key,
                                StringProc proc, void* closure), {
        var result = null;
        var jsKey = UTF8ToString(key);
        var val = localStorage.getItem(jsKey);
        ccallString(proc, closure, val);
    });

EM_JS(void, set_stored_value, (const char* key, const char* val), {
        var jsKey = UTF8ToString(key);
        var jsVal = UTF8ToString(val);
        var jsString = localStorage.setItem(jsKey, jsVal);
    });

EM_JS(void, remove_stored_value, (const char* key), {
        var jsKey = UTF8ToString(key);
        var jsString = localStorage.removeItem(jsKey);
    });

EM_JS(bool, have_stored_value, (const char* key), {
        let jsKey = UTF8ToString(key);
        let jsVal = localStorage.getItem(jsKey);
        let result = null !== jsVal;
        return result;
    });

typedef struct _ValState {
    void* ptr;
    size_t** len;
    bool success;
} ValState;

static void
onGotVal(void* closure, const char* val)
{
    if ( !!val ) {
        ValState* vs = (ValState*)closure;
        size_t slen = 1 + strlen(val);
        if ( !!vs->ptr && slen <= **vs->len ) {
            memcpy( vs->ptr, val, slen );
            vs->success = true;
        }
        **vs->len = slen;
    }
}

bool
get_stored_value( const char* key, char out[], size_t* len )
{
    ValState state = { .ptr = out, .len = &len, .success = false, };
    _get_stored_value( key, onGotVal, &state );
    return state.success;
}

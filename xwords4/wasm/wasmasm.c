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

#include "wasmasm.h"

EM_JS(const char*, get_stored_value, (const char* key), {
        var result = null;
        var jsKey = UTF8ToString(key);
        var jsString = localStorage.getItem(jsKey);
        if ( jsString != null ) {
            var lengthBytes = lengthBytesUTF8(jsString)+1;
            result = _malloc(lengthBytes);
            stringToUTF8(jsString, result, lengthBytes);
        }
        return result;
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

/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/*
 * Copyright 2013 - 2021 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _WASMDICT_H_
#define _WASMDICT_H_

#include "dictnry.h"
#include "main.h"

DictionaryCtxt* wasm_dictionary_make( Globals* globals, XWEnv xwe,
                                      const char* name, uint8_t* base, size_t len );
void formatDictIndx( char buf[], size_t len, const char* lang, const char* name );

#endif

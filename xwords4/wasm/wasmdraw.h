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

#ifndef _WASMDRAW_H_
#define  _WASMDRAW_H_
#include "draw.h"

DrawCtx* wasm_draw_make( MPFORMAL int width, int height );
void wasm_draw_resize( DrawCtx* dctx, int useWidth, int useHeight );
void wasm_draw_render( DrawCtx* dctx, SDL_Renderer* dest );
void wasm_draw_setInTrade( DrawCtx* dctx, bool inTrade );

#endif



#ifndef _WASMDRAW_H_
#define  _WASMDRAW_H_
#include "draw.h"

DrawCtx* wasm_draw_make( MPFORMAL int width, int height );
void wasm_draw_render( DrawCtx* dctx, SDL_Renderer* dest );

#endif


#ifndef _WASMUTIL_H_
#define _WASMUTIL_H_

#include "dutil.h"

XW_UtilCtxt* wasm_util_make( MPFORMAL CurGameInfo* gi, XW_DUtilCtxt* dutil,
                             void* closure );
void wasm_util_destroy( XW_UtilCtxt* util );

#endif

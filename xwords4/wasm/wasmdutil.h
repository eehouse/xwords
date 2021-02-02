
#ifndef _WASMDUTIL_H_
#define _WASMDUTIL_H_

#include <pthread.h>

#include "dutil.h"
#include "mempool.h"

XW_DUtilCtxt* wasm_dutil_make( MPFORMAL VTableMgr* vtMgr, void* closure );
void wasm_dutil_destroy( XW_DUtilCtxt* dutil );

#endif

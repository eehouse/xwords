
#ifndef _ANDGLOBALS_H_
#define _ANDGLOBALS_H_

#include "vtabmgr.h"
#include "dictnry.h"

typedef struct _AndGlobals {
    VTableMgr* vtMgr;
    CurGameInfo* gi;
    DrawCtx* dctx;
    XW_UtilCtxt* util;
    TransportProcs* xportProcs;
} AndGlobals;

#endif

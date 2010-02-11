
#ifndef _ANDGLOBALS_H_
#define _ANDGLOBALS_H_

#include "vtabmgr.h"
#include "dictnry.h"
#include "game.h"

typedef struct _AndGlobals {
    VTableMgr* vtMgr;
    CurGameInfo* gi;
    DrawCtx* dctx;
    XW_UtilCtxt* util;
    struct JNIUtilCtxt* jniutil;
    TransportProcs* xportProcs;
    struct JNIState* state;
} AndGlobals;

#endif

/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */

#ifndef _PNOSTATE_H_
#define _PNOSTATE_H_

#include <PceNativeCall.h>
#include <MemoryMgr.h>

/* This gets written into the code by the callback below. */
typedef struct PNOState {
    const void* emulStateP;
    Call68KFuncType* call68KFuncP;
    void* gotTable;
} PNOState;

/* I can't get the circular decls to work now */
typedef void StorageCallback(/*PnoletUserData*/void* dataP);

/* This is how armlet and 68K stub communicate on startup */
typedef struct PnoletUserData {
    unsigned long* pnoletEntry;
    unsigned long* gotTable;
    StorageCallback* storageCallback; /* armlet calls this */
    PNOState* stateSrc;           /* armlet fills in */
    PNOState* stateDest;          /* armlet fills in */

    /* PilotMain params */
    MemPtr cmdPBP;
    UInt16 cmd; 
    UInt16 launchFlags;
} PnoletUserData;

#endif

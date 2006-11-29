/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */

#ifndef _PNOSTATE_H_
#define _PNOSTATE_H_

#include <PceNativeCall.h>
#include <MemoryMgr.h>

/* from http://news.palmos.com/read/messages?id=159373 */
typedef struct EmulStateType {
    UInt32 instr;
    UInt32 regD[8];
    UInt32 regA[8];
    UInt32 regPC;
} EmulStateType;

/* This gets written into the code by the callback below. */
typedef struct PNOState {
    const EmulStateType* emulStateP;
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

    /* Other.... */
    Boolean recursive;          /* PilotMain called from inside PilotMain */
} PnoletUserData;

#endif

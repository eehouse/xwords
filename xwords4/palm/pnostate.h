/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */

#ifndef _PNOSTATE_H_
#define _PNOSTATE_H_

#include <PceNativeCall.h>

/* This gets written into the code by the callback below. */
typedef struct PNOState {
    const void* emulStateP;
    Call68KFuncType* call68KFuncP;
    void* gotTable;
} PNOState;

typedef struct PnoletUserData PnoletUserData;
typedef void StorageCallback(PnoletUserData* dataP);

/* This is how armlet and 68K stub communicate on startup */
struct PnoletUserData {
    unsigned long* pnoletEntry;
    unsigned long* gotTable;
    StorageCallback* storageCallback; /* armlet calls this */
    PNOState* stateSrc;           /* armlet fills in */
    PNOState* stateDest;          /* armlet fills in */
};

#endif

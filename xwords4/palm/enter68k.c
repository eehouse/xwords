/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */

#include <PalmTypes.h>
#include <SystemMgr.h>
#include <PceNativeCall.h>
#include <FeatureMgr.h>
#include <StringMgr.h>
#include <Form.h>
#include <DataMgr.h>
#include "pnostate.h"

#define FTR_NUM 1

static void
write_byte32( void* dest, UInt32 val )
{
    MemMove( dest, &val, sizeof(val) );
}

static UInt32
byte_swap32( UInt32 in )
{
    UInt32 tmp = 0L;
    tmp |= (in & 0x000000FF) << 24;
    tmp |= (in & 0x0000FF00) << 8;
    tmp |= (in & 0x00FF0000) >> 8;
    tmp |= (in & 0xFF000000) >> 24;
    return tmp;
}

static void
alertUser( char* str )
{
}

void
storageCallback( PnoletUserData* dataP )
{
    char buf[48];
    UInt32 offset;

    StrPrintF( buf, "storageCallback(%lx)", dataP );
    WinDrawChars( buf, StrLen(buf), 5, 35 );

    StrPrintF( buf, "src=%lx; dest=%lx", dataP->stateSrc, dataP->stateDest );
    WinDrawChars( buf, StrLen(buf), 5, 50 );

    offset = (char*)dataP->stateDest - (char*)dataP->pnoletEntry;
    DmWrite( dataP->pnoletEntry, offset, dataP->stateSrc, sizeof(PNOState) );
    WinDrawChars( "callback done", 13, 5, 65 );
}

static void
countOrLoadPNOCs( UInt32* pnoSizeP, UInt32* codePtr )
{
    DmResID id;
    UInt32 offset = 0;

    if ( !!pnoSizeP ) {
        *pnoSizeP = 0;
    }

    for ( id = 0; ; ++id ) {
        UInt32 size;
        MemHandle h = DmGetResource( 'PNOC', id );

        if ( !h ) {
            break;
        }
        size = MemHandleSize( h );
        if ( !!pnoSizeP ) {
            *pnoSizeP += size;
        }
        if ( !!codePtr ) {
            Err err = DmWrite( codePtr, offset, MemHandleLock(h), size );
            if ( err != errNone ) {
                alertUser( "error from DmWrite" );
            }
            offset += size;
            MemHandleUnlock(h);
        }
        DmReleaseResource(h);
    }
} /* countOrLoadPNOCs */

static UInt32*
setupPnoletIfFirstTime( UInt32** gotTableP )
{
    UInt32* pnoCode;
    Err err = FtrGet( APPID, FTR_NUM, (UInt32*)&pnoCode );
    if ( err == errNone ) {
        /* we're set to go, I guess */
    } else {
        UInt32* gotTable;
        UInt32 pnoSize, gotSize, pad;

        // LOAD: GOT table
        MemHandle h = DmGetResource( 'PNOG', 0 );
        if ( !h ) {
            gotSize = 0;
            gotTable = NULL;
        } else {
            gotSize = MemHandleSize( h );
            gotTable = (UInt32*)MemPtrNew( gotSize );
            MemMove( gotTable, MemHandleLock(h), gotSize );
            MemHandleUnlock( h );
            DmReleaseResource( h );
        }

        countOrLoadPNOCs( &pnoSize, NULL );
        pad = (4 - (pnoSize & 3)) & 3;
        FtrPtrNew( APPID, FTR_NUM, pnoSize + gotSize + pad, (void**)&pnoCode );

        countOrLoadPNOCs( NULL, pnoCode );

        if ( gotSize > 0 ) {
            UInt32 cnt = gotSize >> 2;
            UInt32 i;
            for ( i = 0; i < cnt; ++i ) {
                write_byte32(&gotTable[i],
                             byte_swap32(byte_swap32(gotTable[i]) 
                                         + (UInt32)pnoCode));
            }

            DmWrite( pnoCode, pnoSize + pad, gotTable, gotSize );
            MemPtrFree( gotTable );
            *gotTableP = (UInt32*)((char*)pnoCode) + pnoSize + pad;
        }
    }

    return pnoCode;
} /* setupPnoletIfFirstTime */

static Boolean
canRunPnolet()
{
    /* Need to check for the arm processor feature */
    return true;
} /* canRunPnolet */

UInt32
PilotMain( UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{

    if ( cmd == sysAppLaunchCmdNormalLaunch ) {
        if ( canRunPnolet() ) {
            UInt32* gotTable;
            PnoletUserData* dataP;
            UInt32* pnoCode = setupPnoletIfFirstTime( &gotTable );
            UInt32 result;

            dataP = (PnoletUserData*)MemPtrNew( sizeof(PnoletUserData) );
            dataP->pnoletEntry = pnoCode;
            dataP->gotTable = gotTable;
            dataP->storageCallback = storageCallback;

            result = PceNativeCall((NativeFuncType*)pnoCode, (void*)dataP );
            MemPtrFree( dataP );

        } else {
            /* warn user: can't run this app!!!! */
        }
    }

    return 0;
} /* PilotMain */

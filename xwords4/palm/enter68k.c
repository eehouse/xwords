/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2004 by Eric House (fixin@peak.org).  All rights reserved.
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

typedef struct PNOFtrHeader {
    UInt32* gotTable;
} PNOFtrHeader;

void
storageCallback( void/*PnoletUserData*/* _dataP )
{
    PnoletUserData* dataP = (PnoletUserData*)_dataP;
    char buf[48];
    UInt32 offset;
    PNOFtrHeader* ftrBase;

    StrPrintF( buf, "storageCallback(%lx)", dataP );
    WinDrawChars( buf, StrLen(buf), 5, 40 );

    StrPrintF( buf, "src=%lx; dest=%lx", dataP->stateSrc, dataP->stateDest );
    WinDrawChars( buf, StrLen(buf), 5, 50 );

    ftrBase = (PNOFtrHeader*)dataP->pnoletEntry;
    --ftrBase;                  /* back up over header */
    offset = (char*)dataP->stateDest - (char*)ftrBase;
    DmWrite( ftrBase, offset, dataP->stateSrc, sizeof(PNOState) );
    WinDrawChars( "callback done", 13, 5, 60 );
}

static void
countOrLoadPNOCs( UInt32* pnoSizeP, UInt8* base, UInt32 offset )
{
    DmResID id;

    for ( id = 0; ; ++id ) {
        UInt32 size;
        MemHandle h = DmGetResource( 'PNOC', id );

        if ( !h ) {
            break;
        }
        size = MemHandleSize( h );
        if ( !!base ) {
            Err err = DmWrite( base, offset, MemHandleLock(h), size );
            if ( err != errNone ) {
                alertUser( "error from DmWrite" );
            }
            MemHandleUnlock(h);
        }
        DmReleaseResource(h);
        offset += size;
    }

    if ( !!pnoSizeP ) {
        *pnoSizeP = offset;
    }
} /* countOrLoadPNOCs */

static void
setupPnolet( UInt32** entryP, UInt32** gotTableP )
{
    char buf[64];
    PNOFtrHeader* ftrBase;
    Err err = FtrGet( APPID, FTR_NUM, (UInt32*)&ftrBase );

    if ( err != errNone ) {
        UInt32* gotTable;
        UInt32 pnoSize, gotSize, pad;
        UInt32 ftrSize = sizeof( PNOFtrHeader );
        UInt32* pnoCode;
        PNOFtrHeader header;

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
        ftrSize += gotSize;

        countOrLoadPNOCs( &pnoSize, NULL, 0 );
        ftrSize += pnoSize;
        pad = (4 - (pnoSize & 3)) & 3;
        ftrSize += pad;

        FtrPtrNew( APPID, FTR_NUM, ftrSize, (void**)&ftrBase );
        pnoCode = (UInt32*)&ftrBase[1];

        StrPrintF( buf, "code ends at 0x%lx", 
                   ((char*)ftrBase) + ftrSize );
        WinDrawChars( buf, StrLen(buf), 5, 10 );

        countOrLoadPNOCs( NULL, (UInt8*)ftrBase, sizeof(PNOFtrHeader) );

        if ( gotSize > 0 ) {
            UInt32 cnt = gotSize >> 2;
            UInt32 i;
            for ( i = 0; i < cnt; ++i ) {
                write_byte32(&gotTable[i],
                             byte_swap32(byte_swap32(gotTable[i]) 
                                         + (UInt32)pnoCode));
            }

            DmWrite( ftrBase, sizeof(PNOFtrHeader) + pnoSize + pad, 
                     gotTable, gotSize );
            MemPtrFree( gotTable );

            header.gotTable = (UInt32*)(((char*)pnoCode) + pnoSize + pad);
            DmWrite( ftrBase, 0, &header, sizeof(header) );
        }
    }

    *gotTableP = ftrBase->gotTable;
    *entryP = (UInt32*)&ftrBase[1];

    StrPrintF( buf, "got at 0x%lx", *gotTableP );
    WinDrawChars( buf, StrLen(buf), 5, 20 );
} /* setupPnolet */

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
            char buf[64];
            UInt32* gotTable;
            PnoletUserData* dataP;
            UInt32* pnoCode;
            UInt32 result;

            setupPnolet( &pnoCode, &gotTable );

            dataP = (PnoletUserData*)MemPtrNew( sizeof(PnoletUserData) );
            dataP->pnoletEntry = pnoCode;
            dataP->gotTable = gotTable;
            dataP->storageCallback = storageCallback;

            dataP->cmdPBP = cmdPBP;
            dataP->cmd = cmd;
            dataP->launchFlags = launchFlags;

            StrPrintF( buf, "armlet starts at 0x%lx", pnoCode );
            WinDrawChars( buf, StrLen(buf), 5, 30 );

            result = PceNativeCall((NativeFuncType*)pnoCode, (void*)dataP );
            MemPtrFree( dataP );

            /* Might want to hang onto this, though it's a bit selfish.... */
            FtrPtrFree( APPID, FTR_NUM );
        } else {
            /* warn user: can't run this app!!!! */
        }
    }

    return 0;
} /* PilotMain */

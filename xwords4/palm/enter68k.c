/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2004 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include "palmmain.h"           /* for Ftr enum */

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
    (void)FrmCustomAlert( XW_ERROR_ALERT_ID,
                          str, " ", " " );
}

typedef struct PNOFtrHeader {
    UInt32* gotTable;
} PNOFtrHeader;

void
storageCallback( void/*PnoletUserData*/* _dataP )
{
    PnoletUserData* dataP = (PnoletUserData*)_dataP;
    UInt32 offset;
    PNOFtrHeader* ftrBase;

    if ( dataP->recursive ) {
        WinDrawChars( "ERROR: overwriting", 13, 5, 60 );
#ifdef DEBUG
        for ( ; ; );            /* make sure we see it. :-) */
#endif
    }

    ftrBase = (PNOFtrHeader*)dataP->pnoletEntry;
    --ftrBase;                  /* back up over header */
    offset = (char*)dataP->stateDest - (char*)ftrBase;
    DmWrite( ftrBase, offset, dataP->stateSrc, sizeof(PNOState) );
}

static void
countOrLoadPNOCs( UInt32* pnoSizeP, UInt8* base, UInt32 offset )
{
    DmResID id;

    for ( id = 0; ; ++id ) {
        UInt32 size;
        MemHandle h = DmGetResource( 'Pnoc', id );

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

/* Return true if we had to load the pnolet.  If we didn't, then we're being
 * called recursively (probably because of ExgMgr activity); in that case the
 * caller better not unload!
 */
static Boolean
setupPnolet( UInt32** entryP, UInt32** gotTableP )
{
    PNOFtrHeader* ftrBase;
    Err err = FtrGet( APPID, PNOLET_STORE_FEATURE, (UInt32*)&ftrBase );
    XP_Bool mustLoad = err != errNone;

    if ( mustLoad ) {
        UInt32* gotTable;
        UInt32 pnoSize, gotSize, pad;
        UInt32 ftrSize = sizeof( PNOFtrHeader );
        UInt32* pnoCode;
        PNOFtrHeader header;

        // LOAD: GOT table
        MemHandle h = DmGetResource( 'Pnog', 0 );
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

        FtrPtrNew( APPID, PNOLET_STORE_FEATURE, ftrSize, (void**)&ftrBase );
        pnoCode = (UInt32*)&ftrBase[1];

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

    return mustLoad;
} /* setupPnolet */

static Boolean
shouldRunPnolet()
{
    UInt32 value;
    Boolean runArm = false;
    Err err = FtrGet( sysFtrCreator, sysFtrNumProcessorID, &value );

    if ( ( err == errNone ) && sysFtrNumProcessorIsARM( value ) ) {
        runArm = true;
    }
#ifdef FEATURE_DUALCHOOSE
    if ( runArm ) {
        err = FtrGet( APPID, FEATURE_WANTS_68K, &value );
        if ( (err == errNone) && (value == WANTS_68K) ) {
            runArm = false;
        }            
    }
#endif
    return runArm;
} /* shouldRunPnolet */

UInt32
PilotMain( UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    UInt32 result = 0;
    if ( ( cmd == sysAppLaunchCmdNormalLaunch )
#ifdef XWFEATURE_IR
         || ( cmd == sysAppLaunchCmdExgAskUser )
         || ( cmd == sysAppLaunchCmdSyncNotify )
         || ( cmd == sysAppLaunchCmdExgReceiveData ) 
#endif
         ) {

        /* This is the entry point for both an ARM-only app and one capable
           of doing both.  If the latter, and ARM's an option, we want to use
           it unless the user's said not to. */

        if ( shouldRunPnolet() ) {
            UInt32* gotTable;
            PnoletUserData* dataP;
            UInt32* pnoCode;
            UInt32 result;
            Boolean loaded;

#ifdef DEBUG
            WinDrawChars( "Loading ARM code...", 19, 5, 25 );
#endif

            loaded = setupPnolet( &pnoCode, &gotTable );
            dataP = (PnoletUserData*)MemPtrNew( sizeof(PnoletUserData) );
            dataP->recursive = !loaded;

            dataP->pnoletEntry = pnoCode;
            dataP->gotTable = gotTable;
            dataP->storageCallback = storageCallback;

            dataP->cmdPBP = cmdPBP;
            dataP->cmd = cmd;
            dataP->launchFlags = launchFlags;

            result = PceNativeCall((NativeFuncType*)pnoCode, (void*)dataP );
            MemPtrFree( dataP );

            if ( loaded ) {
                FtrPtrFree( APPID, PNOLET_STORE_FEATURE );
            }
        } else {
#ifdef FEATURE_PNOAND68K
            result = PM2(PilotMain)( cmd, cmdPBP, launchFlags);
#else
            alertUser( "This copy of Crosswords runs only on ARM-based Palms. "
                       "Get the right version at xwords.sf.net." );
#endif
        }
    }

    return result;
} /* PilotMain */

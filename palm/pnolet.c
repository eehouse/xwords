/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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

#include <PceNativeCall.h>
#include "pnostate.h"
#include "pace_gen.h"
#include "pace_man.h"           /* for the ExgSocketType flippers */
#include "palmmain.h"

typedef union ParamStub {
    ExgAskParamType exgAskParamType;
    ExgSocketType   exgSocketType;
} ParamStub;

unsigned long
realArmletEntryPoint( const void *emulStateP, 
                      void *userData68KP, 
                      Call68KFuncType* call68KFuncP );

/* With arm-palmos-gcc, there can't be any .GOT references in the entry point
   (since those values get put inline BEFORE the function rather than
   after.) */
unsigned long
ArmletEntryPoint( const void *emulStateP, 
                  void *userData68KP, 
                  Call68KFuncType* call68KFuncP )
{
    return realArmletEntryPoint( emulStateP, 
                                 userData68KP, 
                                 call68KFuncP );
}

#ifdef XWFEATURE_IR
static XP_Bool
convertParamToArm( UInt16 cmd, ParamStub* armParam, MemPtr parm68K )
{
    XP_Bool revert;

    if ( cmd == sysAppLaunchCmdExgAskUser ) {
        /* We don't read the data, but we do write to one field.  */
        revert = XP_TRUE;
    } else if ( cmd == sysAppLaunchCmdExgReceiveData ) {
        flipEngSocketToArm( &armParam->exgSocketType, 
                            (const unsigned char*)parm68K );
        revert = XP_TRUE;       /* just to be safe */
    } else {
        revert = XP_FALSE;
    }

    return revert;
} /* convertParamToArm */

static void
convertParamFromArm( UInt16 cmd, MemPtr parm68K, ParamStub* armParam )
{
    if ( cmd == sysAppLaunchCmdExgAskUser ) {
        write_unaligned8( &((unsigned char*)parm68K)[4], 
                           armParam->exgAskParamType.result );
    } else if ( cmd == sysAppLaunchCmdExgReceiveData ) {
        flipEngSocketFromArm( parm68K, &armParam->exgSocketType );
    } else {
        XP_ASSERT(0);
    }
}
#endif

unsigned long
realArmletEntryPoint( const void *emulStateP, 
                      void *userData68KP, 
                      Call68KFuncType* call68KFuncP )
{
    PNOState* loc;
    PNOState state;
    PnoletUserData* dataP;
    unsigned long result;
    unsigned long oldR10;
    UInt16 cmd;
    MemPtr cmdPBP;
    MemPtr oldVal;
    ParamStub ptrStorage;
    XP_Bool mustRevert;

    loc = getStorageLoc();

    dataP = (PnoletUserData*)userData68KP;
    dataP->stateSrc = (PNOState*)Byte_Swap32((unsigned long)&state);
    dataP->stateDest = (PNOState*)Byte_Swap32((unsigned long)loc);

    state.gotTable = (unsigned long*)
        Byte_Swap32((unsigned long)dataP->gotTable);

    if ( !dataP->recursive ) {
        STACK_START(unsigned char, stack, 4 );
        ADD_TO_STACK4(stack, userData68KP, 0);
        STACK_END(stack);

        state.emulStateP = emulStateP;
        state.call68KFuncP = call68KFuncP;

        (*call68KFuncP)( emulStateP, 
                         Byte_Swap32((unsigned long)dataP->storageCallback),
                         stack, 4 );
    }

    asm( "mov %0, r10" : "=r" (oldR10) );
    asm( "mov r10, %0" : : "r" (state.gotTable) );

    cmd = Byte_Swap16( dataP->cmd );
    cmdPBP = (MemPtr)Byte_Swap32((unsigned long)dataP->cmdPBP);

#ifdef XWFEATURE_IR
    /* if the cmd is sysAppLaunchCmdExgAskUser or
       sysAppLaunchCmdExgReceiveData then we're going to be making use of the
       cmdPBP value in PilotMain.  Need to convert it here. */
    mustRevert = convertParamToArm( cmd, &ptrStorage, cmdPBP );
#endif

    oldVal = cmdPBP;
    cmdPBP = &ptrStorage;

    result = PM2(PilotMain)( cmd, cmdPBP, Byte_Swap16(dataP->launchFlags) );

#ifdef XWFEATURE_IR
    if ( mustRevert ) {
        convertParamFromArm( cmd, oldVal, &ptrStorage );
    }
#endif

    asm( "mov r10, %0" : : "r" (oldR10) );
    return result;
} /* realArmletEntryPoint( */

PNOState*
getStorageLoc( void )
{
    asm( "adr r0,data" );
    asm( "mov pc,lr" );
    asm( "data:" );
    /* we need sizeof(PNOState) worth of bytes after the data label. */
    asm( "nop" );
    asm( "nop" );
    return (PNOState*)0L;       /* shut up compiler; overwrite me!!!! */
    /* The compiler's adding a "mov pc,lr" here too that we can overwrite. */
}


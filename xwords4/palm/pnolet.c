/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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

#include <PceNativeCall.h>
#include "pnostate.h"
#include "pace_gen.h"
#include "palmmain.h"

unsigned long
ArmletEntryPoint( const void *emulStateP, 
                  void *userData68KP, 
                  Call68KFuncType* call68KFuncP )
{
    PNOState* loc;
    PNOState state;
    PnoletUserData* dataP;
    char* str;
    char buf[32];
    unsigned long result;
    unsigned long oldR10;
    unsigned long sp;

    loc = getStorageLoc();

    dataP = (PnoletUserData*)userData68KP;
    dataP->stateSrc = (PNOState*)Byte_Swap32((unsigned long)&state);
    dataP->stateDest = (PNOState*)Byte_Swap32((unsigned long)loc);

    state.emulStateP = emulStateP;
    state.call68KFuncP = call68KFuncP;
    state.gotTable = (unsigned long*)
        Byte_Swap32((unsigned long)dataP->gotTable);

    {
        STACK_START(unsigned char, stack, 4 );
        ADD_TO_STACK4(stack, userData68KP, 0);
        STACK_END(stack);
        (*call68KFuncP)( emulStateP, 
                         Byte_Swap32((unsigned long)dataP->storageCallback),
                         stack, 4 );
    }

    asm( "mov %0, r10" : "=r" (oldR10) );
    asm( "mov r10, %0" : : "r" (state.gotTable) );

    asm( "mov %0, r13" : "=r" (sp) );
    StrPrintF( buf, "Launching PilotMain;sp=%lx", sp );
    WinDrawChars( buf, StrLen(buf), 5, 100 );

    result = PM2(PilotMain)( Byte_Swap16(dataP->cmd), 
                             Byte_Swap32((unsigned long)dataP->cmdPBP), 
                             Byte_Swap16(dataP->launchFlags) );

    str = "back from PilotMain";
    WinDrawChars( str, StrLen(str), 5, 150 );

    asm( "mov r10, %0" : : "r" (oldR10) );
    return result;
}

PNOState*
getStorageLoc()
{
    asm( "adr r0,data" );
    asm( "mov pc,lr" );
    asm( "data:" );
    asm( "nop" );
    asm( "nop" );
    asm( "nop" );
    /* The compiler's adding a "mov pc,lr" here too that we can overwrite. */
}


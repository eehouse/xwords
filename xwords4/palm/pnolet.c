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

unsigned long
ArmletEntryPoint( const void *emulStateP, 
                  void *userData68KP, 
                  Call68KFuncType* call68KFuncP )
{
    PNOState* loc;
    PNOState state;
    PnoletUserData* dataP;
    char* str;
    unsigned long result;
    unsigned long oldR10;

    loc = getStorageLoc();

    dataP = (PnoletUserData*)userData68KP;
    dataP->stateSrc = (PNOState*)Byte_Swap32((unsigned long)&state);
    dataP->stateDest = (PNOState*)Byte_Swap32((unsigned long)loc);

    state.emulStateP = emulStateP;
    state.call68KFuncP = call68KFuncP;
    state.gotTable = (unsigned long*)
        Byte_Swap32((unsigned long)dataP->gotTable);

    {
        unsigned char stack[] = {
            ADD_TO_STACK4(userData68KP, 0)
        };
        (*call68KFuncP)( emulStateP, 
                         Byte_Swap32((unsigned long)dataP->storageCallback),
                         stack, sizeof(stack) );
    }

    asm( "mov %0, r10" : "=r" (oldR10) );
    asm( "mov r10, %0" : : "r" (state.gotTable) );

    str = "Launching PilotMain";
    WinDrawChars( str, StrLen(str), 5, 100 );

    result = PilotMain( Byte_Swap16(dataP->cmd), 
                        Byte_Swap32(dataP->cmdPBP), 
                        Byte_Swap16(dataP->launchFlags) );

    str = "back from PilotMain";
    WinDrawChars( str, StrLen(str), 5, 150 );

    asm( "mov r10, %0" : : "r" (oldR10) );
    return result;
}

PNOState*
getStorageLoc()
{
    PNOState* loc;
    asm( "mov %0,pc" : "=r" (loc) );
    asm( "add %0, %1, #8" : "=r" (loc) : "r" (loc) );
    asm( "bal done" );
    asm( "nop" );
    asm( "nop" );
    asm( "nop" );
    asm( "nop" );
    asm( "nop" );
    asm( "nop" );
    asm( "nop" );
    asm( "done:" );
    return loc;
}


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

#include <Event.h>
#include <Menu.h>
#include <DateTime.h>

#include "pace_man.h"

#include "palmmain.h"           /* for custom event types */
#include "palmsavg.h"

/* Looks like I still need this??? */
void*
memcpy( void* dest, const void* src, unsigned long n )
{
    void* oldDest = dest;
    unsigned char* d = (unsigned char*)dest;
    unsigned char* s = (unsigned char*)src;

    FUNC_HEADER(memcpy);

    if ( dest < src ) {
        while ( n-- > 0 ) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while ( n-- > 0 ) {
            *--d = *--s;
        }
    }

    FUNC_TAIL(memcpy);
    return oldDest;
} /* memcpy */

unsigned long
Byte_Swap32( unsigned long l )
{
    unsigned long result;
    result  = ((l >> 24) & 0xFF);
    result |= ((l >> 8) & 0xFF00);
    result |= ((l << 8) & 0xFF0000);
    result |= ((l << 24) & 0xFF000000);
    return result;
}

unsigned short
Byte_Swap16( unsigned short l )
{
    unsigned short result;
    result  = ((l >> 8) & 0xFF);
    result |= ((l << 8) & 0xFF00);
    return result;
}

void
write_unaligned32( unsigned char* dest, unsigned long val )
{
    int i;
    dest += sizeof(val);
    for ( i = 0; i < sizeof(val); ++i ) {
        *--dest = val & 0x000000FF;
        val >>= 8;
    }
} /* write_unaligned32 */

void
write_unaligned16( unsigned char* dest, unsigned short val )
{
    int i;

    dest += sizeof(val);
    for ( i = 0; i < sizeof(val); ++i ) {
        *--dest = val & 0x00FF;
        val >>= 8;
    }
} /* write_unaligned16 */

#define write_unaligned8( p, v ) *(p) = v

unsigned short 
read_unaligned16( unsigned char* src )
{
    int i;
    unsigned short val = 0;

    for ( i = 0; i < sizeof(val); ++i ) {
        val <<= 8;
        val |= *src++;
    }

    return val;
} /* read_unaligned16 */

#define read_unaligned8(cp) (*(cp))

unsigned long
read_unaligned32( unsigned char* src )
{
    int i;
    unsigned long val = 0;

    for ( i = 0; i < sizeof(val); ++i ) {
        val <<= 8;
        val |= *src++;
    }

    return val;
} /* read_unaligned32 */

/* Need to parse the format string */
Int16
StrPrintF( Char* s, const Char* formatStr, ... )
{
    unsigned long* inArgs = ((unsigned long*)&formatStr) + 1;

    return StrVPrintF( s, formatStr, (_Palm_va_list)inArgs );
} /* StrPrintF */

/* from file StringMgr.h */
Int16
StrVPrintF( Char* s, const Char* formatStr, _Palm_va_list arg )
{
    Int16 result;
    unsigned long* argv_arm = (unsigned long*)arg;
    unsigned char argv_68k[48];
    unsigned short done, isLong, innerDone, useArg;
    unsigned char* str = (unsigned char*)formatStr;
    unsigned short offset = 0;

    for ( done = 0; !done; ) {
        switch( *str++ ) {
        case '\0':
            done = 1; 
            break;
        case '%':
            isLong = useArg = 0;
            for( innerDone = 0; !innerDone; ) {
                switch( *str++ ) {                
                case '%':
                    innerDone = 1;
                    break;
                case 'l':
                    isLong = 1;
                    break;
                case 's':
                    isLong = 1;
                case 'd':
                case 'x':
                    innerDone = 1;
                    useArg = 1;
                    break;
                default:
                    crash();
                }
            }

            if ( useArg ) {
                unsigned long param;
                param = *argv_arm++;
                if ( isLong ) {
                    write_unaligned32( &argv_68k[offset], param );
                    offset += 4;
                } else {
                    write_unaligned16( &argv_68k[offset],
                                       (unsigned short)param );
                    offset += 2;
                }
            }
            break;
        }
    }

    /* now call the OS.... */
    {
        PNOState* sp = GET_CALLBACK_STATE();
        STACK_START(unsigned char, stack, 12);
        ADD_TO_STACK4(stack, s, 0);
        ADD_TO_STACK4(stack, formatStr, 4);
        ADD_TO_STACK4(stack, argv_68k, 8);
        STACK_END(stack);
        result = (Int16)
            (*sp->call68KFuncP)( sp->emulStateP, 
                                 // NOT sysTrapStrPrintF !!!!
                                 PceNativeTrapNo(sysTrapStrVPrintF),
                                 stack, 12 );
    }

    return result;
} /* StrVPrintF */

/* from file DateTime.h */
void
TimSecondsToDateTime( UInt32 seconds, DateTimeType* dateTimeP )
{
    FUNC_HEADER(TimSecondsToDateTime);
   {
       DateTimeType dateTime;
       PNOState* sp = GET_CALLBACK_STATE();
       STACK_START(unsigned char, stack, 8);
       ADD_TO_STACK4(stack, seconds, 0);
       ADD_TO_STACK4(stack, &dateTime, 4);
       STACK_END(stack);

       (*sp->call68KFuncP)( sp->emulStateP, 
                            PceNativeTrapNo(sysTrapTimSecondsToDateTime),
                            stack, 8 );

       dateTimeP->second = Byte_Swap16( dateTime.second );
       dateTimeP->minute = Byte_Swap16( dateTime.minute );
       dateTimeP->hour = Byte_Swap16( dateTime.hour );
       dateTimeP->day = Byte_Swap16( dateTime.day );
       dateTimeP->month = Byte_Swap16( dateTime.month );
       dateTimeP->year = Byte_Swap16( dateTime.year );
       dateTimeP->weekDay = Byte_Swap16( dateTime.weekDay );
   }
   FUNC_TAIL(TimSecondsToDateTime);
} /* TimSecondsToDateTime */

/* Events.  Need to translate back and forth since the ARM struct looks
 * different from the 68K version yet we have to be able to come up with a 68K
 * version for the OS at various times.  May also need to translate inside
 * event handlers since the OS will pass the 68K struct to us there.
 */
#define EVT_DATASIZE_68K 16 /* sez sizeof(event.data) in 68K code */
static void
evt68k2evtARM( unsigned char* evt68k, EventType* event )
{
    event->eType = read_unaligned16( evt68k );
    event->penDown = read_unaligned8( evt68k+2 );
    event->tapCount = read_unaligned8( evt68k+3 );
    event->screenX = read_unaligned16( evt68k+4 );
    event->screenY = read_unaligned16( evt68k+6 );

    evt68k += 8;                /* skip to start of data union */

    XP_LOGF( "evt68k2evtARM(%d)", event->eType );
    switch ( event->eType ) {
    case frmLoadEvent:
    case frmOpenEvent:
    case frmCloseEvent:
    case frmUpdateEvent:
        XP_ASSERT( &event->data.frmLoad.formID ==
                   &event->data.frmClose.formID );
        event->data.frmLoad.formID = read_unaligned16( evt68k );
        event->data.frmUpdate.updateCode = read_unaligned16( evt68k + 2 );
        break;
    case keyDownEvent:
        event->data.keyDown.chr = read_unaligned16( evt68k );
        event->data.keyDown.keyCode = read_unaligned16( evt68k+2 );
        event->data.keyDown.modifiers = read_unaligned16( evt68k+4 );
        break;

    case ctlSelectEvent:
        event->data.ctlSelect.controlID = read_unaligned16(evt68k);
        event->data.ctlSelect.pControl
            = (ControlType*)read_unaligned32(evt68k+2);
        event->data.ctlSelect.on = (Boolean)read_unaligned8(evt68k+6);
        event->data.ctlSelect.reserved1 = read_unaligned8(evt68k+7);
        event->data.ctlSelect.value = read_unaligned16(evt68k+8);
        break;

    case winExitEvent:
    case winEnterEvent:
        XP_ASSERT( &event->data.winEnter.enterWindow == 
                   &event->data.winExit.enterWindow );
        event->data.winEnter.enterWindow
            = (WinHandle)read_unaligned32( evt68k );
        event->data.winEnter.exitWindow
            = (WinHandle)read_unaligned32( evt68k+4 );
        break;

        // case penDownEvent: // stuff above is enough
        // case penMoveEvent: // stuff above is enough
        // case penUpEvent:   // stuff above is enough
    case menuEvent:
        event->data.menu.itemID = read_unaligned16( evt68k );
        break;
    case sclRepeatEvent:
        event->data.sclRepeat.scrollBarID = read_unaligned16( evt68k );
        event->data.sclRepeat.pScrollBar
            = (ScrollBarType*)read_unaligned32( evt68k+2 );
        event->data.sclRepeat.value = read_unaligned16( evt68k+6 );
        event->data.sclRepeat.newValue = read_unaligned16( evt68k+8 );
        event->data.sclRepeat.time = read_unaligned32( evt68k+10 );
        break;
        /* Crosswords events */
    case newGameCancelEvent:
        break;
    case openSavedGameEvent:
        ((OpenSavedGameData*)(&event->data.generic))->newGameIndex =
            read_unaligned16( evt68k );
            break;
    case newGameOkEvent:
    case loadGameEvent:
        break;
/*     case boardRedrawEvt: */
            // doResizeWinEvent
    case prefsChangedEvent:
            break;

    default:   /* copy the data as binary so we can copy it back later */
        memcpy( &event->data, evt68k, EVT_DATASIZE_68K );
        break;
    }
} /* evt68k2evtARM */

static void
evtArm2evt68K( unsigned char* evt68k, EventType* event )
{
    write_unaligned16( evt68k, event->eType );
    write_unaligned8( evt68k + 2, event->penDown );
    write_unaligned8( evt68k + 3, event->tapCount );
    write_unaligned16( evt68k + 4, event->screenX );
    write_unaligned16( evt68k + 6, event->screenY );

    evt68k += 8;

    XP_LOGF( "evtArm2evt68K(%d)", event->eType );
    switch ( event->eType ) {
    case frmLoadEvent:
    case frmOpenEvent:
    case frmCloseEvent:
    case frmUpdateEvent:
        XP_ASSERT( &event->data.frmLoad.formID
                   == &event->data.frmOpen.formID );
        XP_ASSERT( &event->data.frmLoad.formID
                   == &event->data.frmClose.formID );
        XP_ASSERT( &event->data.frmLoad.formID
                   == &event->data.frmUpdate.formID );
        write_unaligned16( evt68k, event->data.frmLoad.formID );
        write_unaligned16( evt68k + 2, event->data.frmUpdate.updateCode );
        XP_LOGF( "frmid=%d", event->data.frmLoad.formID );
        break;
    case keyDownEvent:
        write_unaligned16( evt68k, event->data.keyDown.chr );
        write_unaligned16( evt68k+2, event->data.keyDown.keyCode );
        write_unaligned16( evt68k+4, event->data.keyDown.modifiers );
        break;

    case ctlSelectEvent:
        write_unaligned16( evt68k, event->data.ctlSelect.controlID );
        write_unaligned32( evt68k+2, 
                           (unsigned long)event->data.ctlSelect.pControl );
        write_unaligned8( evt68k+6, event->data.ctlSelect.on );
        write_unaligned8( evt68k+7, event->data.ctlSelect.reserved1 );
        write_unaligned16( evt68k+8, event->data.ctlSelect.value );
        break;

    case winExitEvent:
    case winEnterEvent:
        XP_ASSERT( &event->data.winEnter.enterWindow == 
                   &event->data.winExit.enterWindow );
        write_unaligned32( evt68k, 
                           (unsigned long)event->data.winEnter.enterWindow );
        write_unaligned32( evt68k+4,
                           (unsigned long)event->data.winEnter.exitWindow );
        break;

        // case penDownEvent: // stuff above is enough
        // case penMoveEvent: // stuff above is enough
        // case penUpEvent:   // stuff above is enough
    case menuEvent:
        write_unaligned16( evt68k, event->data.menu.itemID );
        break;
    case sclRepeatEvent:
        write_unaligned16( evt68k, event->data.sclRepeat.scrollBarID );
        write_unaligned32( evt68k+2, 
                           (unsigned long)event->data.sclRepeat.pScrollBar );
        write_unaligned16( evt68k+6, event->data.sclRepeat.value );
        write_unaligned16( evt68k+8, event->data.sclRepeat.newValue );
        write_unaligned32( evt68k+10, event->data.sclRepeat.time );
        break;
        /* Crosswords events */
    case newGameCancelEvent:
        break;
    case openSavedGameEvent:
        write_unaligned16( evt68k,
                           ((OpenSavedGameData*)
                            (&event->data.generic))->newGameIndex );
        break;
    case newGameOkEvent:
    case loadGameEvent:
        break;
/*     case boardRedrawEvt: */
            // doResizeWinEvent
    case prefsChangedEvent:
            break;

    default:   /* copy the data as binary so we can copy it back later */
        memcpy( evt68k, &event->data, EVT_DATASIZE_68K );
        break;
    }

} /* evtArm2evt68K */

/* from file Event.h */
void
EvtGetEvent( EventType* event, Int32 timeout )
{
    FUNC_HEADER(EvtGetEvent);
    {
        EventType evt68k;
        PNOState* sp = GET_CALLBACK_STATE();
        STACK_START(unsigned char, stack, 8);
        ADD_TO_STACK4(stack, &evt68k, 0);
        ADD_TO_STACK4(stack, timeout, 4);
        STACK_END(stack);
        (*sp->call68KFuncP)( sp->emulStateP, 
                             PceNativeTrapNo(sysTrapEvtGetEvent),
                             stack, 8 );

        evt68k2evtARM( (unsigned char*)&evt68k, event );
    }
    FUNC_TAIL(EvtGetEvent);
} /* EvtGetEvent */

/* from file SystemMgr.h */
Boolean
SysHandleEvent( EventPtr eventP )
{
    Boolean result;
    EventType event68K;
    FUNC_HEADER(SysHandleEvent);

    evtArm2evt68K( (unsigned char*)&event68K, eventP );

    {
        PNOState* sp = GET_CALLBACK_STATE();
        STACK_START(unsigned char, stack, 4);
        ADD_TO_STACK4(stack, &event68K, 0);
        STACK_END(stack);
        result = (Boolean)(*sp->call68KFuncP)( sp->emulStateP, 
                                               PceNativeTrapNo(sysTrapSysHandleEvent),
                                               stack, 4 );
    }
    FUNC_TAIL(SysHandleEvent);
    return result;
} /* SysHandleEvent */

/* from file Form.h */
Boolean
FrmDispatchEvent( EventType* eventP )
{
    Boolean result;
    EventType event68k;
    FUNC_HEADER(FrmDispatchEvent);
    XP_LOGF("in FrmDispatchEvent" );
    evtArm2evt68K( (unsigned char*)&event68k, eventP );
    {
        PNOState* sp = GET_CALLBACK_STATE();
        STACK_START(unsigned char, stack, 4);
        ADD_TO_STACK4(stack, &event68k, 0);
        STACK_END(stack);
        result = (Boolean)(*sp->call68KFuncP)( sp->emulStateP, 
                                               PceNativeTrapNo(sysTrapFrmDispatchEvent),
                                               stack, 4 );
    }
    XP_LOGF("FrmDispatchEvent: back from PACE" );
    FUNC_TAIL(FrmDispatchEvent);
    return result;
} /* FrmDispatchEvent */

/* from file Menu.h */
Boolean
MenuHandleEvent( MenuBarType* menuP, EventType* event, UInt16* error )
{
    Boolean result;
    EventType event68k;
    FUNC_HEADER(MenuHandleEvent);

    evtArm2evt68K( (unsigned char*)&event68k, event );
    SWAP2_NON_NULL_IN(error);
    {
        PNOState* sp = GET_CALLBACK_STATE();
        STACK_START(unsigned char, stack, 12);
        ADD_TO_STACK4(stack, menuP, 0);
        ADD_TO_STACK4(stack, &event68k, 4);
        ADD_TO_STACK4(stack, error, 8);
        STACK_END(stack);
        result = (Boolean)(*sp->call68KFuncP)( 
                 sp->emulStateP, 
                 PceNativeTrapNo(sysTrapMenuHandleEvent),
                 stack, 12 );
        SWAP2_NON_NULL_OUT(error);
    }
    FUNC_TAIL(MenuHandleEvent);
    return result;
} /* MenuHandleEvent */

unsigned long
handlerEntryPoint( const void* emulStateP, 
                   void* userData68KP, 
                   Call68KFuncType* call68KFuncP )
{
    unsigned long* data = (unsigned long*)userData68KP;
    FormEventHandlerType* handler
        = (FormEventHandlerType*)read_unaligned32( (unsigned long*)&data[0] );
    PNOState* state = getStorageLoc();
    unsigned long oldR10;
    EventType evtArm;
    Boolean result;

    /* set up stack here too? */
    asm( "mov %0, r10" : "=r" (oldR10) );
    asm( "mov r10, %0" : : "r" (state->gotTable) );

    XP_LOGF( "handlerEntryPoint" );
    evt68k2evtARM( (unsigned char*)read_unaligned32(&data[1]), 
                   &evtArm );

    result = (*handler)(&evtArm);

    asm( "mov r10, %0" : : "r" (oldR10) );

    return (unsigned long)result;
}

/* The stub wants to look like this:
   static Boolean
   FormEventHandlerType( EventType *eventP )
   {
       unsigned long data[] = { armEvtHandler, eventP };
       return (Boolean)PceNativeCall( handlerEntryPoint, (void*)data );
   }
 */
static unsigned char*
makeHandlerStub( FormEventHandlerType* handlerArm )
{
    unsigned char* stub;
    unsigned char code_68k[] = {
        /* 0:*/	0x4e, 0x56, 0xff, 0xf8,             // linkw %fp,#-8
        /* 4:*/	0x20, 0x2e, 0x00, 0x08,         	// movel %fp@(8),%d0
        /* 8:*/	0x2d, 0x7c, 0x11, 0x22, 0x33, 0x44, // movel #287454020,%fp@(-8)
        /*14:*/ 0xff, 0xf8,                         // ????? REQUIRED!!!!
        /*16:*/	0x2d, 0x40, 0xff, 0xfc,      	    // movel %d0,%fp@(-4)
        /*20:*/	0x48, 0x6e, 0xff, 0xf8,      	    // pea %fp@(-8)
        /*24:*/	0x2f, 0x3c, 0x55, 0x66, 0x77, 0x88, // movel #1432778632,%sp@-
        /*30:*/	0x4e, 0x4f,           	            // trap #15
        /*32:*/	0xa4, 0x5a,                         // 0122132
        /*34:*/	0x02, 0x40, 0x00, 0xff,      	    // andiw #255,%d0
        /*38:*/	0x4e, 0x5e,           	            // unlk %fp
        /*40:*/	0x4e, 0x75                          // rts
    };

    stub = MemPtrNew( sizeof(code_68k) );
    memcpy( stub, code_68k, sizeof(code_68k) );

    write_unaligned32( &stub[10], 
                        /* replace 0x11223344 */
                       (unsigned long)handlerArm );
    write_unaligned32( &stub[26], 
                       /* replace 0x55667788 */
                       (unsigned long)handlerEntryPoint );
    /* Need to register this stub so it can be freed (once leaking ceases to
       be ok on PalmOS) */
    
    return (unsigned char*)stub;
} /* makeHandlerStub */

void
FrmSetEventHandler( FormType* formP, FormEventHandlerType* handler )
{
    FUNC_HEADER(FrmSetEventHandler);
    XP_LOGF( "FrmSetEventHandler called" );
    {
        PNOState* sp = GET_CALLBACK_STATE();
        unsigned char* handlerStub = makeHandlerStub( handler );
        STACK_START(unsigned char, stack, 8);
        ADD_TO_STACK4(stack, formP, 0);
        ADD_TO_STACK4(stack, handlerStub, 4);
        STACK_END(stack);
        (*sp->call68KFuncP)( sp->emulStateP, 
                             PceNativeTrapNo(sysTrapFrmSetEventHandler),
                             stack, 8 );
    }
    XP_LOGF( "FrmSetEventHandler done" );
    FUNC_TAIL(FrmSetEventHandler);
} /* FrmSetEventHandler */

/* from file Event.h */
void
EvtAddEventToQueue( const EventType* event )
{
    FUNC_HEADER(EvtAddEventToQueue);

    EventType evt68k;
    evtArm2evt68K( (unsigned char*)&evt68k, event );

    {
        PNOState* sp = GET_CALLBACK_STATE();
        STACK_START(unsigned char, stack, 4);
        ADD_TO_STACK4(stack, &evt68k, 0);
        STACK_END(stack);
        (*sp->call68KFuncP)( sp->emulStateP, 
                             PceNativeTrapNo(sysTrapEvtAddEventToQueue),
                             stack, 4 );
    }
    FUNC_TAIL(EvtAddEventToQueue);
    EMIT_NAME("'E','v','t','A','d','d','E','v','e','n','t','T','o','Q','u','e','u','e'");
} /* EvtAddEventToQueue */

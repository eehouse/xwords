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
#include <VFSMgr.h>

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
read_unaligned16( const unsigned char* src )
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
read_unaligned32( const unsigned char* src )
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
evt68k2evtARM( EventType* event, const unsigned char* evt68k )
{
    event->eType = read_unaligned16( evt68k );
    event->penDown = read_unaligned8( evt68k+2 );
    event->tapCount = read_unaligned8( evt68k+3 );
    event->screenX = read_unaligned16( evt68k+4 );
    event->screenY = read_unaligned16( evt68k+6 );

    evt68k += 8;                /* skip to start of data union */

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
evtArm2evt68K( unsigned char* evt68k, const EventType* event )
{
    write_unaligned16( evt68k, event->eType );
    write_unaligned8( evt68k + 2, event->penDown );
    write_unaligned8( evt68k + 3, event->tapCount );
    write_unaligned16( evt68k + 4, event->screenX );
    write_unaligned16( evt68k + 6, event->screenY );

    evt68k += 8;

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

        evt68k2evtARM( event, (unsigned char*)&evt68k );
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
        = (FormEventHandlerType*)read_unaligned32( (unsigned char*)&data[0] );
    PNOState* state = getStorageLoc();
    unsigned long oldR10;
    EventType evtArm;
    Boolean result;

    /* set up stack here too? */
    asm( "mov %0, r10" : "=r" (oldR10) );
    asm( "mov r10, %0" : : "r" (state->gotTable) );

    evt68k2evtARM( &evtArm,
                   (unsigned char*)read_unaligned32( (unsigned char*)&data[1]) );

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
} /* EvtAddEventToQueue */

void
flipRect( RectangleType* rout, const RectangleType* rin )
{
    rout->topLeft.x = Byte_Swap16(rin->topLeft.x);
    rout->topLeft.y = Byte_Swap16(rin->topLeft.y);
    rout->extent.x = Byte_Swap16(rin->extent.x);
    rout->extent.y = Byte_Swap16(rin->extent.y);
} /* flipRect */

void
flipFieldAttr( FieldAttrType* fout, const FieldAttrType* fin )
{
    /* It's a bleeding bitfield */
} /* flipFieldAttr */

void
flipEngSocketFromArm( unsigned char* sout, const ExgSocketType* sin )
{
	write_unaligned16( &sout[0],  Byte_Swap16(sin->libraryRef) ); // UInt16 libraryRef;
	write_unaligned32( &sout[2],  Byte_Swap32(sin->socketRef) );  // UInt32 	socketRef;
	write_unaligned32( &sout[6],  Byte_Swap32(sin->target) );     // UInt32 	target;
	write_unaligned32( &sout[10], Byte_Swap32(sin->count) ); // UInt32	count;
    write_unaligned32( &sout[14], Byte_Swap32(sin->length) );// UInt32	length;
	write_unaligned32( &sout[18], Byte_Swap32(sin->time) );// UInt32	time;
    write_unaligned32( &sout[22], Byte_Swap32(sin->appData) );	// UInt32	appData;
    write_unaligned32( &sout[26], Byte_Swap32(sin->goToCreator) );	// UInt32 	goToCreator;
	write_unaligned16( &sout[30], Byte_Swap16(sin->goToParams.dbCardNo) );	// UInt16	goToParams.dbCardNo;
	write_unaligned32( &sout[32], Byte_Swap32(sin->goToParams.dbID) );	// LocalID	goToParams.dbID;
	write_unaligned16( &sout[36], Byte_Swap16(sin->goToParams.recordNum) );	// UInt16 	goToParams.recordNum;
	write_unaligned32( &sout[38], Byte_Swap32(sin->goToParams.uniqueID) );	// UInt32	goToParams.uniqueID;
	write_unaligned32( &sout[42], Byte_Swap32(sin->goToParams.matchCustom) );	// UInt32	goToParams.matchCustom;
    /* bitfield.  All we can do is copy the whole thing, assuming it's 16
       bits, and pray that no arm code wants to to use it. */
	write_unaligned16( &sout[46], Byte_Swap16(*(UInt16*)((unsigned char*)&sin->goToParams.matchCustom) 
                       + sizeof(sin->goToParams.matchCustom)) );
	write_unaligned32( &sout[48], Byte_Swap32((unsigned long)sin->description) );	// Char *description;
	write_unaligned32( &sout[52], Byte_Swap32((unsigned long)sin->type) );	// Char *type;
	write_unaligned32( &sout[56], Byte_Swap16((unsigned long)sin->name) );	// Char *name;
} /* flipEngSocketFromArm */

void
flipEngSocketToArm( ExgSocketType* sout, const unsigned char* sin )
{
    sout->libraryRef = Byte_Swap16(read_unaligned16( &sin[0] ));
	sout->socketRef = Byte_Swap32(read_unaligned32( &sout[2] ) );
	sout->target = Byte_Swap32(read_unaligned32( &sout[6] ) );
	sout->count = Byte_Swap32(read_unaligned32( &sout[10] ) );
    sout->length = Byte_Swap32(read_unaligned32( &sout[14] ) );
	sout->time = Byte_Swap32(read_unaligned32( &sout[18] ) );
    sout->appData = Byte_Swap32(read_unaligned32( &sout[22] )  );
    sout->goToCreator = Byte_Swap32(read_unaligned32( &sout[26] ) );
	sout->goToParams.dbCardNo = Byte_Swap16( read_unaligned16( &sout[30] ) );
	sout->goToParams.dbID = Byte_Swap32( read_unaligned32( &sout[32] ) );
	sout->goToParams.recordNum = Byte_Swap16(read_unaligned16( &sout[36] ) );
	sout->goToParams.uniqueID = Byte_Swap32(read_unaligned32( &sout[38]) );
	sout->goToParams.matchCustom = Byte_Swap32( read_unaligned32( &sout[42] ) );
    /* bitfield.  All we can do is copy the whole thing, assuming it's 16
       bits, and pray that no arm code wants to to use it. */
    *(UInt16*)(((unsigned char*)&sout->goToParams.matchCustom) 
               + sizeof(sout->goToParams.matchCustom)) = Byte_Swap16(read_unaligned16( &sout[46] ));
	sout->description = Byte_Swap32( read_unaligned32( &sout[48] ) );
	sout->type = Byte_Swap32(read_unaligned32( &sout[52]) );
	sout->name = Byte_Swap32(read_unaligned32( &sout[56]) );
} /* flipEngSocketToArm */

void
flipFileInfoFromArm( unsigned char* fiout, const FileInfoType* fiin )
{
    write_unaligned32( &fiout[0], fiin->attributes );
    write_unaligned32( &fiout[4], (unsigned long)fiin->nameP );
    write_unaligned16( &fiout[8], fiin->nameBufLen );
}

void
flipFileInfoToArm( FileInfoType* fout, const unsigned char* fin )
{
    fout->attributes = read_unaligned32( &fin[0] );
    fout->nameP = read_unaligned32( &fin[4] );
    fout->nameBufLen = read_unaligned16( &fin[8] );
} /* flipFileInfo */

/* from file List.h */
void
LstSetListChoices( ListType* listP, Char** itemsText, Int16 numItems )
{
    FUNC_HEADER(LstSetListChoices);
    /* var decls */
    /* swapIns */
    {
        XP_U16 i;
        PNOState* sp = GET_CALLBACK_STATE();
        STACK_START(unsigned char, stack, 10);
        /* pushes */
        ADD_TO_STACK4(stack, listP, 0);
        ADD_TO_STACK4(stack, itemsText, 4);
        ADD_TO_STACK2(stack, numItems, 8);
        STACK_END(stack);

        for ( i = 0; i < numItems; ++i ) {
            itemsText[i] = Byte_Swap32( itemsText[i] );
        }

        (*sp->call68KFuncP)( sp->emulStateP, 
                             PceNativeTrapNo(sysTrapLstSetListChoices),
                             stack, 10 );
        /* swapOuts */
    }
    FUNC_TAIL(LstSetListChoices);
    EMIT_NAME("LstSetListChoices","'L','s','t','S','e','t','L','i','s','t','C','h','o','i','c','e','s'");
} /* LstSetListChoices */

static void
params68KtoParamsArm( SysNotifyParamType* paramsArm,
                      const unsigned char* params68K )
{
    paramsArm->notifyType = read_unaligned32( &params68K[0] );
    paramsArm->broadcaster = read_unaligned32( &params68K[4] );
    paramsArm->notifyDetailsP = (void*)read_unaligned32( &params68K[8] );
    paramsArm->userDataP = (void*)read_unaligned32( &params68K[12] );
    paramsArm->handled = read_unaligned8( &params68K[16] );

    /* I don't do anything with the data passed in, so no need to swap it...
       But that'd change for others: make an ARM-corrected copy of the
       contents of notifyDetailsP if your handler will use it. */
    switch( paramsArm->notifyType ) {
    case sysNotifyVolumeUnmountedEvent:
    case sysNotifyVolumeMountedEvent:
        break;
#ifdef FEATURE_SILK
    case sysNotifyDisplayChangeEvent:
        break;
#endif
    }

} /* params68KtoParamsArm */

static void
paramsArmtoParams68K( unsigned char* params68K, 
                      const SysNotifyParamType* armParams )
{
    write_unaligned8( &params68K[16], armParams->handled );
} /* paramsArmtoParams68K */

unsigned long
notifyEntryPoint( const void* emulStateP, 
                  void* userData68KP, 
                  Call68KFuncType* call68KFuncP )
{
    unsigned long* data = (unsigned long*)userData68KP;
    SysNotifyProcPtr callback
        = (SysNotifyProcPtr)read_unaligned32( (unsigned long*)&data[0] );
    SysNotifyParamType armParams;
    PNOState* state = getStorageLoc();
    unsigned long oldR10;
    unsigned char* params68K;
    Err result;

    /* set up stack here too? */
    asm( "mov %0, r10" : "=r" (oldR10) );
    asm( "mov r10, %0" : : "r" (state->gotTable) );

    XP_ASSERT( emulStateP == state->emulStateP );
    XP_ASSERT( call68KFuncP == state->call68KFuncP );

    params68K = (unsigned char*)read_unaligned32(&data[1]);
    params68KtoParamsArm( &armParams, params68K );

    result = (*callback)(&armParams);

    /* at least need to write 'handled' back out... */
    paramsArmtoParams68K( params68K, &armParams );

    asm( "mov r10, %0" : : "r" (oldR10) );

    return (unsigned long)result;
} /* notifyEntryPoint */

/* The stub wants to look like this:
   static Err SysNotifyProc(SysNotifyParamType *notifyParamsP) 
   {
       unsigned long data[] = { armNotifyHandler, notifyParamsP };
       return (Err)PceNativeCall( handlerEntryPoint, (void*)data );
   }
 */
static unsigned char*
makeNotifyStub( SysNotifyProcPtr callback )
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
        /*34:*/	0x4e, 0x5e,           	            // unlk %fp
        /*36:*/	0x4e, 0x75                          // rts
    };

    stub = MemPtrNew( sizeof(code_68k) );
    memcpy( stub, code_68k, sizeof(code_68k) );

    write_unaligned32( &stub[10], 
                        /* replace 0x11223344 */
                       (unsigned long)callback );
    write_unaligned32( &stub[26], 
                       /* replace 0x55667788 */
                       (unsigned long)notifyEntryPoint );
    /* Need to register this stub so it can be freed (once leaking ceases to
       be ok on PalmOS) */
    
    return (unsigned char*)stub;
} /* makeNotifyStub */

/* from file NotifyMgr.h */
Err
SysNotifyRegister( UInt16 cardNo, LocalID dbID, UInt32 notifyType, 
                   SysNotifyProcPtr callbackP, Int8 priority, void* userDataP )
{
    Err result;
    FUNC_HEADER(SysNotifyRegister);
    /* var decls */
    /* swapIns */
    {
        PNOState* sp = GET_CALLBACK_STATE();
        unsigned char* handlerStub = makeNotifyStub( callbackP );
        STACK_START(unsigned char, stack, 20);
        /* pushes */
        ADD_TO_STACK2(stack, cardNo, 0);
        ADD_TO_STACK4(stack, dbID, 2);
        ADD_TO_STACK4(stack, notifyType, 6);
        ADD_TO_STACK4(stack, handlerStub, 10);
        ADD_TO_STACK1(stack, priority, 14);
        ADD_TO_STACK4(stack, userDataP, 16);
        STACK_END(stack);
        result = (Err)(*sp->call68KFuncP)( sp->emulStateP, 
                                           PceNativeTrapNo(sysTrapSysNotifyRegister),
                                           stack, 20 );
        /* swapOuts */
    }
    FUNC_TAIL(SysNotifyRegister);
    EMIT_NAME("SysNotifyRegister","'S','y','s','N','o','t','i','f','y','R','e','g','i','s','t','e','r'");
    return result;
} /* SysNotifyRegister */

unsigned long
listDrawEntryPoint( const void* emulStateP, 
                    void* userData68KP, 
                    Call68KFuncType* call68KFuncP )
{
    unsigned long* data = (unsigned long*)userData68KP;
    ListDrawDataFuncPtr listDrawProc
        = (ListDrawDataFuncPtr)read_unaligned32( (unsigned long*)&data[0] );
    PNOState* state = getStorageLoc();
    unsigned long oldR10;
    Int16 index;
    RectanglePtr bounds;
    char** itemsText;

    /* set up stack here too? */
    asm( "mov %0, r10" : "=r" (oldR10) );
    asm( "mov r10, %0" : : "r" (state->gotTable) );

    XP_ASSERT( emulStateP == state->emulStateP );
    XP_ASSERT( call68KFuncP == state->call68KFuncP );

    index = (Int16)read_unaligned32( &data[1] );
    bounds = (RectanglePtr)read_unaligned32( &data[2] );
    itemsText = (char**)read_unaligned32( &data[3] );
    (*listDrawProc)( index, bounds, itemsText );

    asm( "mov r10, %0" : : "r" (oldR10) );

    return 0L;                  /* no result to return */
} /* listDrawEntryPoint */

static unsigned char*
makeListDrawStub( ListDrawDataFuncPtr func )
{
/* called function looks like this:
   void listDrawFunc(Int16 index, RectanglePtr bounds, char** itemsText)
   {
       unsigned long data[] = { func, index, 
                                bounds, itemsText };
       return (Err)PceNativeCall( listDrawEntryPoint, (void*)data );
   }
 */
    unsigned char* stub;
    unsigned char code_68k[] = {
        /* 0:*/	0x4e, 0x56, 0xff, 0xf0,      	// linkw %fp,#-16
        /* 4:*/	0x30, 0x2e, 0x00, 0x08,      	// movew %fp@(8),%d0
        /* 8:*/	0x22, 0x2e, 0x00, 0x0a,      	// movel %fp@(10),%d1
        /* c:*/	0x24, 0x2e, 0x00, 0x0e,      	// movel %fp@(14),%d2
        /*10:*/	0x2d, 0x7c, 0x11, 0x22,0x33,0x44,// movel #287454020,%fp@(-16)
        /*16:*/	0xff, 0xf0,
        /*18:*/	0x30, 0x40,           	// moveaw %d0,%a0
        /*1a:*/	0x2d, 0x48, 0xff, 0xf4,      	// movel %a0,%fp@(-12)
        /*1e:*/	0x2d, 0x41, 0xff, 0xf8,      	// movel %d1,%fp@(-8)
        /*22:*/	0x2d, 0x42, 0xff, 0xfc,      // movel %d2,%fp@(-4)
        /*26:*/	0x48, 0x6e, 0xff, 0xf0,      	// pea %fp@(-16)
        /*2a:*/	0x2f, 0x3c, 0x55, 0x66, 0x77, 0x88,	// movel #1432778632,%sp@-
        /*30:*/	0x4e, 0x4f,           	// trap #15
        /*32:*/	0xa4, 0x5a,           	// 0122132
        /*34:*/	0x4e, 0x5e,           	// unlk %fp
        /*36:*/	0x4e, 0x75           	// rts
    };
    stub = MemPtrNew( sizeof(code_68k) );
    memcpy( stub, code_68k, sizeof(code_68k) );

    write_unaligned32( &stub[0x12], 
                        /* replace 0x11223344 */
                       (unsigned long)func );
    write_unaligned32( &stub[0x2c], 
                       /* replace 0x55667788 */
                       (unsigned long)listDrawEntryPoint );

    return (unsigned char*)stub;
} /* makeListDrawStub */

/* from file List.h */
void
LstSetDrawFunction( ListType* listP, ListDrawDataFuncPtr func )
{
    FUNC_HEADER(LstSetDrawFunction);
    /* var decls */
    /* swapIns */
    {
        PNOState* sp = GET_CALLBACK_STATE();
        unsigned char* stub = makeListDrawStub( func );
        STACK_START(unsigned char, stack, 8);
        /* pushes */
        ADD_TO_STACK4(stack, listP, 0);
        ADD_TO_STACK4(stack, stub, 4);
        STACK_END(stack);
        (*sp->call68KFuncP)( sp->emulStateP, 
                             PceNativeTrapNo(sysTrapLstSetDrawFunction),
                             stack, 8 );
        /* swapOuts */
    }
    FUNC_TAIL(LstSetDrawFunction);
    EMIT_NAME("LstSetDrawFunction","'L','s','t','S','e','t','D','r','a','w','F','u','n','c','t','i','o','n'");
} /* LstSetDrawFunction */

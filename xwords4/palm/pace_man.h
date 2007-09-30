/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2004-2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _PACE_MAN_H_
#define _PACE_MAN_H_

#ifdef XW_TARGET_PNO

#include <Form.h>
#include <Menu.h>
#include <ExgMgr.h>
#include <VFSMgr.h>
#include <NotifyMgr.h>
#include <NetMgr.h>
#ifdef XWFEATURE_BLUETOOTH
# include <BtLib.h>
# include <BtLibTypes.h>
#endif

#include "pnostate.h"
#include "xptypes.h"

void* memcpy( void* dest, const void* src, unsigned long n );
extern Int16 StrPrintF( Char* s, const Char* formatStr, ... );
extern Int16 StrVPrintF( Char* s, const Char* formatStr, _Palm_va_list arg );
extern Boolean SysHandleEvent( EventPtr eventP );
extern void FrmSetEventHandler( FormType* formP, 
                                FormEventHandlerType* handler );
extern void LstSetListChoices( ListType* listP, Char** itemsText, 
                               Int16 numItems );
extern Err SysNotifyRegister( UInt16 cardNo, LocalID dbID, 
                              UInt32 notifyType, SysNotifyProcPtr callbackP, 
                              Int8 priority, void* userDataP );
extern void LstSetDrawFunction( ListType* listP, ListDrawDataFuncPtr func );
extern Err ExgDBWrite( ExgDBWriteProcPtr writeProcP, void* userDataP, 
                       const char* nameP, LocalID dbID, UInt16 cardNo );

#if 0
# define FUNC_HEADER(n) XP_LOGF( #n " called" )
# define FUNC_TAIL(n)   XP_LOGF( #n " done" )
#else
# define FUNC_HEADER(n)
# define FUNC_TAIL(n)
#endif

#define STACK_START(typ, var, siz ) typ var[siz]
#define STACK_END(s)
#define ADD_TO_STACK4(s,val,offset) \
    write_unaligned32( &s[offset], (unsigned long)val )
#define ADD_TO_STACK2(s,val,offset) \
    write_unaligned16( &s[offset], (unsigned short)val )
#define ADD_TO_STACK1(s,val,offset) \
    s[offset] = (unsigned char)val; \
    s[offset+1] = 0

#define SWAP1_NON_NULL_IN(ptr) if ( !!(ptr) ) { *(ptr) = Byte_Swap16(*(ptr)); }
#define SWAP2_NON_NULL_IN(ptr) if ( !!(ptr) ) { *(ptr) = Byte_Swap16(*(ptr)); }
#define SWAP4_NON_NULL_IN(ptr) if ( !!(ptr) ) { *(ptr) = Byte_Swap32(*(ptr)); }
#define SWAP1_NON_NULL_OUT(ptr) SWAP1_NON_NULL_IN(ptr) 
#define SWAP2_NON_NULL_OUT(ptr) SWAP2_NON_NULL_IN(ptr) 
#define SWAP4_NON_NULL_OUT(ptr) SWAP4_NON_NULL_IN(ptr) 

#define SET_SEL_REG(trap, sp) ((unsigned long*)((sp)->emulStateP))[3] = (trap)

void evt68k2evtARM( EventType* event, const unsigned char* evt68k );
#define SWAP_EVENTTYPE_68K_TO_ARM( dp, sp ) evt68k2evtARM( (dp), (sp) )
void evtArm2evt68K( unsigned char* evt68k, const EventType* event );
#define SWAP_EVENTTYPE_ARM_TO_68K( dp, sp ) evtArm2evt68K( (dp), (sp) )

void flipRect( RectangleType* rout, const RectangleType* rin );
#define SWAP_RECTANGLETYPE_ARM_TO_68K( dp, sp ) flipRect( (dp), (sp) )
#define SWAP_RECTANGLETYPE_68K_TO_ARM SWAP_RECTANGLETYPE_ARM_TO_68K

void flipFieldAttr( FieldAttrType* fout, const FieldAttrType* fin );
#define SWAP_FIELDATTRTYPE_ARM_TO_68K( dp, sp ) flipFieldAttr( (dp), (sp) )

void flipEngSocketFromArm( unsigned char* sout, const ExgSocketType* sin );
#define SWAP_EXGSOCKETTYPE_ARM_TO_68K( dp, sp ) flipEngSocketFromArm( (dp), (sp) )
void flipEngSocketToArm( ExgSocketType* out, const unsigned char* sin );
#define SWAP_EXGSOCKETTYPE_68K_TO_ARM( dp, sp ) flipEngSocketToArm( (dp), (sp) )

void flipFileInfoFromArm( unsigned char* fiout, const FileInfoType* fiin );
#define SWAP_FILEINFOTYPE_ARM_TO_68K( dp, sp ) \
    flipFileInfoFromArm( (unsigned char*)(dp), (sp) )
void flipFileInfoToArm( FileInfoType* fout, const unsigned char* fin );
#define SWAP_FILEINFOTYPE_68K_TO_ARM( dp, sp ) flipFileInfoToArm( (dp), (sp) )


#define SWAP_DATETIMETYPE_ARM_TO_68K( dp, sp ) /* nothing for now */
void flipDateTimeToArm( DateTimeType* out, const unsigned char* in );
#define SWAP_DATETIMETYPE_68K_TO_ARM( dp, sp ) flipDateTimeToArm( (dp), (sp) )

NetHostInfoPtr NetLibGetHostByName( UInt16 libRefNum, 
                                    const Char* nameP, NetHostInfoBufPtr bufP, 
                                    Int32 timeout, Err* errP );

#ifdef XWFEATURE_BLUETOOTH
void flipBtConnInfoArm268K( unsigned char* out, const BtLibSocketConnectInfoType* in );
void flipBtSocketListenInfoArm268K( unsigned char* out, 
                                     const BtLibSocketListenInfoType* in);

# define SWAP_BTLIBSOCKETCONNECTINFOTYPE_ARM_TO_68K( out, in ) \
    flipBtConnInfoArm268K( out, in )
# define SWAP_BTLIBSOCKETCONNECTINFOTYPE_68K_TO_ARM( out, in ) /* nop */
# define SWAP_BTLIBSOCKETLISTENINFOTYPE_ARM_TO_68K( out, in ) \
    flipBtSocketListenInfoArm268K( out, in )
# define SWAP_BTLIBSOCKETLISTENINFOTYPE_68K_TO_ARM( out, in ) /* nop */

void flipBtLibSdpUuidTypeArm268K( unsigned char* out, 
                                  const BtLibSdpUuidType* in );
void flipBtLibFriendlyNameTypeArm268K( unsigned char* out, 
                                       const BtLibFriendlyNameType* in );

# define SWAP_BTLIBSDPUUIDTYPE_ARM_TO_68K( out, in ) \
     flipBtLibSdpUuidTypeArm268K( out, in )
# define SWAP_BTLIBSDPUUIDTYPE_68K_TO_ARM( out, in ) /* nothing */
# define SWAP_BTLIBFRIENDLYNAMETYPE_ARM_TO_68K( out, in ) \
     flipBtLibFriendlyNameTypeArm268K( out, in )
# define SWAP_BTLIBFRIENDLYNAMETYPE_68K_TO_ARM( out, in ) /* nothing? */

#endif /* XWFEATURE_BLUETOOTH */

PNOState* getStorageLoc(void);
#define GET_CALLBACK_STATE() getStorageLoc()

#define crash()  *(int*)1L = 1

unsigned long Byte_Swap32( unsigned long l );
unsigned short Byte_Swap16( unsigned short l );
void write_unaligned16( unsigned char* dest, unsigned short val );
void write_unaligned32( unsigned char* dest, unsigned long val );
#define write_unaligned8( p, v ) *(p) = v

unsigned short read_unaligned16( const unsigned char* src );

#ifdef DEBUG
# define EMIT_NAME(name,bytes) \
    asm( "bal done_" name ); \
    asm( ".byte " bytes ); \
    asm( ".align" ); \
    asm( "done_" name ":" )

#else
# define EMIT_NAME(n,b)
#endif

/* Temporary until can generate */
#ifdef XWFEATURE_FIVEWAY
#include <Hs.h>
extern Err HsNavDrawFocusRing( FormType* formP, UInt16 objectID, 
                               Int16 extraInfo,
                               RectangleType* boundsInsideRingP,
                               HsNavFocusRingStyleEnum ringStyle, 
                               Boolean forceRestore);
#endif

#endif
#endif

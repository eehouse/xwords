// -*-mode: C; fill-column: 80; c-basic-offset: 4; -*-
/****************************************************************************
 *									    *
 *	Copyright 1998-2001 by Eric House (fixin@peak.org).  All rights reserved.	    *
 *									    *
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
 ****************************************************************************/
#include <StringMgr.h>
#include <MemoryMgr.h>
#include <SoundMgr.h>
#include <TimeMgr.h>
#include <Form.h>
#include <unix_stdarg.h>

#include "strutils.h"
#include "palmutil.h"
#include "xwords4defines.h"
#include "palmmain.h"
#include "comtypes.h"

/*****************************************************************************
 * This is meant to be replaced by an actual beep....
 ****************************************************************************/
#if defined FEATURE_BEEP || defined XW_FEATURE_UTILS
void beep( void ) {
    SndPlaySystemSound( sndError );
} /* beep */
#endif

/*****************************************************************************
 * 
 ****************************************************************************/
MemPtr
getActiveObjectPtr( UInt16 objectID )
{
    FormPtr form = FrmGetActiveForm();
    XP_ASSERT( FrmGetObjectPtr( 
	form, FrmGetObjectIndex( form, objectID ) )!= NULL );
    return FrmGetObjectPtr( form, FrmGetObjectIndex( form, objectID ) );
} /* getActiveObjectPtr */

/*****************************************************************************
 *
 ****************************************************************************/
void
getObjectBounds( UInt16 objectID, RectangleType* rectP ) 
{
   FormPtr form = FrmGetActiveForm();
   FrmGetObjectBounds( form, FrmGetObjectIndex( form, objectID ), rectP );
} /* getObjectBounds */

#if defined XW_FEATURE_UTILS
/*****************************************************************************
 *
 ****************************************************************************/
void
setObjectBounds( UInt16 objectID, RectangleType* rectP ) 
{
   FormPtr form = FrmGetActiveForm();
   FrmSetObjectBounds( form, FrmGetObjectIndex( form, objectID ), rectP );
} /* getObjectBounds */

/*****************************************************************************
 *
 ****************************************************************************/
void
setBooleanCtrl( UInt16 objectID, Boolean isSet ) 
{
    CtlSetValue( getActiveObjectPtr( objectID ), isSet );
} /* setBooleanCtrl */

void
setFieldEditable( FieldPtr fld, Boolean editable )
{
    FieldAttrType attrs;

    FldGetAttributes( fld, &attrs );
    if ( attrs.editable != editable ) {
        attrs.editable = editable;
        FldSetAttributes( fld, &attrs );
    }
} /* setFieldEditable */

void
disOrEnable( FormPtr form, UInt16 id, Boolean enable )
{
    UInt16 index = FrmGetObjectIndex( form, id );
    FormObjectKind typ;

    if ( enable ) {
        FrmShowObject( form, index );
    } else {
        FrmHideObject( form, index );
    }

    typ = FrmGetObjectType( form, index );
    if ( typ == frmControlObj ) {
        ControlPtr ctl = getActiveObjectPtr( id );
        CtlSetEnabled( ctl, enable );
    }
} /* disOrEnable */

void
disOrEnableSet( FormPtr form, const UInt16* ids, Boolean enable )
{
    for ( ; ; ) {
        XP_U16 id = *ids++;
        if ( !id ) {
            break;
        }
        disOrEnable( form, id, enable );
    }
} /* disOrEnableSet */

/* Position a control at the horizontal center of its container.
 */
void
centerControl( FormPtr form, UInt16 id )
{
    RectangleType cBounds, fBounds;
    UInt16 index = FrmGetObjectIndex( form, id );

    FrmGetObjectBounds( form, index, &cBounds );
    FrmGetFormBounds( form, &fBounds );

    cBounds.topLeft.x = (fBounds.extent.x - cBounds.extent.x) / 2;
    FrmSetObjectBounds( form, index, &cBounds );
} /* centerButton */

/*****************************************************************************
 *
 ****************************************************************************/
Boolean 
getBooleanCtrl( UInt16 objectID ) 
{
    return CtlGetValue( getActiveObjectPtr( objectID ) );
} /* getBooleanCtrl */

/*****************************************************************************
 * Set up to build the string and ptr-to-string lists needed for the
 * LstSetListChoices system call.
 ****************************************************************************/
void
initListData( MPFORMAL ListData* ld, XP_U16 nItems ) 
{
    /* include room for the closure */
    ld->strings = XP_MALLOC( mpool, ((nItems+1) * sizeof(*ld->strings)) );

    ld->nItems = nItems;
    ld->nextIndex = 0;
    ld->selIndex = -1;
} /* initListData */

void 
addListTextItem( MPFORMAL ListData* ld, XP_UCHAR* txt ) 
{
    XP_UCHAR* copy = copyString( MPPARM(mpool) txt );
    XP_ASSERT( ld->nextIndex < ld->nItems );
    ld->strings[++ld->nextIndex] = (char*)copy; /* ++ skips 0th for closure */
} /* addListTextItem */

/*****************************************************************************
 * Turn the list of offsets into ptrs, free the offsets list, and call
 * LstSetListChoices
 ****************************************************************************/
void 
setListChoices( ListData* ld, ListPtr list, void* closure ) 
{
    ld->strings[0] = closure;
    LstSetListChoices( list, &ld->strings[1], ld->nextIndex );
    if ( ld->selIndex >= 0 ) {
	LstSetSelection( list, ld->selIndex );
    }
} /* setListChoices */

/*****************************************************************************
 * Given a string, figure out which item it matches and save off that item's
 * index so that at show time it can be used to set the selection.  Note that
 * anything that changes the order will invalidate this, but also that there's
 * no harm in calling it again.
 ****************************************************************************/
void 
setListSelection( ListData* ld, char* selName )
{
    if ( !!selName ) {
	XP_U16 i;
	for ( i = 0; i < ld->nextIndex; ++i ) {
	    if ( StrCompare( ld->strings[i+1], selName ) == 0 ) {
		ld->selIndex = i;
		break;
	    }
	}
    } else {
	ld->selIndex = 0;
    }
} /* setListSelection */

/*****************************************************************************
 * Meant to be called after all items are added to the list, this function
 * sorts them in place.  For now I'll build in a call to StrCompare; later
 * it could be a callback passed in.
 ****************************************************************************/
void
sortList( ListData* ld ) 
{
    XP_S16 i, j, smallest;
    char** strings = ld->strings;
    char* tmp;

    for ( i = 1; i <= ld->nextIndex; ++i ) { /* skip 0th (closure) slot */
	for ( smallest = i, j = i+1; j <= ld->nextIndex; ++j ) {
	    if ( StrCompare( strings[smallest], strings[j] ) > 0 ) {
		smallest = j;
	    }
	}

	if ( smallest == i ) {	/* we got to the end without finding anything */
	    break;
	}

	tmp = strings[i];
	strings[i] = strings[smallest];
	strings[smallest] = tmp;
    }
} /* sortList */

/*****************************************************************************
 * Dispose the memory.  Docs don't say whether LstSetListChoices does this for
 * me so I assume not.  It'll crash if I'm wrong. :-)
 ****************************************************************************/
void
freeListData( MPFORMAL ListData* ld  ) 
{
    char** strings = ld->strings;
    XP_U16 i;

    for ( i = 0; i < ld->nextIndex; ++i ) {
	XP_FREE( mpool, *++strings ); /* skip 0th */
    }
    XP_FREE( mpool, ld->strings );
} /* freeListData */

/*****************************************************************************
 *
 ****************************************************************************/
void 
setSelectorFromList( UInt16 triggerID, ListPtr list, short listSelIndex ) 
{
    XP_ASSERT( list != NULL );
    XP_ASSERT( getActiveObjectPtr( triggerID ) != NULL );
    XP_ASSERT( !!LstGetSelectionText( list, listSelIndex ) );
    CtlSetLabel( getActiveObjectPtr( triggerID ),
		 LstGetSelectionText( list, listSelIndex ) );
} /* setTriggerFromList */

#define MAX_GADGET_COUNT 3	/* the most we need at this point */
void
sizeGadgetsForStrings( FormPtr form, ListPtr list, XP_U16 firstGadgetID )
{
    XP_U16 i;
    XP_U16 nGadgets = LstGetNumberOfItems( list );
    XP_U16 strWidths[MAX_GADGET_COUNT];
    XP_U16 totalStrWidth, totalGadgetWidth;
    XP_U16 left, extra;
    RectangleType rects[MAX_GADGET_COUNT];
    RectangleType* rp;

    XP_ASSERT( nGadgets <= MAX_GADGET_COUNT );

    totalStrWidth = totalGadgetWidth = 0;
    for ( i = 0, rp = rects; i < nGadgets; ++i, ++rp ) {
        XP_U16 len;
        XP_UCHAR* txt = (XP_UCHAR*)LstGetSelectionText( list, i );

        len = XP_STRLEN( (const char*)txt );
        totalStrWidth += strWidths[i] = FntCharsWidth( (const char*)txt, len );

        getObjectBounds( firstGadgetID+i, rp );
        totalGadgetWidth += rp->extent.x;
    }

    XP_ASSERT( totalGadgetWidth >= totalStrWidth );
    extra = (totalGadgetWidth - totalStrWidth) / nGadgets;

    left = rects[0].topLeft.x;
    for ( i = 0, rp = rects; i < nGadgets; ++i, ++rp ) {
        UInt16 index;
        UInt16 width;

        rp->topLeft.x = left;
        rp->extent.x = width = strWidths[i] + extra;

        index = FrmGetObjectIndex( form, firstGadgetID+i );
        FrmSetObjectBounds( form, index, rp );

        left += width;
    }
} /* sizeGadgetsForStrings */

XP_Bool
penInGadget( EventPtr event, UInt16* whichGadget )
{
    UInt16 x = event->screenX;
    UInt16 y = event->screenY;
    FormPtr form = FrmGetActiveForm();
    UInt16 nObjects, i;
    XP_Bool result = XP_FALSE;

    for ( i = 0, nObjects = FrmGetNumberOfObjects(form); i < nObjects; ++i ) {
	if ( frmGadgetObj == FrmGetObjectType( form, i ) ) {
	    UInt16 objId = FrmGetObjectId( form, i );
	    if ( objId != REFCON_GADGET_ID ) {
		RectangleType rect;
		FrmGetObjectBounds( form, i, &rect );
		if ( RctPtInRectangle( x, y, &rect ) ) {
		    *whichGadget = objId;
		    result = XP_TRUE;
		    break;
		}
	    }
	}
    }
    
    return result;
} /* penInGadget */

void
setFormRefcon( void* refcon )
{
    UInt16 index;
    FormPtr form = FrmGetFormPtr( XW_MAIN_FORM );
    XP_ASSERT( !!form );
    index = FrmGetObjectIndex( form, REFCON_GADGET_ID );
    FrmSetGadgetData( form, index, refcon );
} /* setFormRefcon */

void* 
getFormRefcon()
{
    void* result = NULL;
    FormPtr form = FrmGetFormPtr( XW_MAIN_FORM );
    XP_ASSERT( !!form );
    if ( !!form ) {
        UInt16 index = FrmGetObjectIndex( form, REFCON_GADGET_ID );
        result = FrmGetGadgetData( form, index );
    }
    return result;
} /* getFormRefcon */
#endif

#if defined FEATURE_REALLOC || defined XW_FEATURE_UTILS
XP_U8* 
palm_realloc( XP_U8* in, XP_U16 size )
{
    MemPtr ptr = (MemPtr)in;
    XP_U32 oldsize = MemPtrSize( ptr );
    MemPtr newptr = MemPtrNew( size );
    XP_ASSERT( !!newptr );
    XP_MEMCPY( newptr, ptr, oldsize );
    MemPtrFree( ptr );
    return newptr;
} /* palm_realloc */

XP_U16
palm_snprintf( XP_UCHAR* buf, XP_U16 len, XP_UCHAR* format, ... )
{
    XP_U16 nChars;
    /* PENDING use len to avoid writing too many chars */
    va_list ap;
    
    va_start( ap, format );
    nChars = StrVPrintF( (char*)buf, (char*)format, ap );
    va_end( ap );
    XP_ASSERT( len >= nChars );
    return nChars;
} /* palm_snprintf */
#endif

#ifdef DEBUG
void
palm_warnf( char* format, ... )
{
    char buf[200];
    va_list ap;
    
    va_start( ap, format );
    StrVPrintF( buf, format, ap );
    va_end( ap );

#ifdef FOR_GREMLINS
    /* If gremlins are active, we want all activity to stop here!  That
       means no cancellation */
    {
        FormPtr form;
        FieldPtr field;

        form = FrmInitForm( XW_GREMLIN_WARN_FORM_ID );

        FrmSetActiveForm( form );

        FrmSetEventHandler( form, doNothing );

        field = getActiveObjectPtr( XW_GREMLIN_WARN_FIELD_ID );
        FldSetTextPtr( field, buf );
        FldRecalculateField( field, true );

        FrmDrawForm( form );
#if 1
        /* This next may freeze X (go to a console to kill POSE), but when
           you look at the logs you'll see what event you were on when the
           ASSERT fired.  Otherwise the events just keep coming and POSE
           fills up the log.  */
        while(1);
#endif
        /* this should NEVER return */
        (void)FrmDoDialog( form );
    }
#else
    (void)FrmCustomAlert( XW_ERROR_ALERT_ID, buf, " ", " " );
#endif
} /* palm_warnf */

void
palm_assert( Boolean b, int line, char* fileName )
{
    if ( !b ) {
        XP_LOGF( "ASSERTION FAILED: line %d in %s", line, fileName );
        XP_WARNF( "ASSERTION FAILED: line %d in %s", line, fileName );
    }
} /* palmassert */

static void
logToMemo( char* buf )
{
    const XP_U16 MAX_MEMO_SIZE = 4000;
    const XP_U16 MAX_NRECORDS = 200;
    DmOpenRef ref;
    UInt16 nRecords, index;
    UInt16 len = XP_STRLEN( buf );
    UInt16 hSize, slen;
    MemHandle hand;
    MemPtr ptr;
    char tsBuf[20];
    UInt16 tsLen;
    DateTimeType dtType;
    LocalID dbID;

    TimSecondsToDateTime( TimGetSeconds(), &dtType );
    StrPrintF( tsBuf, "\n%d:%d:%d-", dtType.hour, dtType.minute, 
               dtType.second );
    tsLen = XP_STRLEN(tsBuf);

    (void)DmCreateDatabase( CARD_0, "MemoDB", 'memo', 'DATA', false );
    dbID = DmFindDatabase( CARD_0, "MemoDB" );
    ref = DmOpenDatabase( CARD_0, dbID, dmModeWrite );

    nRecords = DmNumRecordsInCategory( ref, dmAllCategories );
    if ( nRecords == 0 ) {
        index = dmMaxRecordIndex;
        hSize = 0;
        hand = DmNewRecord( ref, &index, 1 );
        DmReleaseRecord( ref, index, true );
    } else {

        while ( nRecords > MAX_NRECORDS ) {
            index = 0;
            DmSeekRecordInCategory( ref, &index, 0, dmSeekForward, 
                                    dmAllCategories);
            DmRemoveRecord( ref, index );
            --nRecords;
        }

        index = 0;
        DmSeekRecordInCategory( ref, &index, nRecords, dmSeekForward, 
                                dmAllCategories);
        hand = DmGetRecord( ref, index );

        XP_ASSERT( !!hand );
        hSize = MemHandleSize( hand ) - 1;
        ptr = MemHandleLock( hand );
        slen = XP_STRLEN(ptr);
        MemHandleUnlock(hand);

        if ( hSize > slen ) {
            hSize = slen;
        }
        (void)DmReleaseRecord( ref, index, false );
    }

    if ( (hSize + len + tsLen) > MAX_MEMO_SIZE ) {
        index = dmMaxRecordIndex;
        hand = DmNewRecord( ref, &index, len + tsLen + 1 );
        hSize = 0;
    } else {
        (void)DmResizeRecord( ref, index, len + hSize + tsLen + 1 );
        hand = DmGetRecord( ref, index );
    }

    ptr = MemHandleLock( hand );
    DmWrite( ptr, hSize, tsBuf, tsLen );
    DmWrite( ptr, hSize + tsLen, buf, len + 1 );
    MemHandleUnlock( hand );

    DmReleaseRecord( ref, index, true );
    DmCloseDatabase( ref );
} /* logToMemo */

void
palm_debugf( char* format, ...)
{
    char buf[200];
    va_list ap;
    
    va_start( ap, format );
    StrVPrintF( buf, format, ap );
    va_end( ap );

    logToMemo( buf );
} /* debugf */

/* if -0 isn't passed to compiler it wants to find this!!! */
void p_ignore(char* s, ...) {}

#ifdef FOR_GREMLINS
static Boolean 
doNothing( EventPtr event )
{
    return true;
} /* doNothing */
#endif

void
palm_logf( char* format, ... )
{
    char buf[200];
    va_list ap;
    
    va_start( ap, format );
    StrVPrintF( buf, format, ap );
    va_end( ap );

    logToMemo( buf );
} /* palm_logf */

#endif /* DEBUG */

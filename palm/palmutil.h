// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/****************************************************************************
 *									    *
 *	Copyright 1998-1999, 2001 by Eric House (xwords@eehouse.org).  All rights reserved.	    *
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

#ifndef _PALMUTIL_H_
#define  _PALMUTIL_H_

#include <PalmTypes.h>
#include <List.h>
/* #include <Window.h> */
/* #include <FeatureMgr.h> */
/* #include <CharAttr.h> */
/* #include <DLServer.h> */
/* #include "xwdefines.h" */
/* #include "xwords.h" */
/* #include "xwdebug.h" */

#include "palmmain.h"

/* short myMemCmp( unsigned char* src1, unsigned char* src2, short size ); */
/* void userError( CharPtr* str ); */
/* void userErrorRes( short resId ); */
/* void userErrorResS( short resId, CharPtr data ); */
void beep( void );

MemPtr getActiveObjectPtr( UInt16 objectID );
void getObjectBounds( UInt16 objectID, RectangleType* rectP );
void setObjectBounds( UInt16 objectID, RectangleType* rectP );

void disOrEnable( FormPtr form, UInt16 id, Boolean enable );
void disOrEnableSet( FormPtr form, const UInt16* id, Boolean enable );

void disOrEnableTri( FormPtr form, UInt16 id, XP_TriEnable enable );

void centerControls( FormPtr form, const UInt16* id, XP_U16 nIds );

void setBooleanCtrl( UInt16 objectID, Boolean isSet );
Boolean getBooleanCtrl( UInt16 objectID );

void setFieldStr( XP_U16 id, const XP_UCHAR* buf );
#ifdef XWFEATURE_RELAY
void getFieldStr( XP_U16 id, XP_UCHAR* buf, XP_U16 max );
#endif
void setFieldEditable( UInt16 objectID, Boolean editable );

void postEmptyEvent( eventsEnum typ );

/* list item stuff */
void initListData( MPFORMAL ListData* ld, XP_U16 nItems );
void addListTextItem( MPFORMAL ListData* ld, const XP_UCHAR* txt );
void setListChoices( ListData* ld, ListPtr list, void* closure );
void setListSelection( ListData* ld, const char* selName );
void sortList( ListData* ld );
void freeListData( MPFORMAL ListData* ld  );

/* this should work on either trigger or selector */
void setSelectorFromList( UInt16 selectorID, ListPtr list, 
                          short listSelIndex );

void sizeGadgetsForStrings( FormPtr form, ListPtr list, XP_U16 firstGadgetID );
void drawGadgetsFromList( ListPtr list, XP_U16 idLow, XP_U16 idHigh, 
                          XP_U16 hiliteItem );

XP_Bool penInGadget( const EventType* event, UInt16* whichGadget );
void drawOneGadget( UInt16 id, const char* text, Boolean hilite );
# ifdef XWFEATURE_FIVEWAY
XP_S16 getFocusOwner( void );
void setFormFocus( FormPtr form, XP_U16 objectID );
XP_Bool isFormObject( FormPtr form, XP_U16 objectID );
void drawFocusRingOnGadget( PalmAppGlobals* globals, XP_U16 idLow, 
                            XP_U16 idHigh );
XP_Bool considerGadgetFocus( PalmAppGlobals* globals, const EventType* event, 
                             XP_U16 idLow, XP_U16 idHigh );

XP_Bool tryRockerKey( XP_U16 key, XP_U16 selGadget, 
                      XP_U16 idLow, XP_U16 idHigh );
# endif

void setFormRefcon( void* refcon );
void* getFormRefcon(void);
void fitButtonToString( XP_U16 id );


#ifdef DEBUG
void PalmClearLogs( void );
#endif

#endif

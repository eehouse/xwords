/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make ARCH=ARM_ONLY MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1999 - 2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#include "palmutil.h"
#include "LocalizedStrIncludes.h"

static Boolean 
handleKeysInBlank( EventPtr event )
{
    Boolean handled = false;

    if ( event->eType == keyDownEvent ) {
        char ch = event->data.keyDown.chr;

        if ( ch >= 'a' && ch <= 'z' ) {
            ch += 'A' - 'a';
        }
        if ( ch >= 'A' && ch <= 'Z' ) {
            ListPtr lettersList = getActiveObjectPtr( XW_BLANK_LIST_ID );
            XP_U16 nItems;
            XP_U16 ii;

            XP_ASSERT( !!lettersList );
            nItems = LstGetNumberOfItems( lettersList );

            for ( ii = 0; ii < nItems; ++ii ) {
                XP_UCHAR* itext = LstGetSelectionText( lettersList, ii );

                if ( !!itext && (itext[0] == ch) ) {
                    LstSetSelection( lettersList, ii );
                    LstMakeItemVisible( lettersList, ii );
                    handled = true;
                    break;
                }
            }
        } else if ( ch == '\n' ) {
            EventType eventToPost;

            eventToPost.eType = ctlSelectEvent;
            eventToPost.data.ctlSelect.controlID = XW_BLANK_OK_BUTTON_ID;
            eventToPost.data.ctlSelect.pControl = 
                getActiveObjectPtr( XW_BLANK_OK_BUTTON_ID );
            EvtAddEventToQueue( &eventToPost );
        }
    }

    return handled;
} /* handleKeysInBlank */

XP_S16
askBlankValue( PalmAppGlobals* globals, XP_U16 playerNum, const PickInfo* pi,
               XP_U16 nTiles, const XP_UCHAR** texts )
{
    FormPtr form, prevForm;
    ListPtr lettersList;
    ListData ld;
    XP_U16 i;
    XP_S16 chosen;
    XP_UCHAR labelBuf[96];
    XP_UCHAR* name;
    const XP_UCHAR* labelFmt;
    FieldPtr fld;
    XP_U16 tapped;
#ifdef FEATURE_TRAY_EDIT
    XP_Bool forBlank = pi->why == PICK_FOR_BLANK;
#endif

    initListData( MEMPOOL &ld, nTiles );

    for ( i = 0; i < nTiles; ++i ) {	
        addListTextItem( MEMPOOL &ld, texts[i] );
    }

    prevForm = FrmGetActiveForm();
    form = FrmInitForm( XW_BLANK_DIALOG_ID );
    FrmSetActiveForm( form );

#ifdef FEATURE_TRAY_EDIT
    disOrEnable( form, XW_BLANK_PICK_BUTTON_ID, !forBlank );
    disOrEnable( form, XW_BLANK_BACKUP_BUTTON_ID, 
                 !forBlank && pi->thisPick > 0 );
#endif

    lettersList = getActiveObjectPtr( XW_BLANK_LIST_ID );
    setListChoices( &ld, lettersList, NULL );

    LstSetSelection( lettersList, 0 );

    name = globals->gameInfo.players[playerNum].name;
    labelFmt = getResString( globals, 
#ifdef FEATURE_TRAY_EDIT
                             !forBlank? STRS_PICK_TILE:
#endif
                             STR_PICK_BLANK );
    XP_SNPRINTF( labelBuf, sizeof(labelBuf), labelFmt, name );

#ifdef FEATURE_TRAY_EDIT
    if ( !forBlank ) {
        const char* cur = getResString( globals, STR_PICK_TILE_CUR );
        XP_U16 lenSoFar;
        XP_U16 i;

        lenSoFar = XP_STRLEN(labelBuf);
        lenSoFar += XP_SNPRINTF( labelBuf + lenSoFar, 
                                 sizeof(labelBuf) - lenSoFar,
                                 " (%d/%d)\n%s", pi->thisPick+1, pi->nTotal, 
                                 cur );

        for ( i = 0; i < pi->nCurTiles; ++i ) {
            lenSoFar += XP_SNPRINTF( labelBuf+lenSoFar, 
                                     sizeof(labelBuf)-lenSoFar, "%s%s",
                                     i==0?": ":", ", pi->curTiles[i] );
        }
    }
#endif

    fld = getActiveObjectPtr( XW_BLANK_LABEL_FIELD_ID );
    FldSetTextPtr( fld, labelBuf );
    FldRecalculateField( fld, false );

    FrmDrawForm( form );

    FrmSetEventHandler( form, handleKeysInBlank );
    tapped = FrmDoDialog( form );

    if ( 0 ) {
#ifdef FEATURE_TRAY_EDIT
    } else if ( tapped == XW_BLANK_PICK_BUTTON_ID ) {
        chosen = PICKER_PICKALL;
    } else if ( tapped == XW_BLANK_BACKUP_BUTTON_ID ) {
        chosen = PICKER_BACKUP;
#endif
    } else {
        chosen = LstGetSelection( lettersList );
    }

    FrmDeleteForm( form );
    FrmSetActiveForm( prevForm );

    freeListData( MEMPOOL &ld );

    return chosen;
} /* askBlankValue */

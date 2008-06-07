/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/****************************************************************************
 *
 * Copyright 1999 - 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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
 *
 ****************************************************************************/

#include <PalmTypes.h>
#include <Form.h>
#include <VFSMgr.h>
#include <FeatureMgr.h>

#include "callback.h"
#include "dictui.h"
#include "palmmain.h"
#include "palmutil.h"
#include "palmdict.h"
#include "dictlist.h"
#include "strutils.h"
#include "xwords4defines.h"

#define TYPE_DAWG 'DAWG'
#define TYPE_XWR3 'Xwr3'

//////////////////////////////////////////////////////////////////////////////
// Prototypes
//////////////////////////////////////////////////////////////////////////////
static XP_U16 populateDictionaryList( MPFORMAL ListData* sLd, 
                                      const XP_UCHAR* curDictName,
                                      ListPtr list, Int16 triggerID,
                                      PalmDictList* dl );
static Boolean beamDict( const PalmDictList* dl, XP_UCHAR* dictName );

/*****************************************************************************
 * Handler for dictionary info form.
 ****************************************************************************/
#define USE_POPULATE 1
Boolean 
dictFormHandleEvent( EventPtr event ) 
{
    FormPtr form;
    Boolean result;
    Int16 chosen;
    DictionaryCtxt* dict;
    PalmAppGlobals* globals;
    XP_UCHAR* dictName;

    CALLBACK_PROLOGUE();

    result = false;
    globals = getFormRefcon();

    switch ( event->eType ) {
    case frmOpenEvent: {
        const XP_UCHAR* curName;
        XP_U16 width;
        RectangleType rect;

        form = FrmGetActiveForm();

        /* we're either a beam dlg or a dict picker; disable a button here. */
        disOrEnable( form, 
                     globals->dictuiForBeaming ?
                     XW_DICTINFO_DONE_BUTTON_ID:XW_DICTINFO_BEAM_BUTTON_ID,
                     false );

        /* dictionary list setup */
        globals->dictState.dictList = 
            getActiveObjectPtr( XW_DICTINFO_LIST_ID );

        dict = !!globals->game.model? 
            model_getDictionary(globals->game.model) : NULL;
        if ( dict ) {
            curName = dict_getName( dict );
        } else {
            curName = NULL;
        }
        width = populateDictionaryList( MPPARM(globals->mpool) 
                                        &globals->dictState.sLd, 
                                        curName, globals->dictState.dictList, 
                                        XW_DICTINFO_TRIGGER_ID,
                                        globals->dictList );
        getObjectBounds( XW_DICTINFO_LIST_ID, &rect );
        rect.extent.x = width;
        setObjectBounds( XW_DICTINFO_LIST_ID, &rect );        

        FrmDrawForm( form );
        break;
    }

    case ctlSelectEvent:
        switch ( event->data.ctlEnter.controlID ) {

        case XW_DICTINFO_TRIGGER_ID:
            // don't let change dict except first time
            if ( globals->dictuiForBeaming || globals->isNewGame ) { 
                chosen = LstPopupList( globals->dictState.dictList );
                if ( chosen >= 0 ) {
                    setSelectorFromList( XW_DICTINFO_TRIGGER_ID, 
                                         globals->dictState.dictList,
                                         chosen );
                }
            }
            result = true;
            break;
/*         case XW_PHONIES_TRIGGER_ID: */
/*             chosen = LstPopupList( sPhoniesList ); */
/*             if ( chosen >= 0 ) { */
/*                 setTriggerFromList( XW_PHONIES_TRIGGER_ID, sPhoniesList, */
/*                                     chosen ); */
/*             } */
/*             result = true; */
/*             break; */

        case XW_DICTINFO_DONE_BUTTON_ID: 
        case XW_DICTINFO_BEAM_BUTTON_ID:
            /* discard the const */
            dictName = (XP_UCHAR*)CtlGetLabel(
                 getActiveObjectPtr( XW_DICTINFO_TRIGGER_ID) );

            if ( globals->dictuiForBeaming ) {
                if ( !beamDict( globals->dictList, dictName ) ) {
                    break; /* don't cancel dialog yet */
                }
            } else {
                EventType eventToPost;

                XP_ASSERT( dictName != NULL );

                eventToPost.eType = dictSelectedEvent;
                ((DictSelectedData*)&eventToPost.data.generic)->dictName
                    = copyString( globals->mpool, dictName );
                EvtAddEventToQueue( &eventToPost );
            }

        case XW_DICTINFO_CANCEL_BUTTON_ID:
            result = true;
            freeListData( MPPARM(globals->mpool) &globals->dictState.sLd );
            FrmReturnToForm( 0 );
            break;

        } // switch ( event->data.ctlEnter.controlID )
        break;

    default:
        break;
    } // switch

    CALLBACK_EPILOGUE();
    return result;
} /* dictFormHandleEvent */

/*****************************************************************************
 *
 ****************************************************************************/
static XP_U16
populateDictionaryList( MPFORMAL ListData* sLd, const XP_UCHAR* curDictName,
                        ListPtr list, Int16 triggerID, PalmDictList* dl )
{
    XP_U16 i;
    XP_U16 maxWidth = 0;
    XP_U16 nDicts;

    initListData( MPPARM(mpool) sLd, 16 ); /* PENDING: MAX_DICTS or count */
    nDicts = DictListCount( dl );

    for ( i = 0; i < nDicts; ++i ) {
        DictListEntry* dle;
        XP_UCHAR* name;
        XP_U16 width;

        getNthDict( dl, i, &dle );
        name = dle->baseName;

        addListTextItem( MPPARM(mpool) sLd, name );
        width = FntCharsWidth( (const char*)name, XP_STRLEN((const char*)name) );
        if ( width > maxWidth ) {
            maxWidth = width;
        }
    }

    sortList( sLd );
    setListSelection( sLd, (char*)curDictName );
    setListChoices( sLd, list, NULL );

    setSelectorFromList( triggerID, list, LstGetSelection(list) );

    return maxWidth + 3;        /* 3: for white space */
} /* populateDictionaryList */

/***********************************************************************
 * The rest of this file mostly stolen from palmos.com.  I've only modified
 * beamDict
 ************************************************************************/
static Err
WriteDBData(const void* dataP, UInt32* sizeP, void* userDataP)
{
    Err            err;

    /* Try to send as many bytes as were requested by the caller */
    *sizeP = ExgSend((ExgSocketPtr)userDataP, (void*)dataP, *sizeP, &err);
    return err;
} /* WriteDBData */

Err
sendDatabase( UInt16 cardNo, LocalID dbID, XP_UCHAR* nameP, 
              XP_UCHAR* descriptionP )
{
    ExgSocketType       exgSocket;
    Err                      err;

    /* Create exgSocket structure */
    XP_MEMSET( &exgSocket, 0, sizeof(exgSocket) );
    exgSocket.description = (char*)descriptionP;
    exgSocket.name = (char*)nameP;

    /* Start an exchange put operation */
    err = ExgPut(&exgSocket);
    if ( !err ) {
        err = ExgDBWrite( WriteDBData, &exgSocket, NULL, dbID, cardNo );
        /* Disconnect Exg and pass error */
        err = ExgDisconnect(&exgSocket, err);
    }
    return err;
} /* sendDatabase */

static Boolean
beamDict( const PalmDictList* dl, XP_UCHAR* dictName )
{
    Err err;
    UInt16 cardNo;
    LocalID dbID;
    Boolean found;
    XP_Bool shouldDispose = XP_FALSE;
    DictListEntry* dle;

    found = getDictWithName( dl, dictName, &dle );

    /* Find our app using its internal name */
    XP_ASSERT( found );

    if ( found ) {
        if ( dle->location == DL_VFS ) {
            err = VFSImportDatabaseFromFile( dle->u.vfsData.volNum, 
                                             (const char*)dle->path,
                                             &cardNo, &dbID );
            if ( err == dmErrAlreadyExists ) {
            } else if ( err == errNone ) {
                shouldDispose = XP_TRUE;
            } else {
                found = XP_FALSE;
            }
        } else {
            cardNo = dle->u.dmData.cardNo;
            dbID = dle->u.dmData.dbID;
        }
    }

    if ( found ) {    /* send it giving external name and description */
        XP_UCHAR prcName[40];
        XP_SNPRINTF( prcName, sizeof(prcName), (XP_UCHAR*)"%s.pdb", dictName );
        err = sendDatabase( cardNo, dbID, prcName, dictName );
        found = err == 0;

        if ( shouldDispose ) {
            DmDeleteDatabase( cardNo, dbID );
        }
    }

    return found;
} /* beamDict */


/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <assert.h>
#include <ctype.h>
#include "sys.h"
#include "gui.h"
#include "ebm_object.h"

extern "C" {
#include "xptypes.h"
#include "xwstream.h"
}

#include "frankshowtext.h"
#include "frankids.h"

#define MAX_DLG_HT 200
#define SHOWTEXT_WINDOW_ID 2000
#define SHOW_TEXT_ID 2001
#define TEXT_X       5
#define TEXT_PADDING_ABOVE 5
#define TEXT_PADDING_BELOW 5
#define TEXT_Y       TEXT_PADDING_ABOVE
#define TEXT_WIDTH   180
#define TEXT_HEIGHT  40

#define TEXT_PADDING (TEXT_PADDING_ABOVE+TEXT_PADDING_BELOW)
#define TITLE_BAR_HT 15


#define OK_BUTTON_ID 1000
#define CANCEL_BUTTON_ID 1001
#define OK_BUTTON_X 40
#define CANCEL_BUTTON_X 100
#define BUTTON_HEIGHT 12
#define BUTTON_PADDING 3	/* below buttons */

CShowTextWindow::CShowTextWindow( MPFORMAL XWStreamCtxt* stream, 
                                  const char* title,
                                  XP_Bool killStream, XP_Bool showCancel,
                                  XP_U16* resultLoc )
    : CWindow( SHOWTEXT_WINDOW_ID, 5, 170, 190, 
               TEXT_HEIGHT + TEXT_PADDING + TITLE_BAR_HT,
               title, TRUE, FALSE, !showCancel )
{
    MPASSIGN( this->mpool, mpool );

    CButton* okButton = (CButton*)NULL;
    CButton* cancelButton = (CButton*)NULL;
    
    fResultLoc = resultLoc;

    CTextEdit* entry = new CTextEdit( SHOW_TEXT_ID, TEXT_WIDTH, TEXT_HEIGHT, 
                                      TEXTOPTION_NOEDIT | 
                                      TEXTOPTION_NOUNDERLINE);

    /* copy the stream's text into the texteditor */
    stream_putU8( stream, '\0' );
    XP_U16 len = stream_getSize( stream );
    char* textPtr = (char*)XP_MALLOC( mpool, len );
    stream_getBytes( stream, textPtr, len );
    XP_ASSERT( textPtr[len-1] == '\0' );
    entry->SetText(textPtr);
    XP_FREE( mpool, textPtr );

    if ( killStream ) {
        stream_destroy( stream );
    }

    RECT rect;
    GetUsableRect( &rect );
    U16 titleBarHt = rect.y - GetY();

    U16 maxTextHeight = MAX_DLG_HT - TEXT_PADDING - titleBarHt;
    U16 buttonHeight;
    if ( showCancel ) {
        okButton = new CButton( OK_BUTTON_ID, 0, 0, "Ok" );
        cancelButton = new CButton( CANCEL_BUTTON_ID, 0, 0, "Cancel" );
        buttonHeight = okButton->GetHeight() + BUTTON_PADDING;
        maxTextHeight -= buttonHeight;
    } else {
        buttonHeight = 0;
    }

    /* FIND out how big the text wants to be.  Make the window as big as
       necessary, growing it upward. */

    U16 curTextHeight = entry->GetMaxHeight();
    if ( curTextHeight > maxTextHeight ) {
        curTextHeight = maxTextHeight;
    }
    entry->SetHeight( curTextHeight );
    
    U16 newDlgHeight = curTextHeight + buttonHeight + TEXT_PADDING
        + titleBarHt;
    S16 diff = newDlgHeight - GetHeight();
    SetY( GetY() - diff );
    SetHeight( newDlgHeight );

    this->AddChild( entry, TEXT_X, TEXT_Y );

    if ( showCancel ) {
        U16 buttonY = TEXT_Y + curTextHeight + TEXT_PADDING_BELOW;
        AddChild( okButton, OK_BUTTON_X, buttonY );
        AddChild( cancelButton, CANCEL_BUTTON_X, buttonY );
    }
    
} /* CShowTextWindow */

S32
CShowTextWindow::MsgHandler( MSG_TYPE type, CViewable *object, S32 data )
{
    S32 result = 0;

    switch ( type ) {
    case MSG_BUTTON_SELECT:	/* there's only one button */
        result = 1;
        switch ( object->GetID() ) {
        case OK_BUTTON_ID:
            *fResultLoc = 1;
            break;
        case CANCEL_BUTTON_ID:
            *fResultLoc = 0;
            break;
        }
        break;

    default:
        break;
    }

    if ( result == 1 ) {
        this->Close();
    }

    return result;
} /* MsgHandler */

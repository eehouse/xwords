// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 1999-2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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
}

#include "frankletter.h"

#include "frankids.h"
#include "frankask.h"

#define LETTER_HEIGHT 12
#define LETTERS_ROW_HEIGHT LETTER_HEIGHT
#define LETTERS_ROW_WIDTH 30
#define LETTERS_NUM_VISROWS 16

class LettersList : public CList {
private:
    const XP_UCHAR4* fTexts;

 public:
    LettersList( const XP_UCHAR4* texts, U16 numRows ); 
    
    U16 GetRowHeight( S32 row ) { return LETTER_HEIGHT; }
    void DrawRow( RECT *rect, S32 row );
};

LettersList::LettersList( const XP_UCHAR4* texts, U16 numRows )
    : CList( 1001, LETTERS_ROW_WIDTH, 
	     LETTERS_ROW_HEIGHT * LETTERS_NUM_VISROWS, 
	     numRows, LISTOPTION_ALWAYS_HIGHLIGHT )
{
    fTexts = texts;

    this->SetCurrentRow(0);    // Select the first item so there's a default
}

void LettersList::DrawRow( RECT *rect, S32 row )
{
    CWindow* window = this->GetWindow();
    window->DrawText( (char*)fTexts[row], rect->x, rect->y );
} /* LettersList::DrawRow */

CAskLetterWindow::CAskLetterWindow( const PickInfo* pi, XP_U16 playerNum,
                                    const XP_UCHAR4* texts, XP_U16 nTiles, 
                                    XP_S16* resultP )
    : CWindow( ASKLETTER_WINDOW_ID, 55, 15, 80, 220, "Blank", TRUE )
{
    fTexts = texts;
    fNTiles = nTiles;
    fResultP = resultP;

    this->list = new LettersList( texts, nTiles );
    this->AddChild( this->list, 5, 5 );

    CButton* okbutton = new CButton( 1000, 0, 0, "Ok" );    
    this->AddChild( okbutton, 40, 70 );
} // CAskWindow

S32
CAskLetterWindow::MsgHandler( MSG_TYPE type, CViewable *object, S32 data )
{
    S32 result = 0;

    switch (type) {
    case MSG_BUTTON_SELECT:	// there's only one button....
        *fResultP = this->list->GetCurrentRow();

        this->Close();
        result = 1;
        break;
	
    case MSG_KEY: // allow keys to select the matching letter in the list 
        if ( isalpha( data ) ) {
            XP_UCHAR ch = toupper(data);
            for ( U16 i = 0; i < fNTiles; ++i ) {
                if ( ch == fTexts[i][0] ) {
                    this->list->SetCurrentRow( i );
                    result = 1;
                    break;                    
                }
            }
        }	

    default:
        break;
    }

    if ( result == 0 ) {
        result = CWindow::MsgHandler( type, object, data );
    }
    return result;
} // CAskLetterWindow::MsgHandler




// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 1999-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#if 0
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "sys.h"
#include "gui.h"
#include "OpenDatabases.h"
#include "ebm_object.h"

extern "C" {
#include "xptypes.h"
#include "board.h"
#include "model.h"
}

#include "frankids.h"
#include "frankask.h"

#define FIRST_BUTTON_ID 2000

CAskDialog::CAskDialog( U16* resultP, char* question, U16 numButtons, ... )
    : CWindow( ASK_WINDOW_ID, 2, 50, 196, 80, "Question..." )
{
    U16 i;
    va_list ap;

    this->resultP = resultP;
    this->question = question;
    this->drawInProgress = FALSE;

    U16 horSpacing = 196 / (numButtons+1);

    va_start(ap, numButtons);
    for ( i = 0; i < numButtons; ++i ) {
	char* buttName = va_arg( ap, char*);
	XP_DEBUGF( "button %d's title is %s\n", i, buttName );
	CButton* button = new CButton( i+FIRST_BUTTON_ID, 0, 0, buttName );

	U16 width = button->GetWidth();
	U16 bx = horSpacing * (i+1);
	this->AddChild( button, bx - (width/2), 40 );
    }
    va_end(ap);


    this->textY = 10;
    this->textX = 5;
} // CAskDialog::CAskDialog

void
CAskDialog::Draw()
{
    if ( !this->drawInProgress ) {

	this->drawInProgress = TRUE;

	CWindow::Draw();		// buttons, etc.

	this->DrawText( this->question, this->textX, this->textY );

	this->drawInProgress = FALSE;
    }
} // CAskDialog::Draw

S32
CAskDialog::MsgHandler( MSG_TYPE type, CViewable *object, S32 data )
{
    S32 result = 0;
    S16 id;

    switch (type) {
    case MSG_BUTTON_SELECT:
	id = object->GetID();
	*this->resultP = id - FIRST_BUTTON_ID;
	this->Close();
	result = 1;
    default:
	break;
    }

    if ( result == 0 ) {
	result = CWindow::MsgHandler( type, object, data );
    }
    return result;
} // CGamesBrowserWindow::MsgHandler

#endif /* 0 */

// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
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
}

#include "frankpasswd.h"

#include "frankids.h"

#define TEXT_ID 2000
#define PASSWD_WIDTH 105
#define PASSWD_HEIGHT 12

#define LABEL_X 10
#define LABEL_Y 10
#define PASSWD_X LABEL_X
#define PASSWD_Y (LABEL_Y+15)

#define BUTTON_Y (PASSWD_Y+20)
#define OK_BUTTON_X 30
#define CANCEL_BUTTON_X 100
#define PASS_CANCEL_ID 2001
#define PASS_OK_ID 2002
#define LABEL_ID 2003

CAskPasswdWindow::CAskPasswdWindow( const XP_UCHAR* name, XP_UCHAR* buf, 
				    XP_U16* len, XP_Bool* result )
    : CWindow( PASSWORD_WINDOW_ID, 10, 120, 180, 85, "Password", TRUE )
{
    fName = name;
    fBuf = buf;
    this->lenp = len;
    this->okP = result;

    snprintf( fLabelBuf, sizeof(fLabelBuf), "Password for %s:", name );
    CLabel* label = new CLabel( LABEL_ID, fLabelBuf );
    this->AddChild( label, LABEL_X, LABEL_Y );

    this->entry = new CTextEdit( TEXT_ID, PASSWD_WIDTH, PASSWD_HEIGHT, 
				 TEXTOPTION_PASSWORD 
				 | TEXTOPTION_ONELINE
				 | TEXTOPTION_HAS_FOCUS);
    this->AddChild( this->entry, PASSWD_X, PASSWD_Y );
	this->SetFocus( this->entry );

    CButton* button = new CButton( PASS_CANCEL_ID, 0, 0, "Cancel" );    
    this->AddChild( button, CANCEL_BUTTON_X, BUTTON_Y );
    button = new CButton( PASS_OK_ID, 0, 0, "Ok" );    
    this->AddChild( button, OK_BUTTON_X, BUTTON_Y );
} // CAskWindow

S32
CAskPasswdWindow::MsgHandler( MSG_TYPE type, CViewable *object, S32 data )
{
    S32 result = 0;
    char* text;
    XP_U16 len;

    switch (type) {
    case MSG_BUTTON_SELECT:	// there's only one button....
	switch ( object->GetID() ) {
	case PASS_OK_ID:
	    text = this->entry->GetText();
	    len = this->entry->TextLength();
	    strncpy( (char*)fBuf, text, XP_MIN(len,*this->lenp) );
	    fBuf[len] = '\0';
	    *this->lenp = len;
	    *this->okP = XP_TRUE;
	    break;
	case PASS_CANCEL_ID:
	    *this->okP = XP_FALSE;
	    break;
	default:
	    return 0;
	}
	result = 1;
	this->Close();
	break;
	
    default:
	break;
    }

    return result;
} // CAskLetterWindow::MsgHandler




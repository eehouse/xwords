/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  (based on sample
 * app helloworldbasic "Copyright (c) 2002, Nokia. All rights
 * reserved.")
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

#include <eikenv.h>
#include <ckninfo.h>

#include "xwords.pan"
#include "xwappui.h"
#include "xwappview.h"
#include "xwords.hrh"

// ConstructL is called by the application framework
void CXWordsAppUi::ConstructL()
{
    BaseConstructL();

    iAppView = CXWordsAppView::NewL(ClientRect());    

    AddToStackL(iAppView);
}

CXWordsAppUi::CXWordsAppUi()                              
{
}

CXWordsAppUi::~CXWordsAppUi()
{
    if ( iAppView ) {
        iEikonEnv->RemoveFromStack(iAppView);
        delete iAppView;
        iAppView = NULL;
    }
}

// handle any menu commands
void CXWordsAppUi::HandleCommandL(TInt aCommand) 
{
    switch(aCommand) {

        // built-in commands here
    case EEikCmdExit:
        CBaActiveScheduler::Exit();
        break;

        // added commands here
    default:
        if ( iAppView->HandleCommand( aCommand ) == 0 ) {
            Panic(EXWordsUi);
        }
        break;
    }
} // HandleCommandL

TKeyResponse
CXWordsAppUi::HandleKeyEventL( const TKeyEvent& aKeyEvent,
                                        TEventCode aType )
{
    if ( aType == EEventKey ) {
        TChar chr(aKeyEvent.iCode);
        XP_LOGF( "got iScanCode: %d (%c)", aKeyEvent.iScanCode,
                 aKeyEvent.iScanCode );
        if ( iAppView->HandleKeyEvent( aKeyEvent ) ) {
            return EKeyWasConsumed;
        }
    }

    return EKeyWasNotConsumed;
} /* HandleKeyEventL */

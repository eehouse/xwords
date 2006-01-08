/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  (based on sample
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
#if defined SERIES_60
# include <w32std.h>
# include <eikinfo.h>
#elif defined SERIES_80
# include <ckninfo.h>
# include <eikappui.h>
# include <eikapp.h>
#endif

#include "xwords.pan"
#include "xwappui.h"
#include "xwappview.h"
#include "xwords.hrh"
#include "symutil.h"

// ConstructL is called by the application framework
void CXWordsAppUi::ConstructL()
{
    BaseConstructL();

    iAppView = CXWordsAppView::NewL( ClientRect(), Application() );

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

    /* Somebody on NewLC says this is required to clean up TLS allocated
       via use of stdlib calls  */
    CloseSTDLIB();
}

// handle any menu commands
void CXWordsAppUi::HandleCommandL(TInt aCommand) 
{
    switch(aCommand) {

        // built-in commands here
    case EEikCmdExit:
        iAppView->Exiting();
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
        XP_LOGF( "got iScanCode: %d (%c)", aKeyEvent.iScanCode,
                 aKeyEvent.iScanCode );
        if ( iAppView->HandleKeyEvent( aKeyEvent ) ) {
            return EKeyWasConsumed;
        }
    }

    return EKeyWasNotConsumed;
} /* HandleKeyEventL */

/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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

#include <eikedwin.h>
#include <eikmfne.h> 

#include "symaskdlg.h"
#include "xwords.hrh"

CXWAskDlg::CXWAskDlg( MPFORMAL XWStreamCtxt* aStream, TBool aKillStream ) :
    iStream(aStream), iKillStream(aKillStream), iMessage(NULL)
{
    MPASSIGN( this->mpool, mpool );
}

CXWAskDlg::CXWAskDlg( MPFORMAL TBuf16<128>* aMessage)
    : iMessage(aMessage), iStream(NULL)
{
    MPASSIGN( this->mpool, mpool );
}

CXWAskDlg::~CXWAskDlg()
{
    if ( iKillStream && iStream != NULL ) {
        stream_destroy( iStream );
    }
}

void
CXWAskDlg::PreLayoutDynInitL()
{   
    CEikEdwin* contents = (CEikEdwin*)Control( EAskContents );

    // Load the stream's contents into a read-only edit control.
    if ( iStream ) {
        TInt size = stream_getSize( iStream );
        XP_U16* buf16 = new(ELeave) XP_U16[size];
        CleanupStack::PushL( buf16 );

        char* buf8 = (char*)XP_MALLOC( mpool, size + 1 );
        /* PENDING This belongs on the leave stack */
        User::LeaveIfNull( buf8 );
        stream_getBytes( iStream, buf8, size );
        buf8[size] = '\0';

        TPtrC8 desc8( (const unsigned char*)buf8, size );
        TPtr16 desc16( buf16, size );

        desc16.Copy( desc8 );
        contents->SetTextL( &desc16 );

        CleanupStack::PopAndDestroy( buf16 );

        XP_FREE( mpool, buf8 );
    } else {
        contents->SetTextL( iMessage );
    }
} /* PreLayoutDynInitL */

TBool CXWAskDlg::OkToExitL( TInt aButtonID /* pressed button */ ) 
{
    /* The function should return ETrue if it is OK to exit, and EFalse to
       keep the dialog active. It should always return ETrue if the button
       with ID EEikBidOK was activated. */

    XP_LOGF( "CXWAskDlg::OkToExitL passed %d", aButtonID );

    return ETrue;
}

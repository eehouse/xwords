/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include <charconv.h>

#include "symaskdlg.h"
# include "xwords.hrh"
#ifdef SERIES_60
# include "xwords_60.rsg"
#elif defined SERIES_80
# include "xwords_80.rsg"
#endif

CXWAskDlg::CXWAskDlg( MPFORMAL XWStreamCtxt* aStream, TBool aKillStream ) :
    iStream(aStream), iMessage(NULL), iKillStream(aKillStream)
{
    MPASSIGN( this->mpool, mpool );
}

CXWAskDlg::CXWAskDlg( MPFORMAL TBuf16<128>* aMessage)
    : iStream(NULL), iMessage(aMessage)
{
    MPASSIGN( this->mpool, mpool );
}

CXWAskDlg::~CXWAskDlg()
{
    if ( iKillStream && iStream != NULL ) {
        stream_destroy( iStream );
    }
}

static void
SwapInSymbLinefeed( TDes16& buf16 )
{
    TBuf16<1> lfDescNew;
    lfDescNew.Append( CEditableText::ELineBreak );
    TBuf8<4> tmp( (unsigned char*)XP_CR );
    TBuf16<4> lfDescOld;
    lfDescOld.Copy( tmp );

    XP_LOGF( "starting search-and-replace" );
#if 0
    TInt len = buf16.Length();
    TPtrC16 rightPart = buf16.Right(len);
    TInt leftLen = 0;
    for ( ; ; ) {
        TInt pos = rightPart.Find( lfDescOld );
        if ( pos == KErrNotFound ) {
            break;
        }
        buf16.Replace( leftLen + pos, 1, lfDescNew );
        leftLen += pos;
        len -= pos;
        /* This won't compile.  Need to figure out how to replace without
           starting the search at the beginning each time */
        rightPart = buf16.Right(len);
    }
#else
    for ( ; ; ) {
        TInt pos = buf16.Find( lfDescOld );
        if ( pos == KErrNotFound ) {
            break;
        }
        buf16.Replace( pos, 1, lfDescNew );
    }
#endif
    XP_LOGF( "search-and-replace done" );
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

        unsigned char* buf8 = (unsigned char*)XP_MALLOC( mpool, size );
        /* PENDING This belongs on the leave stack */
        User::LeaveIfNull( buf8 );
        stream_getBytes( iStream, buf8, SC(XP_U16,size) );

        TPtrC8 desc8( buf8, size );
        TPtr16 desc16( buf16, size );
#if 0
        if ( ConvertToDblByteL( desc16, desc8 ) ) {
            contents->SetTextL( &desc16 );
        }
#else
        desc16.Copy( desc8 );
        SwapInSymbLinefeed( desc16 );
        contents->SetTextL( &desc16 );
#endif
        XP_FREE( mpool, buf8 );
        CleanupStack::PopAndDestroy( buf16 );
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

/* static */ TBool
CXWAskDlg::DoAskDlg( MPFORMAL XWStreamCtxt* aStream, TBool aKillStream )
{
    CXWAskDlg* me = new(ELeave)CXWAskDlg( MPPARM(mpool) aStream, aKillStream );
    return me->ExecuteLD( R_XWORDS_CONFIRMATION_QUERY );
}

/* static */ TBool
CXWAskDlg::DoAskDlg( MPFORMAL TBuf16<128>* aMessage )
{
    CXWAskDlg* me = new(ELeave)CXWAskDlg( MPPARM(mpool) aMessage );
    return me->ExecuteLD( R_XWORDS_CONFIRMATION_QUERY );
}

/* static */ void
CXWAskDlg::DoInfoDlg( MPFORMAL XWStreamCtxt* aStream, TBool aKillStream )
{
    CXWAskDlg* me = new(ELeave)CXWAskDlg( MPPARM(mpool) aStream, aKillStream );
    (void)me->ExecuteLD( R_XWORDS_INFO_ONLY );
}


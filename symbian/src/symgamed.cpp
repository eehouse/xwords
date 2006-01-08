/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).
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

#if defined SERIES_80
# include <eikfsel.h>
# include "xwords_80.rsg"
#elif defined SERIES_60
# include "xwords_60.rsg"
#endif
#include "xwords.hrh"
#include "symgamed.h"


CNameEditDlg::CNameEditDlg( TGameName* aGameName )
    : iGameName( aGameName )
{
}

TBool 
CNameEditDlg::OkToExitL( TInt aKeyCode )
{
#ifdef SERIES_80
    CEikFileNameEditor* ed;
    ed = static_cast<CEikFileNameEditor*>(Control(EEditNameEdwin));

    if ( aKeyCode != EEikBidCancel ) {
        TInt len = ed->TextLength();
        XP_ASSERT( len <= iGameName->MaxLength() );
        TGameName tmp;
        ed->GetText( tmp );
        iGameName->Copy( tmp );
    }
#endif
    return ETrue;
} // OkToExitL

void
CNameEditDlg::PreLayoutDynInitL()
{
#ifdef SERIES_80
    CEikFileNameEditor* ed;
    ed = static_cast<CEikFileNameEditor*>(Control(EEditNameEdwin));
    ed->SetTextL( iGameName );
    ed->SetTextLimit( iGameName->MaxLength() );
#endif
}

/* static */ TBool
CNameEditDlg::EditName( TGameName* aGameName )
{
    CNameEditDlg* me = new CNameEditDlg( aGameName );
    User::LeaveIfNull( me );

    return me->ExecuteLD( R_XWORDS_EDITNAME_DLG );
}

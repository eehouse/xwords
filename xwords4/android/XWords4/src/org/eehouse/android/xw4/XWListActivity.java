/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package org.eehouse.android.xw4;

import android.app.ListActivity;
import android.app.Dialog;
import android.os.Bundle;

public class XWListActivity extends ListActivity {

    private static int DLG_SOMETIMES = 100;

    @Override
    protected void onStart()
    {
        Utils.logf( "XWListActivity::onStart" );
        super.onStart();
        DispatchNotify.SetRunning( this );
    }

    @Override
    protected void onStop()
    {
        Utils.logf( "XWListActivity::onStop" );
        super.onStop();
        DispatchNotify.ClearRunning( this );
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = XWActivity.onCreateDialog( this, id );
        if ( null == dialog ) {
            dialog = super.onCreateDialog( id );
        }
        return dialog;
    }

    protected void setDialogBundle( Bundle bundle )
    {
        XWActivity.setDialogBundle( bundle );
    }
}

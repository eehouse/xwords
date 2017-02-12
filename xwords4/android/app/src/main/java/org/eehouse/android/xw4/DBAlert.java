/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
/*
 * Copyright 2017 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.support.v4.app.DialogFragment;
import android.os.Bundle;
import android.app.Dialog;

import java.io.Serializable;

import junit.framework.Assert;

public class DBAlert extends DialogFragment {
    private static final String DLG_ID_KEY = "DLG_ID_KEY";
    private static final String PARMS_KEY = "PARMS_KEY";

    private Object[] mParams;
    private DlgID mDlgID;
    
    public static DBAlert newInstance( DlgID dlgID, Object[] params )
    {
        if ( BuildConfig.DEBUG ) {
            for ( Object obj : params ) {
                Assert.assertTrue( obj instanceof Serializable );
            }
        }
        
        Bundle bundle = new Bundle();
        bundle.putInt( DLG_ID_KEY, dlgID.ordinal() );
        bundle.putSerializable( PARMS_KEY, params );
        DBAlert result = new DBAlert();
        result.setArguments( bundle );
        return result;
    }

    public DBAlert() {}

    @Override
    public void onSaveInstanceState( Bundle bundle )
    {
        super.onSaveInstanceState( bundle );
        bundle.putInt( DLG_ID_KEY, mDlgID.ordinal() );
        bundle.putSerializable( PARMS_KEY, mParams );
    }
    
    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        if ( null == sis ) {
            sis = getArguments();
        }
        mDlgID = DlgID.values()[sis.getInt(DLG_ID_KEY, -1)];
        mParams = (Object[])sis.getSerializable(PARMS_KEY);
        
        XWActivity activity = (XWActivity)getActivity();
        return activity.makeDialog( mDlgID, mParams );
    }
}

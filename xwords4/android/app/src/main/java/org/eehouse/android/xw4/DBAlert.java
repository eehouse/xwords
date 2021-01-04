/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;

import org.eehouse.android.xw4.loc.LocUtils;

import java.io.Serializable;

public class DBAlert extends XWDialogFragment {
    private static final String TAG = DBAlert.class.getSimpleName();
    private static final String DLG_ID_KEY = "DLG_ID_KEY";
    private static final String PARMS_KEY = "PARMS_KEY";

    private Object[] mParams;
    private DlgID mDlgID;
    public static DBAlert newInstance( DlgID dlgID, Object[] params )
    {
        if ( BuildConfig.DEBUG ) {
            for ( Object obj : params ) {
                if ( null != obj && !(obj instanceof Serializable) ) {
                    Log.d( TAG, "OOPS: %s not Serializable",
                           obj.getClass().getName() );
                    Assert.failDbg();
                }
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

    public DlgID getDlgID() {
        if ( null == mDlgID ) {
            mDlgID = DlgID.values()[getArguments().getInt(DLG_ID_KEY, -1)];
        }
        return mDlgID;
    }

    @Override
    public boolean belongsOnBackStack()
    {
        boolean result = getDlgID().belongsOnBackStack();
        return result;
    }

    @Override
    public String getFragTag()
    {
        return getDlgID().toString();
    }

    @Override
    public void onSaveInstanceState( Bundle bundle )
    {
        bundle.putInt( DLG_ID_KEY, getDlgID().ordinal() );
        bundle.putSerializable( PARMS_KEY, mParams );
        super.onSaveInstanceState( bundle );
    }
    
    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        if ( null == sis ) {
            sis = getArguments();
        }
        mParams = (Object[])sis.getSerializable(PARMS_KEY);
        
        XWActivity activity = (XWActivity)getActivity();
        Dialog dialog = activity.makeDialog( this, mParams );

        if ( null == dialog ) {
            Log.e( TAG, "no dialog for %s from %s", getDlgID(), activity );
            // Assert.failDbg();   // remove: better to see what users will see
            dialog = LocUtils.makeAlertBuilder( activity )
                .setMessage( "Unable to create " + getDlgID() + " Alert" )
                .setPositiveButton( android.R.string.ok, null )
                .setNegativeButton( "Try again", new OnClickListener() {
                        @Override
                        public void onClick( DialogInterface dlg, int button ) {
                            DBAlert alrt = newInstance( mDlgID, mParams );
                            ((MainActivity)getActivity()).show( alrt );
                        }
                    })
                .create();
        }
        return dialog;
    }
}

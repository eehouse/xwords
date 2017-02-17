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

import android.app.Dialog;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.DialogFragment;

import org.eehouse.android.xw4.loc.LocUtils;

import java.io.Serializable;

import junit.framework.Assert;

public class DBAlert extends DialogFragment {
    private static final String TAG = DBAlert.class.getSimpleName();
    private static final String DLG_ID_KEY = "DLG_ID_KEY";
    private static final String PARMS_KEY = "PARMS_KEY";

    public interface OnDismissListener {
        void onDismissed();
    }

    private Object[] mParams;
    private DlgID mDlgID;
    private OnDismissListener m_onDismiss;
    
    public static DBAlert newInstance( DlgID dlgID, Object[] params )
    {
        if ( BuildConfig.DEBUG ) {
            for ( Object obj : params ) {
                if ( null != obj && !(obj instanceof Serializable) ) {
                    DbgUtils.logd( TAG, "OOPS: %s not Serializable",
                                   obj.getClass().getName() );
                    // Assert.fail();
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

    public DlgID getDlgID() { return mDlgID; }

    @Override
    public void onSaveInstanceState( Bundle bundle )
    {
        super.onSaveInstanceState( bundle );
        bundle.putInt( DLG_ID_KEY, mDlgID.ordinal() );
        bundle.putSerializable( PARMS_KEY, mParams );
    }
    
    @Override
    public void onDismiss( DialogInterface dif )
    {
        if ( null != m_onDismiss ) {
            m_onDismiss.onDismissed();
        }
        super.onDismiss( dif );
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
        Dialog dialog = activity.makeDialog( this, mParams );

        if ( null == dialog ) {
            dialog = LocUtils.makeAlertBuilder( getActivity() )
                .setTitle( "Stub Alert" )
                .setMessage( String.format( "Unable to create for %s", mDlgID.toString() ) )
                .setPositiveButton( "Bummer", null )
                // .setNegativeButton( "Try now", new OnClickListener() {
                //         @Override
                //         public void onClick( DialogInterface dlg, int button ) {
                //             DBAlert alrt = newInstance( mDlgID, mParams );
                //             ((MainActivity)getActivity()).show( alrt );
                //         }
                //     })
                .create();

            new Handler().post( new Runnable() {
                    @Override
                    public void run() {
                        DBAlert newMe = newInstance( mDlgID, mParams );
                        ((MainActivity)getActivity()).show( newMe );

                        dismiss();          // kill myself...
                    }
                } );
        }
        return dialog;
    }

    protected void setOnDismissListener( OnDismissListener lstnr )
    {
        m_onDismiss = lstnr;
    }
}
/* -*- compile-command: "find-and-gradle.sh inXw4dDebug"; -*- */
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

import android.app.AlertDialog;
import android.content.DialogInterface;
import androidx.fragment.app.DialogFragment;
import android.view.View.OnClickListener;
import android.view.View;

import java.util.HashMap;
import java.util.Map;


abstract public class XWDialogFragment extends DialogFragment {
    private static final String TAG = XWDialogFragment.class.getSimpleName();

    private OnDismissListener m_onDismiss;
    private OnCancelListener m_onCancel;
    private Map<Integer, DialogInterface.OnClickListener> m_buttonMap;

    public interface OnDismissListener {
        void onDismissed( XWDialogFragment frag );
    }
    public interface OnCancelListener {
        void onCancelled( XWDialogFragment frag );
    }

    abstract String getFragTag();

    @Override
    public void onResume()
    {
        super.onResume();

        if ( null != m_buttonMap ) {
            AlertDialog dialog = (AlertDialog)getDialog();
            if ( null != dialog) {
                for ( final int but : m_buttonMap.keySet() ) {
                    // final int fbut = but;
                    dialog.getButton( but ) // NPE!!!
                        .setOnClickListener( new OnClickListener() {
                                @Override
                                public void onClick( View view ) {
                                    dialogButtonClicked( view, but );
                                }
                            } );
                }
            }
        }
    }

    @Override
    public void onCancel( DialogInterface dialog )
    {
        super.onCancel( dialog );
        // Log.d( TAG, "%s.onCancel() called", getClass().getSimpleName() );
        if ( null != m_onCancel ) {
            m_onCancel.onCancelled( this );
        }
    }

    @Override
    public void onDismiss( DialogInterface dif )
    {
        // Log.d( TAG, "%s.onDismiss() called", getClass().getSimpleName() );
        super.onDismiss( dif );

        if ( null != m_onDismiss ) {
            m_onDismiss.onDismissed( this );
        }
    }

    public boolean belongsOnBackStack() { return false; }

    protected void setOnDismissListener( OnDismissListener lstnr )
    {
        Assert.assertTrue( null == lstnr || null == m_onDismiss || !BuildConfig.DEBUG );
        m_onDismiss = lstnr;
    }

    protected void setOnCancelListener( OnCancelListener lstnr )
    {
        Assert.assertTrue( null == lstnr || null == m_onCancel || !BuildConfig.DEBUG );
        m_onCancel = lstnr;
    }

    protected void setNoDismissListenerPos( AlertDialog.Builder ab, int buttonID,
                                            DialogInterface.OnClickListener lstnr )
    {
        ab.setPositiveButton( buttonID, null );
        getButtonMap().put( AlertDialog.BUTTON_POSITIVE, lstnr );
    }

    protected void setNoDismissListenerNeut( AlertDialog.Builder ab, int buttonID,
                                             DialogInterface.OnClickListener lstnr )
    {
        ab.setNeutralButton( buttonID, null );
        getButtonMap().put( AlertDialog.BUTTON_NEUTRAL, lstnr );
    }

    protected void setNoDismissListenerNeg( AlertDialog.Builder ab, int buttonID,
                                            DialogInterface.OnClickListener lstnr )
    {
        ab.setNegativeButton( buttonID, null );
        getButtonMap().put( AlertDialog.BUTTON_NEGATIVE, lstnr );
    }

    private Map<Integer, DialogInterface.OnClickListener> getButtonMap()
    {
        if ( null == m_buttonMap ) {
            m_buttonMap = new HashMap<>();
        }
        return m_buttonMap;
    }

    private void dialogButtonClicked( View view, int button )
    {
        DialogInterface.OnClickListener listener = m_buttonMap.get( button );
        if ( null != listener ) {
            listener.onClick( getDialog(), button );
        } else {
            Assert.failDbg();
        }
    }
}

/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.preference.CheckBoxPreference;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Spinner;

public class SMSCheckBoxPreference extends CheckBoxPreference {

    private PrefsActivity m_activity;
    private boolean m_attached = false;
    protected static SMSCheckBoxPreference s_this = null;

    public SMSCheckBoxPreference( Context context, AttributeSet attrs )
    {
        super( context, attrs );
        s_this = this;
        m_activity = (PrefsActivity)context;
    }

    @Override
    protected void onAttachedToActivity()
    {
        super.onAttachedToActivity();
        if ( !XWApp.SMSSUPPORTED || !Utils.deviceSupportsSMS( m_activity ) ) {
            setEnabled( false );
        }
        m_attached = true;
    }

    @Override
    public void setChecked( boolean checked )
    {
        if ( !checked || !m_attached ) {
            super.setChecked( false );
        } else {
            m_activity.showDialog( PrefsActivity.CONFIRM_SMS );
        }
    }

    // Because s_this.super.setChecked() isn't allowed...
    private void super_setChecked( boolean checked )
    {
        super.setChecked( checked );
    }

    public static Dialog onCreateDialog( final Activity activity, final int id )
    {
        final View layout = Utils.inflate( activity, R.layout.confirm_sms );

        DialogInterface.OnClickListener lstnr = 
            new DialogInterface.OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    Spinner reasons = (Spinner)
                        layout.findViewById( R.id.confirm_sms_reasons );
                    if ( 0 < reasons.getSelectedItemPosition() ) {
                        s_this.super_setChecked( true );
                    }
                }
            };

        AlertDialog.Builder ab = new AlertDialog.Builder( activity )
            .setTitle( R.string.confirm_sms_title )
            .setView( layout )
            .setNegativeButton( R.string.button_ok, lstnr );
        Dialog dialog = ab.create();

        dialog.setOnDismissListener( new DialogInterface.OnDismissListener() {
                public void onDismiss( DialogInterface di ) {
                    activity.removeDialog( id );
                }
            } );

        return dialog;
    }
}
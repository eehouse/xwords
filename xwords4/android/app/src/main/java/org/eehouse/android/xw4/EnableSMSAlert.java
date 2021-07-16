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
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.Spinner;


import org.eehouse.android.xw4.loc.LocUtils;

public class EnableSMSAlert extends DlgDelegateAlert {
    private Spinner mSpinner;

    public static EnableSMSAlert newInstance( DlgState state )
    {
        EnableSMSAlert result = new EnableSMSAlert();
        result.addStateArgument( state );
        return result;
    }

    public EnableSMSAlert() {}

    @Override
    public void populateBuilder( Context context, final DlgState state,
                                 AlertDialog.Builder builder )
    {
        View layout = LocUtils.inflate( context, R.layout.confirm_sms );
        mSpinner = (Spinner)layout.findViewById( R.id.confirm_sms_reasons );

        OnItemSelectedListener onItemSel = new Utils.OnNothingSelDoesNothing() {
                @Override
                public void onItemSelected( AdapterView<?> parent, View view,
                                            int position, long id ) {
                    checkEnableButton( (AlertDialog)getDialog() );
                }
            };
        mSpinner.setOnItemSelectedListener( onItemSel );

        DialogInterface.OnClickListener lstnr =
            new DialogInterface.OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    Assert.assertTrue( 0 < mSpinner.getSelectedItemPosition() );
                    XWActivity xwact = (XWActivity)getActivity();
                    xwact.onPosButton( state.m_action, state.getParams() );
                }
            };

        builder.setTitle( R.string.confirm_sms_title )
            .setView( layout )
            .setPositiveButton( R.string.button_enable, lstnr )
            .setNegativeButton( android.R.string.cancel, null )
            ;
    }

    @Override
    AlertDialog create( AlertDialog.Builder builder )
    {
        AlertDialog dialog = super.create( builder );
        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
                @Override
                public void onShow( DialogInterface dialog ) {
                    checkEnableButton( (AlertDialog)dialog );
                }
            });

        return dialog;
    }

    private void checkEnableButton( AlertDialog dialog )
    {
        boolean enabled = 0 < mSpinner.getSelectedItemPosition();
        Utils.enableAlertButton( dialog, AlertDialog.BUTTON_POSITIVE, enabled );
    }
}

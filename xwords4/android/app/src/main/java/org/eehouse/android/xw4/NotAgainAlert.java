/* -*- compile-command: "find-and-gradle.sh insXw4Debug"; -*- */
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
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

public class NotAgainAlert extends DlgDelegateAlert {
    private static final String TAG = NotAgainAlert.class.getSimpleName();

    public static NotAgainAlert newInstance( DlgState state )
    {
        return new NotAgainAlert( state );
    }

    public NotAgainAlert( DlgState state )
    {
        super( state );
    }

    public NotAgainAlert() {}

    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        final Context context = getActivity();

        getBundleData( sis );
        final DlgState state = getState();

        final NotAgainView naView = (NotAgainView)
            LocUtils.inflate( context, R.layout.not_again_view );
        naView.setMessage( state.m_msg );
        // final OnClickListener lstnr_p = mkCallbackClickListener( state, naView );

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( context )
            .setTitle( R.string.newbie_title )
            .setView( naView )
            .setPositiveButton( android.R.string.ok,
                                mkCallbackClickListener( naView ) );

        if ( null != state.m_pair ) {
            final ActionPair more = state.m_pair;
            OnClickListener lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        checkNotAgainCheck( state, naView );
                        // m_clickCallback.onPosButton( more.action, more.params );
                    }
                };
            builder.setNegativeButton( more.buttonStr, lstnr );
        }

        Dialog dialog = builder.create();
        return dialog;
    }

}

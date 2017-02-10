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
import android.widget.Button;

import java.util.ArrayList;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans;
import org.eehouse.android.xw4.DlgDelegate.NotAgainBuilder;
import org.eehouse.android.xw4.DlgDelegate.ConfirmThenBuilder;
import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;

public class InviteChoicesAlert extends DlgDelegateAlert {

    public static InviteChoicesAlert newInstance( DlgState state )
    {
        InviteChoicesAlert result = new InviteChoicesAlert();
        result.addStateArgument( state );
        return result;
    }
    
    public InviteChoicesAlert() {}

    @Override
    public Dialog onCreateDialog( Bundle sis )
    {
        final Context context = getActivity();

        final DlgState state = getState( sis );

        final ArrayList<InviteMeans> means =
            new ArrayList<InviteMeans>();
        ArrayList<String> items = new ArrayList<String>();
        InviteMeans lastMeans = null;
        if ( null != state.m_params
             && state.m_params[0] instanceof SentInvitesInfo ) {
            lastMeans = ((SentInvitesInfo)state.m_params[0]).getLastMeans();
        }

        if ( XWApp.SMS_INVITE_ENABLED && Utils.deviceSupportsSMS(context) ) {
            items.add( getString( R.string.invite_choice_sms ) );
            means.add( InviteMeans.SMS );
        }
        items.add( getString( R.string.invite_choice_email ) );
        means.add( InviteMeans.EMAIL );
        if ( BTService.BTAvailable() ) {
            items.add( getString( R.string.invite_choice_bt ) );
            means.add( InviteMeans.BLUETOOTH );
        }
        if ( XWApp.RELAYINVITE_SUPPORTED ) {
            items.add( getString( R.string.invite_choice_relay ) );
            means.add( InviteMeans.RELAY );
        }
        if ( WiDirService.enabled() ) {
            items.add( getString( R.string.invite_choice_p2p ) );
            means.add( InviteMeans.WIFIDIRECT );
        }
        if ( XWPrefs.getNFCToSelfEnabled( context )
             || NFCUtils.nfcAvail( context )[0] ) {
            items.add( getString( R.string.invite_choice_nfc ) );
            means.add( InviteMeans.NFC );
        }
        items.add( getString( R.string.slmenu_copy_sel ) );
        means.add( InviteMeans.CLIPBOARD );

        final int[] sel = { -1 };
        if ( null != lastMeans ) {
            for ( int ii = 0; ii < means.size(); ++ii ) {
                if ( lastMeans == means.get(ii) ) {
                    sel[0] = ii;
                    break;
                }
            }
        }

        OnClickListener selChanged = new OnClickListener() {
                public void onClick( DialogInterface dlg, int view ) {
                    XWActivity activity = (XWActivity)getActivity();
                    sel[0] = view;
                    switch ( means.get(view) ) {
                    case CLIPBOARD:
                        String msg =
                            getString( R.string.not_again_clip_expl_fmt,
                                       getString(R.string.slmenu_copy_sel) );
                        activity
                            .makeNotAgainBuilder(msg, R.string.key_na_clip_expl)
                            .show();
                        break;
                    case SMS:
                        if ( ! XWPrefs.getSMSEnabled( context ) ) {
                            activity
                                .makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                                         Action.ENABLE_SMS_ASK )
                                .setPosButton( R.string.button_enable_sms )
                                .setNegButton( R.string.button_later )
                                .show();
                        }
                        break;
                    }
                    Button button = ((AlertDialog)dlg)
                        .getButton( AlertDialog.BUTTON_POSITIVE );
                    button.setEnabled( true );
                }
            };
        OnClickListener okClicked = new OnClickListener() {
                public void onClick( DialogInterface dlg, int view ) {
                    Assert.assertTrue( Action.SKIP_CALLBACK != state.m_action );
                    int indx = sel[0];
                    if ( 0 <= indx ) {
                        XWActivity activity = (XWActivity)context;
                        activity.inviteChoiceMade( state.m_action,
                                                   means.get(indx),
                                                   state.m_params );
                    }
                }
            };

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( context )
            .setTitle( R.string.invite_choice_title )
            .setSingleChoiceItems( items.toArray( new String[items.size()] ),
                                   sel[0], selChanged )
            .setPositiveButton( android.R.string.ok, okClicked )
            .setNegativeButton( android.R.string.cancel, null );

        return builder.create();
    }
}

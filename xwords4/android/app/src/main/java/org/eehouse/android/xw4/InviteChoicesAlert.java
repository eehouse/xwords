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
import java.util.List;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.DlgDelegate.ConfirmThenBuilder;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans;
import org.eehouse.android.xw4.DlgDelegate.NotAgainBuilder;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.loc.LocUtils;

public class InviteChoicesAlert extends DlgDelegateAlert {

    public static InviteChoicesAlert newInstance( DlgState state )
    {
        InviteChoicesAlert result = new InviteChoicesAlert();
        result.addStateArgument( state );
        return result;
    }
    
    public InviteChoicesAlert() {}

    @Override
    public void populateBuilder( final Context context, final DlgState state,
                                 AlertDialog.Builder builder,
                                 NotAgainView naView )
    {
        final ArrayList<InviteMeans> means =
            new ArrayList<InviteMeans>();
        ArrayList<String> items = new ArrayList<String>();
        InviteMeans lastMeans = null;
        if ( null != state.m_params
             && state.m_params[0] instanceof SentInvitesInfo ) {
            lastMeans = ((SentInvitesInfo)state.m_params[0]).getLastMeans();
        }

        add( items, means, R.string.invite_choice_email, InviteMeans.EMAIL );
        add( items, means, R.string.invite_choice_user_sms, InviteMeans.SMS_USER );

        if ( BTService.BTAvailable() ) {
            add( items, means, R.string.invite_choice_bt, InviteMeans.BLUETOOTH );
        }
        if ( Utils.deviceSupportsNBS(context) ) {
            add( items, means, R.string.invite_choice_data_sms, InviteMeans.SMS_DATA );
        }
        if ( BuildConfig.RELAYINVITE_SUPPORTED ) {
            add( items, means, R.string.invite_choice_relay, InviteMeans.RELAY );
        }
        if ( WiDirWrapper.enabled() ) {
            add( items, means, R.string.invite_choice_p2p, InviteMeans.WIFIDIRECT );
        }
        if ( XWPrefs.getNFCToSelfEnabled( context ) || NFCUtils.nfcAvail( context )[0] ) {
            add( items, means, R.string.invite_choice_nfc, InviteMeans.NFC );
        }
        add( items, means, R.string.slmenu_copy_sel, InviteMeans.CLIPBOARD );

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
                public void onClick( DialogInterface dlg, int pos ) {
                    XWActivity activity = (XWActivity)getActivity();
                    sel[0] = pos;
                    switch ( means.get(pos) ) {
                    case SMS_USER:
                        activity
                            .makeNotAgainBuilder( R.string.sms_invite_flakey,
                                                  R.string.key_na_sms_invite_flakey )
                            .show();
                        break;
                    case CLIPBOARD:
                        String msg =
                            getString( R.string.not_again_clip_expl_fmt,
                                       getString(R.string.slmenu_copy_sel) );
                        activity
                            .makeNotAgainBuilder(msg, R.string.key_na_clip_expl)
                            .show();
                        break;
                    case SMS_DATA:
                        if ( !Perms23.havePermissions( activity, Perm.SEND_SMS, Perm.RECEIVE_SMS )
                             && Perm.SEND_SMS.isBanned(activity) ) {
                            activity
                                .makeOkOnlyBuilder( R.string.sms_banned_ok_only )
                                .setActionPair(new ActionPair( Action.PERMS_BANNED_INFO,
                                                               R.string.button_more_info ) )
                                .show();
                        } else if ( ! XWPrefs.getNBSEnabled( context ) ) {
                            activity
                                .makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                                         Action.ENABLE_NBS_ASK )
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

        final OnClickListener okClicked = new OnClickListener() {
                @Override
                public void onClick( DialogInterface dlg, int pos ) {
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

        builder.setTitle( R.string.invite_choice_title )
            .setSingleChoiceItems( items.toArray( new String[items.size()] ),
                                   sel[0], selChanged )
            .setPositiveButton( android.R.string.ok, okClicked )
            .setNegativeButton( android.R.string.cancel, null );
        if ( BuildConfig.DEBUG ) {
            OnClickListener ocl = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int pos ) {
                        if ( state.m_params[0] instanceof SentInvitesInfo ) {
                            SentInvitesInfo sii = (SentInvitesInfo)
                                state.m_params[0];
                            sii.setRemotesRobots();
                        }
                        okClicked.onClick( dlg, pos );
                    }
                };
            builder.setNeutralButton( R.string.ok_with_robots, ocl );
        }
    }

    private void add( List<String> items, List<InviteMeans> means,
                      int resID, InviteMeans oneMeans )
    {
        items.add( getString( resID ) );
        means.add( oneMeans );
    }
}

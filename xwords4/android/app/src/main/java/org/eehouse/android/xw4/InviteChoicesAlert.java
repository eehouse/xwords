/* -*- compile-command: "find-and-gradle.sh inXw4dDebug"; -*- */
/*
 * Copyright 2017 - 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.widget.Button;
import android.widget.RadioGroup;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class InviteChoicesAlert extends DlgDelegateAlert
    implements InviteView.ItemClicked  {
    private static final String TAG = InviteChoicesAlert.class.getSimpleName();

    private static WeakReference<InviteChoicesAlert> sSelf;

    private Button mPosButton;
    private InviteView mInviteView;

    public static InviteChoicesAlert newInstance( DlgState state )
    {
        InviteChoicesAlert result = new InviteChoicesAlert();
        result.addStateArgument( state );
        sSelf = new WeakReference<>(result);
        return result;
    }

    public static void dismissAny()
    {
        WeakReference<InviteChoicesAlert> ref = sSelf;
        if ( null != ref ) {
            InviteChoicesAlert self = ref.get();
            if ( null != self ) {
                self.dismiss();
            }
        }
    }
    
    public InviteChoicesAlert() {}

    @Override
    public void onDestroy()
    {
        sSelf = null;
        super.onDestroy();
    }

    @Override
    public void populateBuilder( final Context context, final DlgState state,
                                 AlertDialog.Builder builder )
    {
        ArrayList<InviteMeans> means = new ArrayList<>();
        InviteMeans lastMeans = null;
        NetLaunchInfo nli = null;
        Object[] params = state.getParams();
        if ( null != params ) {
            if ( 0 < params.length && params[0] instanceof SentInvitesInfo ) {
                lastMeans = ((SentInvitesInfo)params[0]).getLastMeans();
            }
            if ( 1 < params.length && params[1] instanceof NetLaunchInfo ) {
                nli = (NetLaunchInfo)params[1];
            }
        }
        means.add( InviteMeans.EMAIL );
        means.add( InviteMeans.SMS_USER );

        if ( Utils.deviceSupportsNBS(context) ) {
            means.add( InviteMeans.SMS_DATA );
        }
        if ( BTUtils.BTAvailable() ) {
            means.add( InviteMeans.BLUETOOTH );
        }
        if ( WiDirWrapper.enabled() ) {
            means.add( InviteMeans.WIFIDIRECT );
        }
        if ( NFCUtils.nfcAvail( context )[0] ) {
            means.add( InviteMeans.NFC );
        }
        means.add( InviteMeans.CLIPBOARD );
        means.add( InviteMeans.QRCODE );

        int lastSelMeans = -1;
        if ( null != lastMeans ) {
            for ( int ii = 0; ii < means.size(); ++ii ) {
                if ( lastMeans == means.get(ii) ) {
                    lastSelMeans = ii;
                    break;
                }
            }
        }

        mInviteView = (InviteView)LocUtils
            .inflate( context, R.layout.invite_view );
        final OnClickListener okClicked = new OnClickListener() {
                @Override
                public void onClick( DialogInterface dlg, int pos ) {
                    Assert.assertTrue( Action.SKIP_CALLBACK != state.m_action );
                    Object choice = mInviteView.getChoice();
                    if ( null != choice ) {
                        XWActivity activity = (XWActivity)context;
                        if ( choice instanceof InviteMeans ) {
                            InviteMeans means = (InviteMeans)choice;
                            activity.inviteChoiceMade( state.m_action,
                                                       means, state.getParams() );
                        } else if ( choice instanceof String ) {
                            String player = (String)choice;
                            CommsAddrRec addr = XwJNI.kplr_getAddr( player );
                            XWActivity xwact = (XWActivity)context;
                            Object[] params = { addr };
                            xwact.onPosButton( state.m_action, params );
                        } else {
                            Assert.failDbg();
                        }
                    }
                }
            };

        builder
            .setTitle( R.string.invite_choice_title )
            .setView( mInviteView )
            .setPositiveButton( android.R.string.ok, okClicked )
            .setNegativeButton( android.R.string.cancel, null )
            ;

        String[] players = XwJNI.kplr_getPlayers();
        mInviteView.setChoices( means, lastSelMeans, players )
            .setNli( nli )
            .setCallbacks( this )
            ;

        // if ( BuildConfig.DEBUG ) {
        //     OnClickListener ocl = new OnClickListener() {
        //             @Override
        //             public void onClick( DialogInterface dlg, int pos ) {
        //                 Object[] params = state.getParams();
        //                 if ( params[0] instanceof SentInvitesInfo ) {
        //                     SentInvitesInfo sii = (SentInvitesInfo)params[0];
        //                     sii.setRemotesRobots();
        //                 }
        //                 okClicked.onClick( dlg, pos );
        //             }
        //         };
        //     builder.setNeutralButton( R.string.ok_with_robots, ocl );
        // }
    }

    @Override
    Dialog create( AlertDialog.Builder builder )
    {
        Dialog dialog = super.create( builder );
        dialog.setOnShowListener(new DialogInterface.OnShowListener() {
                @Override
                public void onShow( DialogInterface diface ) {
                    mPosButton = ((AlertDialog)diface)
                        .getButton( AlertDialog.BUTTON_POSITIVE );
                    enableOkButton();
                }
            });

        return dialog;
    }

    @Override
    public void meansClicked( InviteMeans means )
    {
        DlgDelegate.Builder builder = null;
        XWActivity activity = (XWActivity)getActivity();

        switch ( means ) {
        case SMS_USER:
            builder = activity
                .makeNotAgainBuilder( R.string.sms_invite_flakey,
                                      R.string.key_na_sms_invite_flakey );
            break;
        case CLIPBOARD:
            String msg =
                getString( R.string.not_again_clip_expl_fmt,
                           getString(R.string.slmenu_copy_sel) );
            builder = activity
                .makeNotAgainBuilder(msg, R.string.key_na_clip_expl);
            break;
        case QRCODE:
            builder = activity
                .makeNotAgainBuilder( R.string.qrcode_invite_expl,
                                      R.string.key_na_qrcode_invite );
            break;
        case SMS_DATA:
            if ( !Perms23.havePermissions( activity, Perm.SEND_SMS, Perm.RECEIVE_SMS )
                 && Perm.SEND_SMS.isBanned(activity) ) {
                builder = activity
                    .makeOkOnlyBuilder( R.string.sms_banned_ok_only )
                    .setActionPair( Action.PERMS_BANNED_INFO,
                                    R.string.button_more_info )
                    ;
            } else if ( ! XWPrefs.getNBSEnabled( getContext() ) ) {
                builder = activity
                    .makeConfirmThenBuilder( R.string.warn_sms_disabled,
                                             Action.ENABLE_NBS_ASK )
                    .setPosButton( R.string.button_enable_sms )
                    .setNegButton( R.string.button_later )
                    ;
            }
            break;
        }

        if ( null != builder ) {
            builder.show();
        }
    }

    @Override
    public void checkButton()
    {
        enableOkButton();
    }

    private void enableOkButton()
    {
        if ( null != mPosButton ) {
            mPosButton.setEnabled( null != mInviteView.getChoice() );
        }
    }
}

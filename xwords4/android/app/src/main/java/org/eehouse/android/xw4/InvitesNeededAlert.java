/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2020 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import java.io.Serializable;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.loc.LocUtils;

class InvitesNeededAlert implements DialogInterface.OnDismissListener {
    private static final String TAG = InvitesNeededAlert.class.getSimpleName();

    private static InvitesNeededAlert[] sInstance = {null};

    private DBAlert mAlert;
    private State mState;
    private Callbacks mCallbacks;
    private DelegateBase mDelegate;

    private static class State implements Serializable {
        int nDevsSeen;
        int nPlayersMissing;
        boolean isRematch;

        State( int nDevs, int nPlayers, boolean rematch )
        {
            nDevsSeen = nDevs;
            nPlayersMissing = nPlayers;
            isRematch = rematch;
        }
    }

    interface Callbacks {
        DelegateBase getDelegate();
        long getRowID();
        void onCloseClicked();
        void onInviteClicked();
        void onInfoClicked();
    }

    static void showOrHide( Callbacks callbacks, int nDevsSeen,
                            int nPlayersMissing, boolean isRematch )
    {
        DbgUtils.assertOnUIThread();
        InvitesNeededAlert self = sInstance[0];
        Log.d( TAG, "showOnceIf(nDevsSeen=%d, nPlayersMissing=%d); self: %s",
               nDevsSeen, nPlayersMissing, self );

        if ( null == self && 0 == nPlayersMissing ) {
            // cool: need and have nothing, so do nothing
        } else if ( 0 < nPlayersMissing && null == self ) { // Need but don't have
            makeNew( callbacks, nDevsSeen, nPlayersMissing, isRematch );
        } else if ( 0 == nPlayersMissing && null != self ) { // Have and need to close
            close( self );
        } else if ( null != self && nPlayersMissing != self.mState.nPlayersMissing ) {
            close( self );
            makeNew( callbacks, nDevsSeen, nPlayersMissing, isRematch );
        } else if ( null != self && nPlayersMissing == self.mState.nPlayersMissing ) {
            // nothing to do
        } else {
            Assert.failDbg();
        }
    }

    static Dialog make( Callbacks callbacks, DBAlert alert, Object[] params )
    {
        DbgUtils.assertOnUIThread();
        InvitesNeededAlert self = sInstance[0];
        return self.makeImpl( callbacks, alert, params );
    }

    static void dismiss()
    {
        Log.d( TAG, "dismiss()" );
        DbgUtils.assertOnUIThread();
        InvitesNeededAlert self = sInstance[0];
        if ( null != self ) {
            close( self );
        }
    }

    private static void makeNew( Callbacks callbacks, int nDevsSeen,
                                 int nPlayersMissing, boolean isRematch )
    {
        Log.d( TAG, "makeNew(nDevsSeen=%d, nPlayersMissing=%d)", nDevsSeen, nPlayersMissing );
        State state = new State( nDevsSeen, nPlayersMissing, isRematch );
        InvitesNeededAlert self = new InvitesNeededAlert( callbacks, state );
        callbacks.getDelegate().showDialogFragment( DlgID.DLG_INVITE, state );
    }

    private static void close( InvitesNeededAlert self )
    {
        DbgUtils.assertOnUIThread();
        Assert.assertTrueNR( self == sInstance[0] );
        if ( self == sInstance[0] ) {
            sInstance[0] = null;
            if ( null != self.mAlert ) {
                InviteChoicesAlert.dismissAny();
                self.mAlert.dismiss();
            }
        }
    }

    ////////////////////////////////////////
    // DialogInterface.OnDismissListener
    ////////////////////////////////////////
    @Override
    public void onDismiss( DialogInterface dialog )
    {
        Log.d( TAG, "onDismiss()" );
        close( this );
    }

    private InvitesNeededAlert( Callbacks callbacks, State state )
    {
        mState = state;
        mCallbacks = callbacks;
        mDelegate = callbacks.getDelegate();
        DbgUtils.assertOnUIThread();
        Assert.assertTrueNR( null == sInstance[0] );
        sInstance[0] = this;
    }

    private Dialog makeImpl( Callbacks callbacks, final DBAlert alert,
                             Object[] params )
    {
        Dialog result = null;
        State state = (State)params[0];
        mAlert = alert;

        Context context = mDelegate.getActivity();
        String title;

        boolean isRematch = state.isRematch;
        if ( isRematch ) {
            title = LocUtils.getString( context, R.string.waiting_rematch_title );
        } else {
            title = LocUtils
                .getQuantityString( context, R.plurals.waiting_title_fmt,
                                    state.nPlayersMissing, state.nPlayersMissing );
        }

        String message = LocUtils
            .getQuantityString( context, R.plurals.invite_msg_fmt,
                                state.nPlayersMissing, state.nPlayersMissing );
        message += "\n\n"
            + LocUtils.getString( context, R.string.invite_msg_extra );

        if ( isRematch ) {
            message += "\n\n"
                + LocUtils.getString( context, R.string.invite_msg_extra_rematch );
        }

        AlertDialog.Builder ab = mDelegate.makeAlertBuilder()
            .setTitle( title )
            .setMessage( message );
        
        alert.setNoDismissListenerPos( ab, R.string.newgame_invite,
                                       new OnClickListener() {
                                           @Override
                                           public void onClick( DialogInterface dlg, int item ) {
                                               onPosClick();
                                           }
                                       } );

        if ( BuildConfig.NON_RELEASE ) {
            long rowid = mCallbacks.getRowID();
            SentInvitesInfo sentInfo = DBUtils.getInvitesFor( context, rowid );
            int nSent = sentInfo.getMinPlayerCount();
            boolean invitesSent = nSent >= state.nPlayersMissing;
            if ( invitesSent ) {
                alert.setNoDismissListenerNeut( ab, R.string.newgame_invite_more,
                                                new OnClickListener() {
                                                    @Override
                                                    public void onClick( DialogInterface dlg, int item ) {
                                                        onNeutClick();
                                                    }
                                                } );
            }
        }

        alert.setNoDismissListenerNeg( ab, R.string.button_close,
                                       new OnClickListener() {
                                           @Override
                                           public void onClick( DialogInterface dlg, int item ) {
                                               onNegClick();
                                           }
                                       } );

        result = ab.create();
        result.setOnDismissListener( this );
        return result;
    }

    private void onPosClick()
    {
        mCallbacks.onInviteClicked();
    }

    private void onNeutClick()
    {
        mCallbacks.onInfoClicked();
    }

    private void onNegClick()
    {
        Log.d( TAG, "onNegClick()" );
        mCallbacks.onCloseClicked();
    }
}

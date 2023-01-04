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
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;

import java.io.Serializable;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;
import org.eehouse.android.xw4.loc.LocUtils;

class InvitesNeededAlert {
    private static final String TAG = InvitesNeededAlert.class.getSimpleName();

    private DelegateBase mDelegate;
    private State mState;
    private DBAlert mAlert;

    static class Wrapper {
        private Callbacks mCallbacks;
        private InvitesNeededAlert mSelf;
        private CommsAddrRec mHostAddr;
        private GamePtr mGamePtr;

        Wrapper( Callbacks callbacks, GamePtr gamePtr )
        {
            mCallbacks = callbacks;
            // PENGING should be calling gamePtr.retain()? Would need to
            // release then somewhere
            mGamePtr = gamePtr;
        }

        void showOrHide( CommsAddrRec hostAddr, int nPlayersMissing, int nInvited,
                         boolean isRematch )
        {
            mHostAddr = hostAddr;
            boolean isServer = null == hostAddr;
            DbgUtils.assertOnUIThread();
            Log.d( TAG, "showOnceIf(nPlayersMissing=%d); self: %s", nPlayersMissing, mSelf );

            if ( null == mSelf && 0 == nPlayersMissing ) {
                // cool: need and have nothing, so do nothing
            } else if ( 0 < nPlayersMissing && null == mSelf ) { // Need but don't have
                makeNew( isServer, nPlayersMissing, nInvited, isRematch );
            } else if ( 0 == nPlayersMissing && null != mSelf ) { // Have and need to close
                mSelf.close();
            } else if ( null != mSelf && nPlayersMissing != mSelf.mState.mNPlayersMissing ) {
                mSelf.close();
                makeNew( isServer, nPlayersMissing, nInvited, isRematch );
            } else if ( null != mSelf && nPlayersMissing == mSelf.mState.mNPlayersMissing ) {
                // nothing to do
            } else {
                Assert.failDbg();
            }
        }

        AlertDialog make( DBAlert alert, Object[] params )
        {
            DbgUtils.assertOnUIThread();
            return mSelf.makeImpl( mCallbacks, alert, mHostAddr, mGamePtr, params );
        }

        void dismiss()
        {
            Log.d( TAG, "dismiss()" );
            DbgUtils.assertOnUIThread();
            if ( null != mSelf && mSelf.close() ) {
                mSelf = null;
            }
        }

        private void makeNew( boolean isServer, int nPlayersMissing,
                              int nInvited, boolean isRematch )
        {
            Log.d( TAG, "makeNew(nPlayersMissing=%d, nInvited=%d)",
                   nPlayersMissing, nInvited );
            State state = new State( isServer, nPlayersMissing, nInvited, isRematch );
            mSelf = new InvitesNeededAlert( mCallbacks.getDelegate(), state );
            mCallbacks.getDelegate().showDialogFragment( DlgID.DLG_INVITE, state );
        }
    }

    // Must be kept separate from this because gets passed as param to
    // showDialogFragment()
    private static class State implements Serializable {
        private int mNPlayersMissing;
        private int mNInvited;
        private boolean mIsRematch;
        private boolean mIsServer;

        State( boolean isServer, int nMissing, int nInvited, boolean rematch )
        {
            mNPlayersMissing = nMissing;
            mNInvited = nInvited;
            mIsRematch = rematch;
            mIsServer = isServer;
        }
    }

    interface Callbacks {
        DelegateBase getDelegate();
        long getRowID();
        void onCloseClicked();
        void onInviteClicked();
        void onInfoClicked( SentInvitesInfo sentInfo );
    }

    private boolean close()
    {
        boolean dismissed = false;
        DbgUtils.assertOnUIThread();
        if ( null != mAlert ) {
            dismissed = InviteChoicesAlert.dismissAny();
            try {
                mAlert.dismiss(); // I've seen this throw a NPE inside
            } catch ( Exception ex ) {
                Log.ex( TAG, ex );
            }
        }
        return dismissed;
    }

    private InvitesNeededAlert( DelegateBase delegate, State state )
    {
        DbgUtils.assertOnUIThread();
        mDelegate = delegate;
        mState = state;
    }

    private AlertDialog makeImpl( final Callbacks callbacks, DBAlert alert,
                                  CommsAddrRec hostAddr, GamePtr gamePtr,
                                  Object[] params )
    {
        State state = (State)params[0];
        AlertDialog.Builder ab = mDelegate.makeAlertBuilder();
        mAlert = alert;
        int[] closeLoc = { AlertDialog.BUTTON_NEGATIVE };

        if ( state.mIsServer ) {
            makeImplHost( ab, callbacks, alert, state, gamePtr, closeLoc );
        } else {
            makeImplGuest( ab, state, hostAddr );
        }

        alert.setOnCancelListener( new XWDialogFragment.OnCancelListener() {
                @Override
                public void onCancelled( XWDialogFragment frag ) {
                    // Log.d( TAG, "onCancelled(frag=%s)", frag );
                    callbacks.onCloseClicked();
                    close();
                }
            } );

        OnClickListener onClose = new OnClickListener() {
                @Override
                public void onClick( DialogInterface dlg, int item ) {
                    callbacks.onCloseClicked();
                }
            };
        switch ( closeLoc[0] ) {
        case AlertDialog.BUTTON_NEGATIVE:
            alert.setNoDismissListenerNeg( ab, R.string.button_close_game, onClose );
            break;
        case AlertDialog.BUTTON_POSITIVE:
            alert.setNoDismissListenerPos( ab, R.string.button_close_game, onClose );
            break;
        default:
            Assert.failDbg();
        }

        AlertDialog result = ab.create();
        result.setCanceledOnTouchOutside( false );
        return result;
    }

    private void makeImplGuest( AlertDialog.Builder ab, State state,
                                CommsAddrRec hostAddr )
    {
        Context context = mDelegate.getActivity();
        String message = LocUtils.getString( context, R.string.waiting_host_expl );

        if ( 1 < state.mNPlayersMissing ) {
            message += "\n\n" +
                LocUtils.getString( context, R.string.waiting_host_expl_multi );
        }

        if ( BuildConfig.NON_RELEASE
             && null != hostAddr
             && hostAddr.contains( CommsConnType.COMMS_CONN_MQTT ) ) {
            String name =  XwJNI.kplr_nameForMqttDev( hostAddr.mqtt_devID );
            if ( null != name ) {
                message += "\n\n" + LocUtils
                    .getString( context, R.string.missing_host_fmt, name );
            }
        }

        ab.setTitle( R.string.waiting_host_title )
            .setMessage( message )
            ;
    }

    private void makeImplHost( AlertDialog.Builder ab, final Callbacks callbacks,
                               DBAlert alert, State state, GamePtr gamePtr,
                               int[] closeLoc )
    {
        Context context = mDelegate.getActivity();
        final int nPlayersMissing = state.mNPlayersMissing;

        long rowid = callbacks.getRowID();
        SentInvitesInfo sentInfo = DBUtils.getInvitesFor( context, rowid );

        int nSent = state.mNInvited + sentInfo.getMinPlayerCount();
        boolean invitesNeeded = nPlayersMissing > nSent;

        String title;
        boolean isRematch = state.mIsRematch;
        if ( isRematch ) {
            title = LocUtils.getString( context, R.string.waiting_rematch_title );
        } else {
            title = LocUtils
                .getQuantityString( context, R.plurals.waiting_title_fmt,
                                    nPlayersMissing, nPlayersMissing );
        }
        ab.setTitle( title );

        String message;
        int inviteButtonTxt;
        if ( invitesNeeded ) {
            Assert.assertTrueNR( !isRematch );
            message = LocUtils.getString( context, R.string.invites_unsent );
            inviteButtonTxt = R.string.newgame_invite;
        } else {
            message = LocUtils
                .getQuantityString( context, R.plurals.invite_msg_fmt, // here
                                    nPlayersMissing, nPlayersMissing );
            if ( isRematch ) {
                message += "\n\n"
                    + LocUtils.getString( context, R.string.invite_msg_extra_rematch );
            }
            inviteButtonTxt = R.string.newgame_reinvite;
        }
        ab.setMessage( message );

        // If user needs to act, emphasize that by having the positive button
        // be Invite. If not, have the positive button be Close
        OnClickListener onInvite = new OnClickListener() {
                @Override
                public void onClick( DialogInterface dlg, int item ) {
                    callbacks.onInviteClicked();
                }
            };

        if ( invitesNeeded ) {
            alert.setNoDismissListenerPos( ab, inviteButtonTxt, onInvite );
        } else {
            alert.setNoDismissListenerNeg( ab, inviteButtonTxt, onInvite );
            closeLoc[0] = DialogInterface.BUTTON_POSITIVE;
        }

        if ( BuildConfig.NON_RELEASE && 0 < nSent ) {
            alert.setNoDismissListenerNeut( ab, R.string.button_invite_history,
                                            new OnClickListener() {
                                                @Override
                                                public void onClick( DialogInterface dlg, int item ) {
                                                    callbacks.onInfoClicked( sentInfo );
                                                }
                                            } );
        }
    }
}

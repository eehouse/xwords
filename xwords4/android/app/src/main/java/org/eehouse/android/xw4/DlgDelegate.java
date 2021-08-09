/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.app.ProgressDialog;
import android.content.DialogInterface.OnCancelListener;
import android.os.Handler;

import java.io.Serializable;


import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;

public class DlgDelegate {
    private static final String TAG = DlgDelegate.class.getSimpleName();

    public static enum Action {
        SKIP_CALLBACK,

        // GameListDelegate
        RESET_GAMES,
        SYNC_MENU,
        NEW_FROM,
        DELETE_GAMES,
        DELETE_GROUPS,
        OPEN_GAME,
        CLEAR_SELS,
        NEW_NET_GAME,
        SET_HIDE_NEWGAME_BUTTONS,
        DWNLD_LOC_DICT,
        NEW_GAME_DFLT_NAME,
        SEND_EMAIL,
        WRITE_LOG_DB,
        CLEAR_LOG_DB,
        QUARANTINE_CLEAR,
        QUARANTINE_DELETE,
        APPLY_CONFIG,

        // BoardDelegate
        UNDO_LAST_ACTION,
        LAUNCH_INVITE_ACTION,
        SYNC_ACTION,
        COMMIT_ACTION,
        SHOW_EXPL_ACTION,
        PREV_HINT_ACTION,
        NEXT_HINT_ACTION,
        JUGGLE_ACTION,
        FLIP_ACTION,
        UNDO_ACTION,
        CHAT_ACTION,
        START_TRADE_ACTION,
        LOOKUP_ACTION,
        BUTTON_BROWSE_ACTION,
        VALUES_ACTION,
        SMS_CONFIG_ACTION,
        BUTTON_BROWSEALL_ACTION,
        DROP_RELAY_ACTION,
        DROP_SMS_ACTION,
        INVITE_SMS_DATA,
        BLANK_PICKED,
        TRAY_PICKED,
        INVITE_INFO,
        DISABLE_DUALPANE,
        ARCHIVE_SEL_ACTION,     // archive was clicked
        ARCHIVE_ACTION,
        REMATCH_ACTION,
        DELETE_ACTION,

        // Dict Browser
        FINISH_ACTION,
        DELETE_DICT_ACTION,
        UPDATE_DICTS_ACTION,
        MOVE_CONFIRMED,
        SHOW_TILES,

        // Game configs
        LOCKED_CHANGE_ACTION,
        DELETE_AND_EXIT,

        // New Game
        NEW_GAME_ACTION,

        // SMS invite
        CLEAR_ACTION,
        USE_IMMOBILE_ACTION,
        POST_WARNING_ACTION,

        // BT Invite
        OPEN_BT_PREFS_ACTION,

        // Study list
        SL_CLEAR_ACTION,
        SL_COPY_ACTION,

        // DwnldDelegate && GamesListDelegate
        STORAGE_CONFIRMED,

        // Known Players
        KNOWN_PLAYER_DELETE,

        // classify me
        ENABLE_NBS_ASK,
        ENABLE_NBS_DO,
        ENABLE_BT_DO,
        ENABLE_RELAY_DO,
        ENABLE_RELAY_DO_OR,
        DISABLE_RELAY_DO,
        DISABLE_BT_DO,
        ASKED_PHONE_STATE,
        PERMS_QUERY,
        PERMS_BANNED_INFO,
        SHOW_FAQ,
        EXPORT_THEME,
    } // Action enum

    public static class ActionPair implements Serializable {
        public ActionPair( Action act, int str ) {
            buttonStr = str; action = act;
        }
        public int buttonStr;
        public Action action;

        @Override
        public boolean equals( Object obj )
        {
            boolean result;
            if ( BuildConfig.DEBUG ) {
                result = null != obj && obj instanceof ActionPair;
                if ( result ) {
                    ActionPair other = (ActionPair)obj;
                    result = buttonStr == other.buttonStr
                        && action == other.action;
                }
            } else {
                result = super.equals( obj );
            }
            return result;
        }
    }

    public class Builder {
        private final DlgState mState;

        Builder( DlgID dlgID ) {
            mState = new DlgState( dlgID )
                .setPosButton( android.R.string.ok ) // default
                .setAction( Action.SKIP_CALLBACK )
                ;
        }

        Builder setMessage( String msg )
        {
            mState.setMsg( msg );
            return this;
        }

        Builder setMessageID( int msgID )
        {
            mState.setMsg( getString( msgID ) );
            return this;
        }

        Builder setActionPair( Action action, int strID )
        {
            mState.setActionPair( new ActionPair( action, strID ) );
            return this;
        }

        Builder setAction( Action action )
        {
            mState.setAction( action );
            return this;
        }

        Builder setPosButton( int strID )
        {
            mState.setPosButton( strID );
            return this;
        }

        Builder setNegButton( int strID )
        {
            mState.setNegButton( strID );
            return this;
        }

        Builder setTitle( int strID )
        {
            mState.setTitle( getString(strID) );
            return this;
        }

        Builder setTitle( String str )
        {
            mState.setTitle( str );
            return this;
        }

        Builder setParams( Object... params )
        {
            mState.setParams( params );
            return this;
        }

        Builder setNAKey( int keyID )
        {
            mState.setPrefsNAKey( keyID );
            return this;
        }

        public void show()
        {
            int naKey = mState.m_prefsNAKey;
            final Action action = mState.m_action;
            // Log.d( TAG, "show(): key: %d; action: %s", naKey, action );

            if ( 0 == naKey || ! XWPrefs.getPrefsBoolean( m_activity, naKey, false ) ) {
                m_dlgt.show( mState );
            } else if ( Action.SKIP_CALLBACK != action ) {
                post( new Runnable() {
                        @Override
                        public void run() {
                            Log.d( TAG, "calling onPosButton()" );
                            XWActivity xwact = (XWActivity)m_activity;
                            xwact.onPosButton( action, mState.getParams() );
                        }
                    });
            }
        }
    }


    public Builder makeOkOnlyBuilder( String msg )
    {
        // return new OkOnlyBuilder( msg );

        Builder builder = new Builder( DlgID.DIALOG_OKONLY )
            .setMessage( msg )
            ;
        return builder;
    }

    public Builder makeOkOnlyBuilder( int msgID )
    {
        Builder builder = new Builder( DlgID.DIALOG_OKONLY )
            .setMessageID( msgID )
            ;
        return builder;
    }

    private Builder makeConfirmThenBuilder( Action action )
    {
        return new Builder( DlgID.CONFIRM_THEN )
            .setAction( action )
            .setNegButton( android.R.string.cancel )
            ;
    }
    
    public Builder makeConfirmThenBuilder( String msg, Action action )
    {
        return makeConfirmThenBuilder( action )
            .setMessage( msg )
            ;
    }

    public Builder makeConfirmThenBuilder( int msgID, Action action )
    {
        return makeConfirmThenBuilder( action )
            .setMessageID( msgID )
            ;
    }

    private Builder makeNotAgainBuilder( int key )
    {
        return new Builder( DlgID.DIALOG_NOTAGAIN )
            .setNAKey( key )
            .setAction( Action.SKIP_CALLBACK )
            .setTitle( R.string.newbie_title )
            ;
    }

    public Builder makeNotAgainBuilder( String msg, int key, Action action )
    {
        return makeNotAgainBuilder( key )
            .setMessage( msg )
            .setAction( action )
            ;
    }

    public Builder makeNotAgainBuilder( int msgID, int key, Action action )
    {
        return makeNotAgainBuilder( key )
            .setMessageID( msgID )
            .setAction( action )
            ;
    }

    public Builder makeNotAgainBuilder( String msg, int key )
    {
        return makeNotAgainBuilder( key )
            .setMessage( msg )
            ;
    }

    public Builder makeNotAgainBuilder( int msgID, int key )
    {
        return makeNotAgainBuilder( key )
            .setMessageID( msgID )
            ;
    }

    public static final int SMS_BTN = AlertDialog.BUTTON_POSITIVE;
    public static final int NFC_BTN = AlertDialog.BUTTON_NEUTRAL;
    public static final int DISMISS_BUTTON = 0;

    public interface DlgClickNotify {
        // These are stored in the INVITES table. Don't change order
        // gratuitously
        public static enum InviteMeans {
            SMS_DATA(R.string.invite_choice_data_sms, false), // classic NBS-based data sms
            EMAIL(R.string.invite_choice_email, false),
            NFC(R.string.invite_choice_nfc, true),
            BLUETOOTH(R.string.invite_choice_bt, true),
            CLIPBOARD(R.string.slmenu_copy_sel, false),
            RELAY(R.string.invite_choice_relay, false),
            WIFIDIRECT(R.string.invite_choice_p2p, false),
            SMS_USER(R.string.invite_choice_user_sms, false), // just launch the SMS app, as with email
            MQTT(R.string.invite_choice_mqtt, false),
            QRCODE(R.string.invite_choice_qrcode, true);

            private InviteMeans( int resid, boolean local) {
                mResID = resid;
                mIsLocal = local;
            }
            private int mResID;
            private boolean mIsLocal;
            public int getUserDescID() { return mResID; }
            public boolean isForLocal() { return mIsLocal; }
        };

        boolean onPosButton( Action action, Object... params );
        boolean onNegButton( Action action, Object... params );
        boolean onDismissed( Action action, Object... params );

        void inviteChoiceMade( Action action, InviteMeans means, Object... params );
    }
    public interface HasDlgDelegate {
        Builder makeOkOnlyBuilder( int msgID );
        Builder makeOkOnlyBuilder( String msg );
        Builder makeNotAgainBuilder( int msgID, int prefsKey, Action action );
        Builder makeNotAgainBuilder( int msgID, int prefsKey );
    }

    private Activity m_activity;
    private DelegateBase m_dlgt;
    private DlgClickNotify m_clickCallback;
    private String m_dictName = null;
    private ProgressDialog m_progress;
    private Handler m_handler;

    public DlgDelegate( Activity activity, DelegateBase dlgt,
                        DlgClickNotify callback )
    {
        m_activity = activity;
        m_dlgt = dlgt;
        m_clickCallback = callback;
        m_handler = new Handler();
    }

    void onPausing()
    {
        stopProgress();
    }

    // Puts up alert asking to choose a reason to enable SMS, and on dismiss
    // calls onPosButton/onNegButton with the action and in params a Boolean
    // indicating whether enabling is now ok.
    public void showSMSEnableDialog( Action action )
    {
        DlgState state = new DlgState( DlgID.DIALOG_ENABLESMS )
            .setAction( action );

        m_dlgt.show( state );
    }

    public void showInviteChoicesThen( final Action action,
                                       NetLaunchInfo nli, int nMissing )
    {
        DlgState state = new DlgState( DlgID.INVITE_CHOICES_THEN )
            .setAction( action )
            .setParams( nli, nMissing );
        m_dlgt.show( state );
    }

    public void doSyncMenuitem()
    {
        Log.d( TAG, "doSyncMenuitem()" );
        if ( null == DBUtils.getRelayIDs( m_activity, null ) ) {
            makeOkOnlyBuilder( R.string.no_games_to_refresh ).show();
        } else {
            RelayService.timerFired( m_activity );
            MQTTUtils.timerFired( m_activity );
            Utils.showToast( m_activity, R.string.msgs_progress );
        }
    }

    public void launchLookup( String[] words, int lang, boolean noStudy )
    {
        m_dlgt.show( LookupAlert.newInstance( words, lang, noStudy ) );
    }

    public void startProgress( int titleID, int msgID, OnCancelListener lstnr )
    {
        startProgress( titleID, getString( msgID ), lstnr );
    }

    public void startProgress( int titleID, String msg, OnCancelListener lstnr )
    {
        String title = getString( titleID );
        startProgress( title, msg, lstnr );
    }

    public void startProgress( String title, String msg, OnCancelListener lstnr )
    {
        m_progress = ProgressDialog.show( m_activity, title, msg, true, true );

        if ( null != lstnr ) {
            m_progress.setCancelable( true );
            m_progress.setOnCancelListener( lstnr );
        }
    }

    public void setProgressMsg( int id )
    {
        ProgressDialog progress = m_progress;
        if ( null != progress ) {
            progress.setMessage( getString( id ) );
        }
    }

    public void stopProgress()
    {
        if ( null != m_progress ) {
            m_progress.dismiss();
            m_progress = null;
        }
    }

    public boolean post( Runnable runnable )
    {
        m_handler.post( runnable );
        return true;
    }

    public void eventOccurred( MultiService.MultiEvent event,
                               final Object ... args )
    {
        String msg = null;
        boolean asToast = true;
        switch( event ) {
        case MESSAGE_RESEND:
            msg = getString( R.string.bt_resend_fmt, (String)args[0],
                             (Long)args[1], (Integer)args[2] );
            break;
        case MESSAGE_FAILOUT:
            msg = getString( R.string.bt_fail_fmt, (String)args[0] );
            asToast = false;
            break;
        case RELAY_ALERT:
            msg = (String)args[0];
            asToast = false;
            break;

        default:
            Log.e( TAG, "eventOccurred: unhandled event %s", event.toString() );
        }

        if ( null != msg ) {
            final String fmsg = msg;
            final boolean asDlg = !asToast;
            post( new Runnable() {
                    public void run() {
                        if ( asDlg ) {
                            makeOkOnlyBuilder( fmsg ).show();
                        } else {
                            DbgUtils.showf( m_activity, fmsg );
                        }
                    }
                } );
        }
    }

    private String getString( int id, Object... params )
    {
        return m_dlgt.getString( id, params );
    }

    private String getQuantityString( int id, int quantity, Object... params )
    {
        return m_dlgt.getQuantityString( id, quantity, params );
    }
}

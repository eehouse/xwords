/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.support.v4.app.DialogFragment;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;

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
        NEW_GAME_PRESSED,
        SET_HIDE_NEWGAME_BUTTONS,
        DWNLD_LOC_DICT,
        NEW_GAME_DFLT_NAME,
        DISABLE_BT_BACK,

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
        NFC_TO_SELF,
        DROP_RELAY_ACTION,
        DROP_SMS_ACTION,
        INVITE_SMS,
        BLANK_PICKED,
        TRAY_PICKED,
        INVITE_INFO,
        DISABLE_DUALPANE,
        ARCHIVE_ACTION,

        // Dict Browser
        FINISH_ACTION,
        DELETE_DICT_ACTION,
        UPDATE_DICTS_ACTION,
        MOVE_CONFIRMED,

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

        // clasify me
        ENABLE_SMS_ASK,
        ENABLE_SMS_DO,
        ENABLE_BT_DO,
        ENABLE_RELAY_DO,
        ENABLE_RELAY_DO_OR,
        DISABLE_RELAY_DO,
        ASKED_PHONE_STATE,
        PERMS_QUERY,

        // Sent when not-again checkbox checked
        SET_NA_DEFAULTNAME,
        SET_GOT_LANGDICT,
    }

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

    public abstract class DlgDelegateBuilder {
        protected String m_msgString;
        protected int m_nakey;
        protected Action m_onNA;
        protected int m_posButton = android.R.string.ok;
        protected int m_negButton = android.R.string.cancel;
        protected Action m_action;
        protected Object[] m_params;
        protected int m_titleId = 0;

        public DlgDelegateBuilder( String msg, Action action )
        { m_msgString = msg; m_action = action; }

        public DlgDelegateBuilder( int msgId, Action action )
        { this( getString(msgId), action );}

        public DlgDelegateBuilder setNAKey( int keyId )
        {  m_nakey = keyId; return this; }

        public DlgDelegateBuilder setOnNA( Action onNA )
        { m_onNA = onNA; return this; }

        public DlgDelegateBuilder setPosButton( int id )
        { m_posButton = id; return this; }

        public DlgDelegateBuilder setNegButton( int id )
        { m_negButton = id; return this; }

        public DlgDelegateBuilder setParams( Object... params )
        { m_params = params; return this; }

        abstract void show();
    }

    public class OkOnlyBuilder extends DlgDelegateBuilder {

        public OkOnlyBuilder(String msg) { super( msg, Action.SKIP_CALLBACK ); }
        public OkOnlyBuilder(int msgId) { super( msgId, Action.SKIP_CALLBACK ); }
        public OkOnlyBuilder setAction( Action action )
        { m_action = action; return this; }
        public OkOnlyBuilder setTitle( int titleId )
        { m_titleId = titleId; return this; }

        @Override
        public void show()
        {
            showOKOnlyDialogThen( m_msgString, m_action, m_params, m_titleId );
        }
    }

    public class ConfirmThenBuilder extends DlgDelegateBuilder {
        public ConfirmThenBuilder(String msg, Action action) {super(msg, action);}
        public ConfirmThenBuilder(int msgId, Action action) {super(msgId, action);}

        public ConfirmThenBuilder setTitle( int titleId )
        { m_titleId = titleId; return this; }

        @Override
        public void show()
        {
            showConfirmThen( m_nakey, m_onNA, m_msgString, m_posButton,
                             m_negButton, m_action, m_titleId, m_params );
        }
    }

    public class NotAgainBuilder extends DlgDelegateBuilder {
        private int m_prefsKey;
        private ActionPair m_actionPair;

        public NotAgainBuilder(String msg, int key, Action action)
        { super(msg, action); m_prefsKey = key; }

        public NotAgainBuilder(int msgId, int key, Action action)
        { super(msgId, action); m_prefsKey = key; }

        public NotAgainBuilder( String msg, int key )
        { super( msg, Action.SKIP_CALLBACK ); m_prefsKey = key; }

        public NotAgainBuilder( int msgId, int key )
        { super( msgId, Action.SKIP_CALLBACK ); m_prefsKey = key; }

        public NotAgainBuilder setActionPair( ActionPair pr )
        { m_actionPair = pr; return this; }

        @Override
        public void show()
        {
            showNotAgainDlgThen( m_msgString, m_prefsKey,
                                 m_action, m_actionPair,
                                 m_params );
        }
    }

    public OkOnlyBuilder makeOkOnlyBuilder( String msg )
    {
        return new OkOnlyBuilder( msg );
    }
    public OkOnlyBuilder makeOkOnlyBuilder( int msgId )
    {
        return new OkOnlyBuilder( msgId );
    }

    public ConfirmThenBuilder makeConfirmThenBuilder( String msg, Action action )
    {
        return new ConfirmThenBuilder( msg, action );
    }

    public ConfirmThenBuilder makeConfirmThenBuilder(int msgId, Action action)
    {
        return new ConfirmThenBuilder( msgId, action );
    }

    public NotAgainBuilder makeNotAgainBuilder( int msgId, int key,
                                                Action action )
    {
        return new NotAgainBuilder( msgId, key, action );
    }

    public NotAgainBuilder makeNotAgainBuilder( String msg, int key,
                                                Action action )
    {
        return new NotAgainBuilder( msg, key, action );
    }

    public NotAgainBuilder makeNotAgainBuilder( String msg, int key )
    {
        return new NotAgainBuilder( msg, key );
    }

    public NotAgainBuilder makeNotAgainBuilder( int msgId, int key ) {
        return new NotAgainBuilder( msgId, key );
    }

    public static final int SMS_BTN = AlertDialog.BUTTON_POSITIVE;
    public static final int NFC_BTN = AlertDialog.BUTTON_NEUTRAL;
    public static final int DISMISS_BUTTON = 0;

    public interface DlgClickNotify {
        // These are stored in the INVITES table. Don't change order
        // gratuitously
        public static enum InviteMeans {
            SMS, EMAIL, NFC, BLUETOOTH, CLIPBOARD, RELAY, WIFIDIRECT,
        };
        boolean onPosButton( Action action, Object... params );
        boolean onNegButton( Action action, Object... params );
        boolean onDismissed( Action action, Object... params );

        void inviteChoiceMade( Action action, InviteMeans means, Object... params );
    }
    public interface HasDlgDelegate {
        OkOnlyBuilder makeOkOnlyBuilder( int msgID );
        OkOnlyBuilder makeOkOnlyBuilder( String msg );
        NotAgainBuilder makeNotAgainBuilder( int msgID, int prefsKey,
                                             Action action );
        NotAgainBuilder makeNotAgainBuilder( int msgID, int prefsKey );
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

    private void showOKOnlyDialogThen( String msg, Action action,
                                       Object[] params, int titleId )
    {
        DlgState state = new DlgState( DlgID.DIALOG_OKONLY )
            .setMsg( msg )
            .setParams( params )
            .setTitle( titleId )
            .setAction(action);
        m_dlgt.show( state );
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

    private void showNotAgainDlgThen( String msg, int prefsKey,
                                      final Action action, ActionPair more,
                                      final Object[] params )
    {
        if ( XWPrefs.getPrefsBoolean( m_activity, prefsKey, false ) ) {
            // If it's set, do the action without bothering with the
            // dialog
            if ( Action.SKIP_CALLBACK != action ) {
                post( new Runnable() {
                        public void run() {
                            XWActivity xwact = (XWActivity)m_activity;
                            xwact.onPosButton( action, params );
                        }
                    });
            }
        } else {
            DlgState state = new DlgState( DlgID.DIALOG_NOTAGAIN )
                .setMsg( msg)
                .setPrefsKey( prefsKey )
                .setAction( action )
                .setActionPair( more )
                .setParams( params );
            m_dlgt.show( state );
        }
    }

    private void showConfirmThen( int nakey, Action onNA, String msg,
                                  int posButton, int negButton, Action action,
                                  int titleId, Object[] params )
    {
        if ( 0 == nakey ||
             ! XWPrefs.getPrefsBoolean( m_activity, nakey, false ) ) {
            DlgState state = new DlgState( DlgID.CONFIRM_THEN ).setOnNA(onNA)
                .setMsg( msg )
                .setPosButton( posButton )
                .setNegButton( negButton )
                .setAction( action )
                .setTitle( titleId )
                .setParams( params );
            m_dlgt.show( state );
        }
    }

    public void showInviteChoicesThen( final Action action,
                                       SentInvitesInfo info )
    {
        if ( (XWApp.SMS_INVITE_ENABLED && Utils.deviceSupportsSMS( m_activity ))
             || XWPrefs.getNFCToSelfEnabled( m_activity )
             || NFCUtils.nfcAvail( m_activity )[0]
             || WiDirWrapper.enabled()
             || BTService.BTAvailable() ) {
            DlgState state = new DlgState( DlgID.INVITE_CHOICES_THEN )
                .setAction( action )
                .setParams( info );
            m_dlgt.show( state );
        } else {
            post( new Runnable() {
                    public void run() {
                        DlgClickNotify.InviteMeans means
                            = DlgClickNotify.InviteMeans.EMAIL;
                        m_clickCallback.inviteChoiceMade( action, means );
                    }
                });
        }
    }

    public void doSyncMenuitem()
    {
        if ( null == DBUtils.getRelayIDs( m_activity, null ) ) {
            makeOkOnlyBuilder( R.string.no_games_to_refresh ).show();
        } else {
            RelayService.timerFired( m_activity );
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
        m_progress = ProgressDialog.show( m_activity, title, msg, true, true );

        if ( null != lstnr ) {
            m_progress.setCancelable( true );
            m_progress.setOnCancelListener( lstnr );
        }
    }

    public void setProgressMsg( int id )
    {
        m_progress.setMessage( getString( id ) );
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

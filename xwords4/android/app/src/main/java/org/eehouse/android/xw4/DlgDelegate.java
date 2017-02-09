/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
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
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.loc.LocUtils;

import java.lang.ref.WeakReference;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

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
        ENABLE_DUALPANE,
        ENABLE_DUALPANE_EXIT,

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

        // Dict Browser
        FINISH_ACTION,
        DELETE_DICT_ACTION,
        UPDATE_DICTS_ACTION,
        MOVE_CONFIRMED,

        // Game configs
        LOCKED_CHANGE_ACTION,
        DELETE_AND_EXIT,
        SET_ENABLE_PUBLIC,

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
    }

    public static class ActionPair {
        public ActionPair( Action act, int str ) {
            buttonStr = str; action = act;
        }
        public int buttonStr;
        public Action action;
        public Object[] params; // null for now
    }

    // typesafe int, basically
    public static class NAKey implements Runnable {
        private Context m_context;
        private int m_nakey;
        public NAKey(int key) { m_nakey = key; }
        boolean isSet( Context context ) {
            m_context = context; // hack!!!
            return XWPrefs.getPrefsBoolean( context, m_nakey, false );
        }

        @Override
        public void run() {
            Assert.assertNotNull( m_context );
            XWPrefs.setPrefsBoolean( m_context, m_nakey, true );
        }
    }

    public abstract class DlgDelegateBuilder {
        protected String m_msgString;
        protected NAKey m_nakey;
        protected Runnable m_onNA;
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
        {  m_nakey = new NAKey( keyId ); return this; }

        public DlgDelegateBuilder setOnNA( Runnable proc )
        { m_onNA = proc; return this; }

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

    private static final String IDS = "IDS";
    private static final String STATE_KEYF = "STATE_%d";

    public interface DlgClickNotify {
        // These are stored in the INVITES table. Don't change order
        // gratuitously
        public static enum InviteMeans {
            SMS, EMAIL, NFC, BLUETOOTH, CLIPBOARD, RELAY, WIFIDIRECT,
        };
        boolean onPosButton( Action action, Object[] params );
        boolean onNegButton( Action action, Object[] params );
        boolean onDismissed( Action action, Object[] params );

        void inviteChoiceMade( Action action, InviteMeans means, Object[] params );
    }
    public interface HasDlgDelegate {
        OkOnlyBuilder makeOkOnlyBuilder( int msgID );
        OkOnlyBuilder makeOkOnlyBuilder( String msg );
        NotAgainBuilder makeNotAgainBuilder( int msgID, int prefsKey,
                                             Action action );
        NotAgainBuilder makeNotAgainBuilder( int msgID, int prefsKey );
    }

    private static Map<DlgID, WeakReference<DelegateBase>> s_pendings
        = new HashMap<DlgID, WeakReference<DelegateBase>>();
    private Activity m_activity;
    private DelegateBase m_dlgt;
    private DlgClickNotify m_clickCallback;
    private String m_dictName = null;
    private ProgressDialog m_progress;
    private Handler m_handler;

    private HashMap<DlgID, DlgState> m_dlgStates;

    public DlgDelegate( Activity activity, DelegateBase dlgt,
                        DlgClickNotify callback, Bundle bundle )
    {
        m_activity = activity;
        m_dlgt = dlgt;
        m_clickCallback = callback;
        m_handler = new Handler();
        m_dlgStates = new HashMap<DlgID,DlgState>();

        if ( null != bundle ) {
            int[] ids = bundle.getIntArray( IDS );
            if ( null != ids ) {
                for ( int id : ids ) {
                    String key = String.format( STATE_KEYF, id );
                    addState( (DlgState)bundle.getParcelable( key ) );
                }
            }
        }
    }

    public void onSaveInstanceState( Bundle outState )
    {
        int[] ids = new int[m_dlgStates.size()];
        if ( 0 < ids.length ) {
            int indx = 0;
            Iterator<DlgState> iter = m_dlgStates.values().iterator();
            while ( iter.hasNext() ) {
                DlgState state = iter.next();
                int id = state.m_id.ordinal();
                String key = String.format( STATE_KEYF, id );
                outState.putParcelable( key, state );
                ids[indx++] = id;
            }
        }
        outState.putIntArray( IDS, ids );
    }

    protected void showDialog( DlgID dlgID )
    {
        // DbgUtils.logf( "showDialog(%s)", dlgID.toString() );
        if ( !m_activity.isFinishing() ) {
            s_pendings.put( dlgID, new WeakReference<DelegateBase>(m_dlgt) );
            m_activity.showDialog( dlgID.ordinal() );
        }
    }

    public Dialog createDialog( int id )
    {
        Dialog dialog = null;
        DlgID dlgID = DlgID.values()[id];
        DlgState state = findForID( dlgID );
        switch( dlgID ) {
        case LOOKUP:
            dialog = createLookupDialog();
            break;
        case DIALOG_OKONLY:
            dialog = createOKDialog( state, dlgID );
            break;
        case DIALOG_NOTAGAIN:
            Assert.assertFalse( BuildConfig.DEBUG );
            //     dialog = createNotAgainDialog( state, dlgID );
            break;
        case CONFIRM_THEN:
            dialog = createConfirmThenDialog( state, dlgID );
            break;
        case INVITE_CHOICES_THEN:
            Assert.assertFalse( BuildConfig.DEBUG );
        //     dialog = createInviteChoicesDialog( state, dlgID );
            break;
        case DLG_DICTGONE:
            dialog = createDictGoneDialog();
            break;
        case DIALOG_ENABLESMS:
            dialog = createEnableSMSDialog( state, dlgID );
            break;
        default:
            DbgUtils.logd( TAG, "not creating %s", dlgID.toString() );
            break;
        }
        return dialog;
    }

    public void prepareDialog( DlgID dlgId, Dialog dialog )
    {
        switch( dlgId ) {
        case INVITE_CHOICES_THEN:
            Assert.assertFalse( BuildConfig.DEBUG );
            // prepareInviteChoicesDialog( dialog );
            break;
        case DIALOG_ENABLESMS:
            prepareEnableSMSDialog( dialog );
            break;
        }
    }

    private void showOKOnlyDialogThen( String msg, Action action,
                                       Object[] params, int titleId )
    {
        // Assert.assertNull( m_dlgStates );
        DlgState state = new DlgState( DlgID.DIALOG_OKONLY )
            .setMsg( msg )
            .setParams( params )
            .setTitle( titleId )
            .setAction(action);
        addState( state );
        showDialog( DlgID.DIALOG_OKONLY );
    }

    public void showDictGoneFinish()
    {
        showDialog( DlgID.DLG_DICTGONE );
    }

    // Puts up alert asking to choose a reason to enable SMS, and on dismiss
    // calls onPosButton/onNegButton with the action and in params a Boolean
    // indicating whether enabling is now ok.
    public void showSMSEnableDialog( Action action, Object... params )
    {
        DlgState state = new DlgState( DlgID.DIALOG_ENABLESMS )
            .setAction( action )
            .setParams( params );
        addState( state );
        showDialog( DlgID.DIALOG_ENABLESMS );
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
                .setMsg( msg).setPrefsKey( prefsKey ).setAction( action )
                .setActionPair( more ).setParams( params );
            // addState( state );
            // showDialog( DlgID.DIALOG_NOTAGAIN );
            m_dlgt.show( NotAgainAlert.newInstance( state ) );
        }
    }

    private void showConfirmThen( NAKey nakey, Runnable onNA, String msg,
                                  int posButton, int negButton, Action action,
                                  int titleId, Object[] params )
    {
        if ( null != nakey ) {
            Assert.assertNull( onNA );
            onNA = nakey;     // so the run() method will be called to set the key
        }
        if ( null == nakey || !nakey.isSet( m_activity ) ) {
            DlgState state = new DlgState( DlgID.CONFIRM_THEN ).setOnNA(onNA)
                .setMsg( msg )
                .setPosButton( posButton )
                .setNegButton( negButton )
                .setAction( action )
                .setTitle( titleId )
                .setParams( params );
            addState( state );
            showDialog( DlgID.CONFIRM_THEN );
        }
    }

    public void showInviteChoicesThen( final Action action,
                                       SentInvitesInfo info )
    {
        if ( (XWApp.SMS_INVITE_ENABLED && Utils.deviceSupportsSMS( m_activity ))
             || XWPrefs.getNFCToSelfEnabled( m_activity )
             || NFCUtils.nfcAvail( m_activity )[0]
             || WiDirService.enabled()
             || BTService.BTAvailable() ) {
            DlgState state = new DlgState( DlgID.INVITE_CHOICES_THEN )
                .setAction( action )
                .setParams( info );
            // addState( state );
            // showDialog( DlgID.INVITE_CHOICES_THEN );
            m_dlgt.show( InviteChoicesAlert.newInstance( state ) );
        } else {
            post( new Runnable() {
                    public void run() {
                        DlgClickNotify.InviteMeans means
                            = DlgClickNotify.InviteMeans.EMAIL;
                        m_clickCallback.inviteChoiceMade( action, means, null );
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
        if ( LookupAlert.needAlert( m_activity, words, lang, noStudy ) ) {
            Bundle params = LookupAlert.makeParams( words, lang, noStudy );
            addState( new DlgState( DlgID.LOOKUP )
                      .setParams( new Object[]{params} ) );
            showDialog( DlgID.LOOKUP );
        } else {
            LookupAlert.launchWordLookup( m_activity, words[0], lang );
        }
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
            DbgUtils.loge( TAG, "eventOccurred: unhandled event %s", event.toString() );
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

    private Dialog createLookupDialog()
    {
        Dialog result = null;
        DlgState state = findForID( DlgID.LOOKUP );
        // state is null per a play store crash report.
        if ( null != state ) {
            Bundle bundle = (Bundle)state.m_params[0];
            result = LookupAlert.makeDialog( m_activity, bundle );
        }
        return result;
    }

    private Dialog createOKDialog( DlgState state, DlgID dlgID )
    {
        Dialog dialog = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( state.m_titleId == 0 ? R.string.info_title : state.m_titleId )
            .setMessage( state.m_msg )
            .setPositiveButton( android.R.string.ok, null )
            .create();
        dialog = setCallbackDismissListener( dialog, state, dlgID );

        return dialog;
    }

    private Dialog createConfirmThenDialog( DlgState state, DlgID dlgID )
    {
        NotAgainView naView = (NotAgainView)
            LocUtils.inflate( m_activity, R.layout.not_again_view );
        naView.setMessage( state.m_msg );
        naView.setShowNACheckbox( null != state.m_onNAChecked );
        OnClickListener lstnr = mkCallbackClickListener( state, naView );

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( state.m_titleId == 0 ? R.string.query_title : state.m_titleId )
            .setView( naView )
            .setPositiveButton( state.m_posButton, lstnr )
            .setNegativeButton( state.m_negButton, lstnr );

        return setCallbackDismissListener( builder.create(), state, dlgID );
    }

    private Dialog createDictGoneDialog()
    {
        Dialog dialog = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.no_dict_title )
            .setMessage( R.string.no_dict_finish )
            .setPositiveButton( R.string.button_close_game, null )
            .create();

        dialog.setOnDismissListener( new DialogInterface.OnDismissListener() {
                public void onDismiss( DialogInterface di ) {
                    m_activity.finish();
                }
            } );

        return dialog;
    }

    private Dialog createEnableSMSDialog( final DlgState state, DlgID dlgID )
    {
        final View layout = LocUtils.inflate( m_activity, R.layout.confirm_sms );

        DialogInterface.OnClickListener lstnr =
            new DialogInterface.OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    Spinner reasons = (Spinner)
                        layout.findViewById( R.id.confirm_sms_reasons );
                    boolean enabled = 0 < reasons.getSelectedItemPosition();
                    Assert.assertTrue( enabled );
                    m_clickCallback.onPosButton( state.m_action, state.m_params );
                }
            };

        Dialog dialog = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.confirm_sms_title )
            .setView( layout )
            .setPositiveButton( R.string.button_enable, lstnr )
            .setNegativeButton( android.R.string.cancel, null )
            .create();
        Utils.setRemoveOnDismiss( m_activity, dialog, dlgID );
        return dialog;
    }

    private void checkEnableButton( Dialog dialog, Spinner reasons )
    {
        boolean enabled = 0 < reasons.getSelectedItemPosition();
        AlertDialog adlg = (AlertDialog)dialog;
        Button button = adlg.getButton( AlertDialog.BUTTON_POSITIVE );
        button.setEnabled( enabled );
    }

    private void prepareEnableSMSDialog( final Dialog dialog )
    {
        final Spinner reasons = (Spinner)
            dialog.findViewById( R.id.confirm_sms_reasons );

        OnItemSelectedListener onItemSel = new OnItemSelectedListener() {
                public void onItemSelected( AdapterView<?> parent, View view,
                                            int position, long id )
                {
                    checkEnableButton( dialog, reasons );
                }

                public void onNothingSelected( AdapterView<?> parent ) {}
            };
        reasons.setOnItemSelectedListener( onItemSel );
        checkEnableButton( dialog, reasons );
    }

    private OnClickListener mkCallbackClickListener( final DlgState state,
                                                     final NotAgainView naView )
    {
        OnClickListener cbkOnClickLstnr;
        cbkOnClickLstnr = new OnClickListener() {
                public void onClick( DialogInterface dlg, int button ) {
                    checkNotAgainCheck( state, naView );

                    if ( Action.SKIP_CALLBACK != state.m_action ) {
                        switch ( button ) {
                        case AlertDialog.BUTTON_POSITIVE:
                            m_clickCallback.onPosButton( state.m_action,
                                                         state.m_params );
                            break;
                        case AlertDialog.BUTTON_NEGATIVE:
                            m_clickCallback.onNegButton( state.m_action,
                                                         state.m_params );
                            break;
                        default:
                            DbgUtils.loge( TAG, "unexpected button %d",
                                           button );
                            // ignore on release builds
                            Assert.assertFalse( BuildConfig.DEBUG );
                        }
                    }
                }
            };
        return cbkOnClickLstnr;
    }

    private void checkNotAgainCheck( DlgState state, NotAgainView naView )
    {
        if ( null != naView && naView.getChecked() ) {
            if ( 0 != state.m_prefsKey ) {
                XWPrefs.setPrefsBoolean( m_activity, state.m_prefsKey,
                                         true );
            } else if ( null != state.m_onNAChecked ) {
                state.m_onNAChecked.run();
            }
        }
    }

    private Dialog setCallbackDismissListener( final Dialog dialog,
                                               final DlgState state,
                                               DlgID dlgID )
    {
        final int id = dlgID.ordinal();
        DialogInterface.OnDismissListener cbkOnDismissLstnr
            = new DialogInterface.OnDismissListener() {
                    public void onDismiss( DialogInterface di ) {
                        dropState( state );
                        if ( Action.SKIP_CALLBACK != state.m_action ) {
                            m_clickCallback.onDismissed( state.m_action,
                                                         state.m_params );
                        }
                        m_activity.removeDialog( id );
                    }
                };

        dialog.setOnDismissListener( cbkOnDismissLstnr );
        return dialog;
    }

    private DlgState findForID( DlgID dlgID )
    {
        DlgState state = m_dlgStates.get( dlgID );
        // DbgUtils.logf( "findForID(%d)=>%H", id, state );
        return state;
    }

    private void dropState( DlgState state )
    {
        int nDlgs = m_dlgStates.size();
        Assert.assertNotNull( state );
        // Assert.assertTrue( state == m_dlgStates.get( state.m_id ) );
        m_dlgStates.remove( state.m_id );
        // DbgUtils.logf( "dropState: active dialogs now %d from %d ",
        //                m_dlgStates.size(), nDlgs );
    }

    private void addState( DlgState state )
    {
        // I'm getting serialization failures on devices pointing at
        // DlgState but the code below says the object's fine (as it
        // should be.)  Just to have a record....
        //
        // Bundle bundle = new Bundle();
        // DbgUtils.logf( "addState: testing serializable" );
        // bundle.putSerializable( "foo", state );
        // state = (DlgState)bundle.getSerializable( "foo" );
        // DbgUtils.logf( "addState: serializable is ok" );

        m_dlgStates.put( state.m_id, state );
    }

    public static Dialog onCreateDialog( int id )
    {
        Dialog result = null;
        DlgID dlgID = DlgID.values()[id];
        WeakReference<DelegateBase> ref = s_pendings.get( dlgID );
        if ( null != ref ) {
            DelegateBase dlgt = ref.get();
            if ( null != dlgt ) {
                result = dlgt.onCreateDialog( id );
            }
        }
        return result;
    }

    public static void onPrepareDialog( int id, Dialog dialog )
    {
        DlgID dlgID = DlgID.values()[id];
        WeakReference<DelegateBase> ref = s_pendings.get( dlgID );
        DelegateBase dlgt = ref.get();
        if ( null != dlgt ) {
            dlgt.prepareDialog( dlgID, dialog );
        }
    }

    protected static void closeAlerts( Activity activity, DelegateBase base )
    {
        DbgUtils.assertOnUIThread();
        Iterator<DlgID> iter = s_pendings.keySet().iterator();
        while ( iter.hasNext() ) {
            DlgID dlgID = iter.next();
            DelegateBase oneBase = s_pendings.get( dlgID ).get();
            if ( null == oneBase ) {
                iter.remove();  // no point in keeping it
            } else if ( base.equals( oneBase ) ) {
                DbgUtils.logd( TAG, "removing alert %s for %s", dlgID.toString(),
                               oneBase.toString() );
                activity.removeDialog( dlgID.ordinal() );
                iter.remove();  // no point in keeping this either
            }
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

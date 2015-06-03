/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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
import android.app.ProgressDialog;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.loc.LocUtils;

public class DlgDelegate {

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
        ZOOM_ACTION,
        UNDO_ACTION,
        CHAT_ACTION,
        START_TRADE_ACTION,
        LOOKUP_ACTION,
        BUTTON_BROWSE_ACTION,
        VALUES_ACTION,
        SMS_CONFIG_ACTION,
        BUTTON_BROWSEALL_ACTION,
        NFC_TO_SELF,

        // Dict Browser
        FINISH_ACTION,
        DELETE_DICT_ACTION,
        UPDATE_DICTS_ACTION,

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

        // clasify me
        ENABLE_SMS_ASK,
        ENABLE_SMS_DO,
        ENABLE_BT_DO,
        __LAST
    }

    public static class ActionPair {
        public ActionPair( Action act, int str ) { 
            buttonStr = str; action = act;
        }
        public int buttonStr;
        public Action action;
        public Object[] params; // null for now
    }

    public static final int SMS_BTN = AlertDialog.BUTTON_POSITIVE;
    public static final int NFC_BTN = AlertDialog.BUTTON_NEUTRAL;
    public static final int DISMISS_BUTTON = 0;

    private static final String IDS = "IDS";
    private static final String STATE_KEYF = "STATE_%d";

    public interface DlgClickNotify {
        public static enum InviteMeans {
            SMS, EMAIL, NFC, BLUETOOTH,
        };
        void dlgButtonClicked( Action action, int button, Object[] params );
        void inviteChoiceMade( Action action, InviteMeans means, Object[] params );
    }
    public interface HasDlgDelegate {
        void showOKOnlyDialog( int msgID );
        void showOKOnlyDialog( String msg );
        void showNotAgainDlgThen( int msgID, int prefsKey, Action action );
    }

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
        if ( !m_activity.isFinishing() ) {
            int id = dlgID.ordinal();
            m_activity.showDialog( id );
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
        case DIALOG_ABOUT:
            dialog = createAboutDialog();
            break;
        case DIALOG_OKONLY:
            dialog = createOKDialog( state, dlgID );
            break;
        case DIALOG_NOTAGAIN:
            dialog = createNotAgainDialog( state, dlgID );
            break;
        case CONFIRM_THEN:
            dialog = createConfirmThenDialog( state, dlgID );
            break;
        case INVITE_CHOICES_THEN:
            dialog = createInviteChoicesDialog( state, dlgID );
            break;
        case DLG_DICTGONE:
            dialog = createDictGoneDialog();
            break;
        case DIALOG_ENABLESMS:
            dialog = createEnableSMSDialog( state, dlgID );
            break;
        }
        return dialog;
    }

    public void prepareDialog( DlgID dlgId, Dialog dialog )
    {
        switch( dlgId ) {
        case INVITE_CHOICES_THEN:
            prepareInviteChoicesDialog( dialog );
            break;
        }
    }

    public void showOKOnlyDialog( String msg )
    {
        showOKOnlyDialog( msg, Action.SKIP_CALLBACK );
    }

    public void showOKOnlyDialog( String msg, Action action )
    {
        // Assert.assertNull( m_dlgStates );
        DlgState state = new DlgState( DlgID.DIALOG_OKONLY, msg, action );
        addState( state );
        showDialog( DlgID.DIALOG_OKONLY );
    }

    public void showOKOnlyDialog( int msgID )
    {
        showOKOnlyDialog( getString( msgID ), Action.SKIP_CALLBACK );
    }

    public void showDictGoneFinish()
    {
        showDialog( DlgID.DLG_DICTGONE );
    }

    public void showAboutDialog()
    {
        showDialog( DlgID.DIALOG_ABOUT );
    }

    // Puts up alert asking to choose a reason to enable SMS, and on dismiss
    // calls dlgButtonClicked with the action and in params a Boolean
    // indicating whether enabling is now ok.
    public void showSMSEnableDialog( Action action )
    {
        DlgState state = new DlgState( DlgID.DIALOG_ENABLESMS, action );
        addState( state );
        showDialog( DlgID.DIALOG_ENABLESMS );
    }

    public void showNotAgainDlgThen( String msg, int prefsKey, 
                                     final Action action, ActionPair more, 
                                     final Object[] params )
    {
        if ( XWPrefs.getPrefsBoolean( m_activity, prefsKey, false ) ) {
            // If it's set, do the action without bothering with the
            // dialog
            if ( Action.SKIP_CALLBACK != action ) {
                post( new Runnable() {
                        public void run() {
                            m_clickCallback
                                .dlgButtonClicked( action,
                                                   AlertDialog.BUTTON_POSITIVE,
                                                   params );
                        }
                    });
            }
        } else {
            DlgState state = 
                new DlgState( DlgID.DIALOG_NOTAGAIN, msg, prefsKey, action, more, 
                              params );
            addState( state );
            showDialog( DlgID.DIALOG_NOTAGAIN );
        }
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, Action action, 
                                     ActionPair more, Object[] params )
    {
        showNotAgainDlgThen( getString( msgID ), prefsKey, action, more, 
                             params );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, Action action )
    {
        showNotAgainDlgThen( msgID, prefsKey, action, null, null );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, Action action, 
                                     ActionPair more )
    {
        showNotAgainDlgThen( msgID, prefsKey, action, more, null );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey )
    {
        showNotAgainDlgThen( msgID, prefsKey, Action.SKIP_CALLBACK );
    }

    public void showConfirmThen( String msg, Action action )
    {
        showConfirmThen( null, msg, android.R.string.ok, action, null );
    }

    public void showConfirmThen( int msgID, Action action )
    {
        showConfirmThen( null, getString( msgID ), android.R.string.ok, action, null );
    }

    public void showConfirmThen( Runnable onNA, String msg, Action action, Object[] params )
    {
        showConfirmThen( onNA, msg, android.R.string.ok, action, params );
    }

    public void showConfirmThen( Runnable onNA, String msg, int posButton, Action action )
    {
        showConfirmThen( onNA, msg, posButton, action, null );
    }

    public void showConfirmThen( int msg, int posButton, int negButton, Action action )
    {
        showConfirmThen( null, getString(msg), posButton, negButton, action, null );
    }

    public void showConfirmThen( int msg, int posButton, Action action,
                                 Object[] params )
    {
        showConfirmThen( null, getString(msg), posButton, android.R.string.cancel, 
                         action, params );
    }

    public void showConfirmThen( Runnable onNA, String msg, int posButton, Action action,
                                 Object[] params )
    {
        showConfirmThen( onNA, msg, posButton, android.R.string.cancel, action, 
                         params );
    }

    public void showConfirmThen( Runnable onNA, String msg, int posButton, 
                                 int negButton, Action action, Object[] params )
    {
        DlgState state = new DlgState( DlgID.CONFIRM_THEN, onNA, msg, posButton, 
                                       negButton, action, 0, params );
        addState( state );
        showDialog( DlgID.CONFIRM_THEN );
    }

    public void showInviteChoicesThen( final Action action )
    {
        if ( (XWApp.SMS_INVITE_ENABLED && Utils.deviceSupportsSMS( m_activity ))
             || XWPrefs.getNFCToSelfEnabled( m_activity )
             || NFCUtils.nfcAvail( m_activity )[0]
             || BTService.BTAvailable() ) {
            DlgState state = new DlgState( DlgID.INVITE_CHOICES_THEN, action );
            addState( state );
            showDialog( DlgID.INVITE_CHOICES_THEN );
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
            showOKOnlyDialog( R.string.no_games_to_refresh );
        } else {
            RelayService.timerFired( m_activity );
            Utils.showToast( m_activity, R.string.msgs_progress );
        }
    }

    public void launchLookup( String[] words, int lang, boolean noStudy )
    {
        if ( LookupAlert.needAlert( m_activity, words, lang, noStudy ) ) {
            Bundle params = LookupAlert.makeParams( words, lang, noStudy );
            addState( new DlgState( DlgID.LOOKUP, new Object[]{params} ) );
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
            DbgUtils.logf( "eventOccurred: unhandled event %s", event.toString() );
        }

        if ( null != msg ) {
            final String fmsg = msg;
            final boolean asDlg = !asToast;
            post( new Runnable() {
                    public void run() {
                        if ( asDlg ) {
                            showOKOnlyDialog( fmsg, Action.SKIP_CALLBACK );
                        } else {
                            DbgUtils.showf( m_activity, fmsg );
                        }
                    }
                } );
        }
    }

    private Dialog createAboutDialog()
    {
        final View view = LocUtils.inflate( m_activity, R.layout.about_dlg );
        TextView vers = (TextView)view.findViewById( R.id.version_string );

        DateFormat df = DateFormat.getDateTimeInstance( DateFormat.FULL, 
                                                        DateFormat.FULL );
        String dateString
            = df.format( new Date( BuildConstants.BUILD_STAMP * 1000 ) );
        vers.setText( getString( R.string.about_vers_fmt,
                                 getString( R.string.app_version ),
                                 BuildConstants.GIT_REV, dateString ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = getString( R.string.xlator );
        if ( str.length() > 0 && !str.equals("[empty]") ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity );
        builder.setIcon( R.drawable.icon48x48 );
        builder.setTitle( R.string.app_name );
        builder.setView( view );
        builder.setNegativeButton( R.string.changes_button,
                                   new OnClickListener() {
                                       @Override
                                       public void onClick( DialogInterface dlg, 
                                                            int which )
                                       {
                                           FirstRunDialog.show( m_activity );
                                       }
                                   } );
        builder.setPositiveButton( android.R.string.ok, null );
        return builder.create();
    }

    private Dialog createLookupDialog()
    {
        DlgState state = findForID( DlgID.LOOKUP );
        Bundle bundle = (Bundle)state.m_params[0];
        return LookupAlert.makeDialog( m_activity, bundle );
    }

    private Dialog createOKDialog( DlgState state, DlgID dlgID )
    {
        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity );
        builder.setTitle( R.string.info_title );
        builder.setMessage( state.m_msg );
        builder.setPositiveButton( android.R.string.ok, null );
        Dialog dialog = builder.create();
        dialog = setCallbackDismissListener( dialog, state, dlgID );

        return dialog;
    }

    private Dialog createNotAgainDialog( final DlgState state, DlgID dlgID )
    {
        NotAgainView naView = (NotAgainView)
            LocUtils.inflate( m_activity, R.layout.not_again_view );
        naView.setMessage( state.m_msg );
        final OnClickListener lstnr_p = mkCallbackClickListener( state, naView );

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.newbie_title )
            .setView( naView )
            .setPositiveButton( android.R.string.ok, lstnr_p );

        // Adding third button doesn't work for some reason. Either this
        // feature goes away or the "do not show again" becomes a checkbox as
        // many apps do it.
        if ( null != state.m_pair ) {
            final ActionPair more = state.m_pair;
            OnClickListener lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        m_clickCallback.
                            dlgButtonClicked( more.action, 
                                              AlertDialog.BUTTON_POSITIVE,
                                              more.params );
                        lstnr_p.onClick( dlg, AlertDialog.BUTTON_POSITIVE );
                    }
                };
            builder.setNegativeButton( more.buttonStr, lstnr );
        }

        Dialog dialog = builder.create();

        return setCallbackDismissListener( dialog, state, dlgID );
    } // createNotAgainDialog

    private Dialog createConfirmThenDialog( DlgState state, DlgID dlgID )
    {
        NotAgainView naView = (NotAgainView)
            LocUtils.inflate( m_activity, R.layout.not_again_view );
        naView.setMessage( state.m_msg );
        naView.setShowNACheckbox( null != state.m_onNAChecked );
        OnClickListener lstnr = mkCallbackClickListener( state, naView );

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.query_title )
            .setView( naView )
            .setPositiveButton( state.m_posButton, lstnr )
            .setNegativeButton( state.m_negButton, lstnr );
        Dialog dialog = builder.create();
        
        return setCallbackDismissListener( dialog, state, dlgID );
    }

    private Dialog createInviteChoicesDialog( final DlgState state, DlgID dlgID )
    {
        final ArrayList<DlgClickNotify.InviteMeans> means = 
            new ArrayList<DlgClickNotify.InviteMeans>();
        ArrayList<String> items = new ArrayList<String>();
        if ( XWApp.SMS_INVITE_ENABLED && Utils.deviceSupportsSMS(m_activity) ) {
            items.add( getString( R.string.invite_choice_sms ) );
            means.add( DlgClickNotify.InviteMeans.SMS );
        }
        items.add( getString( R.string.invite_choice_email ) );
        means.add( DlgClickNotify.InviteMeans.EMAIL );
        if ( BTService.BTAvailable() ) {
            items.add( getString( R.string.invite_choice_bt ) );
            means.add( DlgClickNotify.InviteMeans.BLUETOOTH );
        }
        if ( XWPrefs.getNFCToSelfEnabled( m_activity ) 
             || NFCUtils.nfcAvail( m_activity )[0] ) {
            items.add( getString( R.string.invite_choice_nfc ) );
            means.add( DlgClickNotify.InviteMeans.NFC );
        }

        final int[] sel = { -1 };
        OnClickListener selChanged = new OnClickListener() {
                public void onClick( DialogInterface dlg, int view ) {
                    // First time through, enable the button
                    if ( -1 == sel[0] ) {
                        ((AlertDialog)dlg)
                            .getButton( AlertDialog.BUTTON_POSITIVE )
                            .setEnabled( true );
                    }
                    sel[0] = view;
                }
            };
        OnClickListener okClicked = new OnClickListener() {
                public void onClick( DialogInterface dlg, int view ) {
                    Assert.assertTrue( Action.SKIP_CALLBACK != state.m_action );
                    int indx = sel[0];
                    if ( 0 <= indx ) {
                        m_clickCallback.inviteChoiceMade( state.m_action, 
                                                          means.get(indx), 
                                                          state.m_params );
                    }
                }
            };

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.invite_choice_title )
            .setSingleChoiceItems( items.toArray( new String[items.size()] ), 
                                   sel[0], selChanged )
            .setPositiveButton( android.R.string.ok, okClicked )
            .setNegativeButton( android.R.string.cancel, null );

        return setCallbackDismissListener( builder.create(), state, dlgID );
    }

    private void prepareInviteChoicesDialog( Dialog dialog )
    {
        AlertDialog ad = (AlertDialog)dialog;
        Button button = ad.getButton( AlertDialog.BUTTON_POSITIVE );
        if ( null != button ) {
            button.setEnabled( false );
        }
    }

    private Dialog createDictGoneDialog()
    {
        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity );
        builder.setTitle( R.string.no_dict_title );
        builder.setMessage( R.string.no_dict_finish );
        builder.setPositiveButton( R.string.button_close_game, null );
        Dialog dialog = builder.create();

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
                    Object[] params = { new Boolean(enabled), };
                    m_clickCallback.dlgButtonClicked( state.m_action, 
                                                      AlertDialog.BUTTON_POSITIVE,
                                                      params );
                }
            };

        Dialog dialog = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.confirm_sms_title )
            .setView( layout )
            .setPositiveButton( android.R.string.ok, lstnr )
            .create();
        Utils.setRemoveOnDismiss( m_activity, dialog, dlgID );
        return dialog;
    }

    private OnClickListener mkCallbackClickListener( final DlgState state,
                                                     final NotAgainView naView )
    {
        OnClickListener cbkOnClickLstnr;
        cbkOnClickLstnr = new OnClickListener() {
                public void onClick( DialogInterface dlg, int button ) {
                    if ( null != naView && naView.getChecked() ) {
                        if ( 0 != state.m_prefsKey ) {
                            XWPrefs.setPrefsBoolean( m_activity, state.m_prefsKey, 
                                                     true );
                        } else if ( null != state.m_onNAChecked ) {
                            state.m_onNAChecked.run();
                        }
                    }

                    if ( Action.SKIP_CALLBACK != state.m_action ) {
                        m_clickCallback.dlgButtonClicked( state.m_action, 
                                                          button, 
                                                          state.m_params );
                    }
                }
            };
        return cbkOnClickLstnr;
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
                            m_clickCallback.dlgButtonClicked( state.m_action, 
                                                              DISMISS_BUTTON, 
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

    private String getString( int id, Object... params )
    {
        return m_dlgt.getString( id, params );
    }

    private String getQuantityString( int id, int quantity, Object... params )
    {
        return m_dlgt.getQuantityString( id, quantity, params );
    }
}

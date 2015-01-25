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
import android.widget.TextView;
import android.widget.Toast;
import java.util.ArrayList;
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
        SET_HIDE_NEWGAME_BUTTONS,

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
        BT_PICK_ACTION,
        SMS_PICK_ACTION,
        SMS_CONFIG_ACTION,
        BUTTON_BROWSEALL_ACTION,

        // Dict Browser
        FINISH_ACTION,
        DELETE_DICT_ACTION,
        UPDATE_DICTS_ACTION,

        // Game configs
        LOCKED_CHANGE_ACTION,

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
    public static final int EMAIL_BTN = AlertDialog.BUTTON_NEGATIVE;
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
        int id = dlgID.ordinal();
        m_activity.showDialog( id );
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
        }
        return dialog;
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

    public void showNotAgainDlgThen( String msg, int prefsKey,
                                     ActionPair pair, final Action action,
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
                new DlgState( DlgID.DIALOG_NOTAGAIN, msg, prefsKey, pair, action, 
                              params );
            addState( state );
            showDialog( DlgID.DIALOG_NOTAGAIN );
        }
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, ActionPair pair,
                                     Action action, Object[] params )
    {
        showNotAgainDlgThen( getString( msgID ), prefsKey, pair, action, 
                             params );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, Action action )
    {
        showNotAgainDlgThen( msgID, prefsKey, null, action, null );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, ActionPair pair )
    {
        showNotAgainDlgThen( msgID, prefsKey, pair, Action.SKIP_CALLBACK, null );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey )
    {
        showNotAgainDlgThen( msgID, prefsKey, Action.SKIP_CALLBACK );
    }

    public void showConfirmThen( String msg, Action action )
    {
        showConfirmThen( msg, R.string.button_ok, action, null );
    }

    public void showConfirmThen( int msgID, Action action )
    {
        showConfirmThen( getString( msgID ), R.string.button_ok, action, null );
    }

    public void showConfirmThen( String msg, Action action, Object[] params )
    {
        showConfirmThen( msg, R.string.button_ok, action, params );
    }

    public void showConfirmThen( String msg, int posButton, Action action )
    {
        showConfirmThen( msg, posButton, action, null );
    }

    public void showConfirmThen( int msg, int posButton, Action action,
                                 Object[] params )
    {
        showConfirmThen( getString( msg ), posButton, action, params );
    }

    public void showConfirmThen( String msg, int posButton, Action action,
                                 Object[] params )
    {
        DlgState state = new DlgState( DlgID.CONFIRM_THEN, msg, posButton, 
                                       action, 0, params );
        addState( state );
        showDialog( DlgID.CONFIRM_THEN );
    }

    public void showInviteChoicesThen( final Action action )
    {
        if ( (XWApp.SMS_INVITE_ENABLED && Utils.deviceSupportsSMS( m_activity ))
             || NFCUtils.nfcAvail( m_activity )[0]
             || BTService.BTAvailable() ) {
            DlgState state = new DlgState( DlgID.INVITE_CHOICES_THEN, action );
            addState( state );
            showDialog( DlgID.INVITE_CHOICES_THEN );
        } else {
            post( new Runnable() {
                    public void run() {
                        m_clickCallback.dlgButtonClicked( action, EMAIL_BTN,
                                                          null );
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
        vers.setText( getString( R.string.about_vers_fmt,
                                 getString( R.string.app_version ),
                                 BuildConstants.GIT_REV, 
                                 BuildConstants.BUILD_STAMP ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = getString( R.string.xlator );
        if ( str.length() > 0 ) {
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
        builder.setPositiveButton( R.string.button_ok, null );
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
        builder.setPositiveButton( R.string.button_ok, null );
        Dialog dialog = builder.create();
        dialog = setCallbackDismissListener( dialog, state, dlgID );

        return dialog;
    }

    private Dialog createNotAgainDialog( final DlgState state, DlgID dlgID )
    {
        OnClickListener lstnr_p = mkCallbackClickListener( state );

        OnClickListener lstnr_n = 
            new OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    XWPrefs.setPrefsBoolean( m_activity, state.m_prefsKey, 
                                             true );
                    if ( Action.SKIP_CALLBACK != state.m_action ) {
                        m_clickCallback.
                            dlgButtonClicked( state.m_action, 
                                              AlertDialog.BUTTON_POSITIVE, 
                                              state.m_params );
                    }
                }
            };

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.newbie_title )
            .setMessage( state.m_msg )
            .setPositiveButton( R.string.button_ok, lstnr_p )
            .setNegativeButton( R.string.button_notagain, lstnr_n );
        if ( null != state.m_pair ) {
            final ActionPair pair = state.m_pair;
            OnClickListener lstnr = new OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        m_clickCallback.
                            dlgButtonClicked( pair.action, 
                                              AlertDialog.BUTTON_POSITIVE,
                                              pair.params );
                    }
                };
            builder.setNeutralButton( pair.buttonStr, lstnr );
        }
        Dialog dialog = builder.create();

        return setCallbackDismissListener( dialog, state, dlgID );
    } // createNotAgainDialog

    private Dialog createConfirmThenDialog( DlgState state, DlgID dlgID )
    {
        OnClickListener lstnr = mkCallbackClickListener( state );

        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity );
        builder.setTitle( R.string.query_title );
        builder.setMessage( state.m_msg );
        builder.setPositiveButton( state.m_posButton, lstnr );
        builder.setNegativeButton( R.string.button_cancel, lstnr );
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
        if ( NFCUtils.nfcAvail( m_activity )[0] ) {
            items.add( getString( R.string.invite_choice_nfc ) );
            means.add( DlgClickNotify.InviteMeans.NFC );
        }

        final int[] sel = { -1 };
        OnClickListener selChanged = new OnClickListener() {
                public void onClick( DialogInterface dlg, int view ) {
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
            .setPositiveButton( R.string.button_ok, okClicked )
            .setNegativeButton( R.string.button_cancel, null );

        return setCallbackDismissListener( builder.create(), state, dlgID );
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

    private OnClickListener mkCallbackClickListener( final DlgState state )
    {
        OnClickListener cbkOnClickLstnr;
        cbkOnClickLstnr = new OnClickListener() {
                public void onClick( DialogInterface dlg, int button ) {
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
}

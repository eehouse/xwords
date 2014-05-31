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
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;
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
        DOWNLOAD_DICT_ACTION,

        // Game configs
        LOCKED_CHANGE_ACTION,

        // New Game
        NEW_GAME_ACTION,

        // SMS invite
        CLEAR_ACTION,
        USE_IMMOBILE_ACTION,
        POST_WARNING_ACTION,

        // Study list
        SL_CLEAR_ACTION,
        SL_COPY_ACTION,

        __LAST
    }

    public static final int SMS_BTN = AlertDialog.BUTTON_POSITIVE;
    public static final int EMAIL_BTN = AlertDialog.BUTTON_NEGATIVE;
    public static final int NFC_BTN = AlertDialog.BUTTON_NEUTRAL;
    public static final int DISMISS_BUTTON = 0;

    private static final String IDS = "IDS";
    private static final String STATE_KEYF = "STATE_%d";

    public interface DlgClickNotify {
        void dlgButtonClicked( Action action, int button, Object[] params );
    }
    public interface HasDlgDelegate {
        void showOKOnlyDialog( int msgID );
        void showOKOnlyDialog( String msg );
        void showNotAgainDlgThen( int msgID, int prefsKey, Action action );
    }

    private Activity m_activity;
    private DlgClickNotify m_clickCallback;
    private String m_dictName = null;
    private ProgressDialog m_progress;
    private Handler m_handler;

    private HashMap<DlgID, DlgState> m_dlgStates;

    public DlgDelegate( Activity activity, DlgClickNotify callback,
                        Bundle bundle ) 
    {
        m_activity = activity;
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
        m_activity.showDialog( dlgID.ordinal() );
    }
    
    public Dialog createDialog( int id )
    {
        // DbgUtils.logf("createDialog(id=%d)", id );
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
        m_activity.showDialog( DlgID.DIALOG_OKONLY.ordinal() );
    }

    public void showOKOnlyDialog( int msgID )
    {
        showOKOnlyDialog( LocUtils.getString( m_activity, msgID ), Action.SKIP_CALLBACK );
    }

    public void showDictGoneFinish()
    {
        m_activity.showDialog( DlgID.DLG_DICTGONE.ordinal() );
    }

    public void showAboutDialog()
    {
        m_activity.showDialog( DlgID.DIALOG_ABOUT.ordinal() );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey,
                                     final Action action,
                                     final Object[] params )
    {
        showNotAgainDlgThen( LocUtils.getString( m_activity, msgID ), prefsKey, 
                             action, params );
    }

    public void showNotAgainDlgThen( String msg, int prefsKey,
                                     final Action action,
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
                new DlgState( DlgID.DIALOG_NOTAGAIN, msg, action, prefsKey, 
                              params );
            addState( state );
            m_activity.showDialog( DlgID.DIALOG_NOTAGAIN.ordinal() );
        }
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey,
                                     Action action)
    {
        showNotAgainDlgThen( msgID, prefsKey, action, null );
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
        showConfirmThen( LocUtils.getString( m_activity, msgID ),
                         R.string.button_ok, action, null );
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
        showConfirmThen( LocUtils.getString( m_activity, msg ), posButton, action, 
                         params );
    }

    public void showConfirmThen( String msg, int posButton, Action action,
                                 Object[] params )
    {
        DlgState state = new DlgState( DlgID.CONFIRM_THEN, msg, posButton, 
                                       action, 0, params );
        addState( state );
        m_activity.showDialog( DlgID.CONFIRM_THEN.ordinal() );
    }

    public void showInviteChoicesThen( final Action action )
    {
        if ( Utils.deviceSupportsSMS( m_activity )
             || NFCUtils.nfcAvail( m_activity )[0] ) {
            DlgState state = new DlgState( DlgID.INVITE_CHOICES_THEN, action );
            addState( state );
            m_activity.showDialog( DlgID.INVITE_CHOICES_THEN.ordinal() );
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

    public void launchLookup( String[] words, int lang, boolean noStudyOption )
    {
        Bundle params = LookupAlert.makeParams( words, lang, noStudyOption );
        addState( new DlgState( DlgID.LOOKUP, new Object[]{params} ) );
        m_activity.showDialog( DlgID.LOOKUP.ordinal() );
    }

    public void startProgress( int id )
    {
        String msg = LocUtils.getString( m_activity, id );
        m_progress = ProgressDialog.show( m_activity, msg, null, true, true );
    }

    public void stopProgress()
    {
        if ( null != m_progress ) {
            m_progress.cancel();
            m_progress = null;
        }
    }

    public boolean post( Runnable runnable )
    {
        m_handler.post( runnable );
        return true;
    }

    public void eventOccurred( MultiService.MultiEvent event, final Object ... args )
    {
        String msg = null;
        boolean asToast = true;
        switch( event ) {
        case BAD_PROTO:
            msg = LocUtils.getString( m_activity, R.string.bt_bad_proto_fmt,
                                      (String)args[0] );
            break;
        case MESSAGE_RESEND:
            msg = LocUtils.getString( m_activity, R.string.bt_resend_fmt,
                                      (String)args[0], (Long)args[1], 
                                      (Integer)args[2] );
            break;
        case MESSAGE_FAILOUT:
            msg = LocUtils.getString( m_activity, R.string.bt_fail_fmt, 
                                      (String)args[0] );
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
        vers.setText( String.format( LocUtils.getString( m_activity,
                                                         R.string.about_vers_fmt ),
                                     LocUtils.getString( m_activity,
                                                         R.string.app_version ),
                                     BuildConstants.GIT_REV, 
                                     BuildConstants.BUILD_STAMP ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = LocUtils.getString( m_activity, R.string.xlator );
        if ( str.length() > 0 ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        return LocUtils.makeAlertBuilder( m_activity )
            .setIcon( R.drawable.icon48x48 )
            .setTitle( R.string.app_name )
            .setView( view )
            .setNegativeButton( R.string.changes_button,
                                new OnClickListener() {
                                    @Override
                                    public void onClick( DialogInterface dlg, 
                                                         int which )
                                    {
                                        FirstRunDialog.show( m_activity );
                                    }
                                } )
            .setPositiveButton( R.string.button_ok, null )
            .create();
    }

    private Dialog createLookupDialog()
    {
        DlgState state = findForID( DlgID.LOOKUP );
        Bundle bundle = (Bundle)state.m_params[0];
        return LookupAlert.createDialog( m_activity, bundle );
    }

    private Dialog createOKDialog( DlgState state, DlgID dlgID )
    {
        Dialog dialog = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.info_title )
            .setMessage( state.m_msg )
            .setPositiveButton( R.string.button_ok, null )
            .create();
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

        Dialog dialog = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.newbie_title )
            .setMessage( state.m_msg )
            .setPositiveButton( R.string.button_ok, lstnr_p )
            .setNegativeButton( R.string.button_notagain, lstnr_n )
            .create();

        return setCallbackDismissListener( dialog, state, dlgID );
    } // createNotAgainDialog

    private Dialog createConfirmThenDialog( DlgState state, DlgID dlgID )
    {
        OnClickListener lstnr = mkCallbackClickListener( state );

        Dialog dialog = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.query_title )
            .setMessage( state.m_msg )
            .setPositiveButton( state.m_posButton, lstnr )
            .setNegativeButton( R.string.button_cancel, lstnr )
            .create();
        
        return setCallbackDismissListener( dialog, state, dlgID );
    }

    private Dialog createInviteChoicesDialog( DlgState state, DlgID dlgID )
    {
        OnClickListener lstnr = mkCallbackClickListener( state );

        boolean haveSMS = Utils.deviceSupportsSMS( m_activity );
        boolean haveNFC = NFCUtils.nfcAvail( m_activity )[0];
        int msgID;
        if ( haveSMS && haveNFC ) {
            msgID = R.string.nfc_or_sms_or_email;
        } else if ( haveSMS ) {
            msgID = R.string.sms_or_email; 
        } else {
            msgID = R.string.nfc_or_email;
        }
        
        AlertDialog.Builder builder = LocUtils.makeAlertBuilder( m_activity )
            .setTitle( R.string.query_title )
            .setMessage( msgID )
            .setNegativeButton( R.string.button_html, lstnr );

        if ( haveSMS ) {
            builder.setPositiveButton( R.string.button_text, lstnr );
        }
        if ( haveNFC ) {
            builder.setNeutralButton( R.string.button_nfc, lstnr );
        }

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

}

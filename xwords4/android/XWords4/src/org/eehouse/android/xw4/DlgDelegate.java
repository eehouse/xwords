/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

public class DlgDelegate {

    public static final int DIALOG_ABOUT = 1;
    public static final int DIALOG_OKONLY = 2;
    public static final int DIALOG_NOTAGAIN = 3;
    public static final int CONFIRM_THEN = 4;
    public static final int TEXT_OR_HTML_THEN = 5;
    public static final int DLG_DICTGONE = 6;
    public static final int DIALOG_LAST = DLG_DICTGONE;

    public static final int SMS_BTN = AlertDialog.BUTTON_POSITIVE;
    public static final int EMAIL_BTN = AlertDialog.BUTTON_NEGATIVE;
    public static final int DISMISS_BUTTON = 0;
    public static final int SKIP_CALLBACK = -1;

    private static final String IDS = "IDS";
    private static final String STATE_KEYF = "STATE_%d";

    public interface DlgClickNotify {
        void dlgButtonClicked( int id, int button );
    }

    private Activity m_activity;
    private DlgClickNotify m_clickCallback;
    private String m_dictName = null;
    private ProgressDialog m_progress;
    private Handler m_handler;

    private HashMap<Integer, DlgState> m_dlgStates;

    public DlgDelegate( Activity activity, DlgClickNotify callback,
                        Bundle bundle ) 
    {
        m_activity = activity;
        m_clickCallback = callback;
        m_handler = new Handler();
        m_dlgStates = new HashMap<Integer,DlgState>();

        if ( null != bundle ) {
            int[] ids = bundle.getIntArray( IDS );
            for ( int id : ids ) {
                String key = String.format( STATE_KEYF, id );
                addState( (DlgState)bundle.getSerializable( key ) );
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
                String key = String.format( STATE_KEYF, state.m_id );
                outState.putSerializable( key, state );
                ids[indx++] = state.m_id;
            }
        }
        outState.putIntArray( IDS, ids );
    }
    
    public Dialog onCreateDialog( int id )
    {
        DbgUtils.logf("onCreateDialog(id=%d)", id );
        Dialog dialog = null;
        DlgState state = findForID( id );
        switch( id ) {
        case DIALOG_ABOUT:
            dialog = createAboutDialog();
            break;
        case DIALOG_OKONLY:
            dialog = createOKDialog( state, id );
            break;
        case DIALOG_NOTAGAIN:
            dialog = createNotAgainDialog( state, id );
            break;
        case CONFIRM_THEN:
            dialog = createConfirmThenDialog( state, id );
            break;
        case TEXT_OR_HTML_THEN:
            dialog = createHtmlThenDialog( state, id );
            break;
        case DLG_DICTGONE:
            dialog = createDictGoneDialog();
            break;
        }
        return dialog;
    }

    public void showOKOnlyDialog( String msg )
    {
        showOKOnlyDialog( msg, SKIP_CALLBACK );
    }

    public void showOKOnlyDialog( String msg, int callbackID )
    {
        // Assert.assertNull( m_dlgStates );
        DlgState state = new DlgState( DIALOG_OKONLY, msg, callbackID );
        addState( state );
        // m_msg = msg;
        // if ( 0 != callbackID ) {
        //     Assert.assertTrue( 0 == m_cbckID );
        //     m_cbckID = callbackID;
        // }
        m_activity.showDialog( DIALOG_OKONLY );
    }

    public void showOKOnlyDialog( int msgID )
    {
        showOKOnlyDialog( m_activity.getString( msgID ), SKIP_CALLBACK );
    }

    public void showDictGoneFinish()
    {
        m_activity.showDialog( DLG_DICTGONE );
    }

    public void showAboutDialog()
    {
        m_activity.showDialog( DIALOG_ABOUT );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey,
                                     int callbackID )
    {
        if ( XWPrefs.getPrefsBoolean( m_activity, prefsKey, false ) ) {
            // If it's set, do the action without bothering with the
            // dialog
            if ( SKIP_CALLBACK != callbackID ) {
                m_clickCallback.dlgButtonClicked( callbackID, 
                                                  AlertDialog.BUTTON_POSITIVE );
            }
        } else {
            DlgState state = new DlgState( DIALOG_NOTAGAIN, msgID, callbackID, 
                                           prefsKey );
            addState( state );
            m_activity.showDialog( DIALOG_NOTAGAIN );
        }
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey )
    {
        showNotAgainDlgThen( msgID, prefsKey, SKIP_CALLBACK );
    }

    public void showConfirmThen( String msg, int callbackID )
    {
        showConfirmThen( msg, R.string.button_ok, callbackID );
    }

    public void showConfirmThen( String msg, int posButton, int callbackID )
    {
        // FIX ME!! Need to store data per message rather than have
        // assertions failing or messages dropped.
        if ( false /*0 != m_cbckID*/ ) {
            // DbgUtils.logf( "showConfirmThen: busy with another message; "
            //                + "dropping \"%s\" in favor of \"%s\"", 
            //                msg, m_msg );
        } else {
            DlgState state = new DlgState( CONFIRM_THEN, msg, posButton, 
                                           callbackID, 0 );
            addState( state );
            m_activity.showDialog( CONFIRM_THEN );
        }
    }

    public void showEmailOrSMSThen( final int callbackID )
    {
        if ( Utils.deviceSupportsSMS( m_activity ) ) {
            DlgState state = new DlgState( TEXT_OR_HTML_THEN, callbackID );
            addState( state );
            m_activity.showDialog( TEXT_OR_HTML_THEN );
        } else {
            post( new Runnable() {
                    public void run() {
                        m_clickCallback.dlgButtonClicked( callbackID, EMAIL_BTN );
                    } 
                });
        }
    }

    public void doSyncMenuitem()
    {
        if ( null == DBUtils.getRelayIDs( m_activity, null ) ) {
            showOKOnlyDialog( R.string.no_games_to_refresh );
        } else {
            RelayReceiver.RestartTimer( m_activity, true );
            Utils.showToast( m_activity, R.string.msgs_progress );
        }
    }

    public void launchLookup( String[] words, int lang, boolean forceList )
    {
        Intent intent = new Intent( m_activity, LookupActivity.class );
        intent.putExtra( LookupActivity.WORDS, words );
        intent.putExtra( LookupActivity.LANG, lang );

        m_activity.startActivity( intent );
    }

    public void startProgress( int id )
    {
        String msg = m_activity.getString( id );
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
            msg = Utils.format( m_activity, R.string.bt_bad_protof,
                                       (String)args[0] );
            break;
        case MESSAGE_RESEND:
            msg = Utils.format( m_activity, R.string.bt_resendf,
                                (String)args[0], (Long)args[1], (Integer)args[2] );
            break;
        case MESSAGE_FAILOUT:
            msg = Utils.format( m_activity, R.string.bt_failf, 
                                (String)args[0] );
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
                            showOKOnlyDialog( fmsg, SKIP_CALLBACK );
                        } else {
                            DbgUtils.showf( m_activity, fmsg );
                        }
                    }
                } );
        }
    }

    private Dialog createAboutDialog()
    {
        final View view = Utils.inflate( m_activity, R.layout.about_dlg );
        TextView vers = (TextView)view.findViewById( R.id.version_string );
        vers.setText( String.format( m_activity.getString(R.string.about_versf), 
                                     m_activity.getString(R.string.app_version),
                                     GitVersion.VERS ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = m_activity.getString( R.string.xlator );
        if ( str.length() > 0 ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        return new AlertDialog.Builder( m_activity )
            .setIcon( R.drawable.icon48x48 )
            .setTitle( R.string.app_name )
            .setView( view )
            .setPositiveButton( R.string.changes_button,
                                new OnClickListener() {
                                    @Override
                                    public void onClick( DialogInterface dlg, 
                                                         int which )
                                    {
                                        FirstRunDialog.show( m_activity );
                                    }
                                } )
            .create();
    }

    private Dialog createOKDialog( DlgState state, int id )
    {
        Dialog dialog = new AlertDialog.Builder( m_activity )
            .setTitle( R.string.info_title )
            .setMessage( state.m_msg )
            .setPositiveButton( R.string.button_ok, null )
            .create();
        dialog = setCallbackDismissListener( dialog, state, id );

        return dialog;
    }

    private Dialog createNotAgainDialog( final DlgState state, int id )
    {
        OnClickListener lstnr_p = mkCallbackClickListener( state );

        OnClickListener lstnr_n = 
            new OnClickListener() {
                public void onClick( DialogInterface dlg, int item ) {
                    XWPrefs.setPrefsBoolean( m_activity, state.m_prefsKey, 
                                             true );
                    if ( SKIP_CALLBACK != state.m_cbckID ) {
                        m_clickCallback.
                            dlgButtonClicked( state.m_cbckID, 
                                              AlertDialog.BUTTON_POSITIVE );
                    }
                }
            };

        Dialog dialog = new AlertDialog.Builder( m_activity )
            .setTitle( R.string.newbie_title )
            .setMessage( state.m_msg )
            .setPositiveButton( R.string.button_ok, lstnr_p )
            .setNegativeButton( R.string.button_notagain, lstnr_n )
            .create();

        return setCallbackDismissListener( dialog, state, id );
    } // createNotAgainDialog

    private Dialog createConfirmThenDialog( DlgState state, int id )
    {
        OnClickListener lstnr = mkCallbackClickListener( state );

        Dialog dialog = new AlertDialog.Builder( m_activity )
            .setTitle( R.string.query_title )
            .setMessage( state.m_msg )
            .setPositiveButton( R.string.button_ok, lstnr )
            .setNegativeButton( R.string.button_cancel, lstnr )
            .create();
        
        return setCallbackDismissListener( dialog, state, id );
    }

    private Dialog createHtmlThenDialog( DlgState state, int id )
    {
        OnClickListener lstnr = mkCallbackClickListener( state );
        Dialog dialog = new AlertDialog.Builder( m_activity )
            .setTitle( R.string.query_title )
            .setMessage( R.string.sms_or_email )
            .setPositiveButton( R.string.button_text, lstnr )
            .setNegativeButton( R.string.button_html, lstnr )
            .create();

        return setCallbackDismissListener( dialog, state, id );
    }

    private Dialog createDictGoneDialog()
    {
        Dialog dialog = new AlertDialog.Builder( m_activity )
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
                    if ( SKIP_CALLBACK != state.m_cbckID ) {
                        m_clickCallback.dlgButtonClicked( state.m_cbckID, 
                                                          button );
                    }
                }
            };
        return cbkOnClickLstnr;
    }

    private Dialog setCallbackDismissListener( final Dialog dialog, 
                                               final DlgState state,
                                               final int id )
    {
        DialogInterface.OnDismissListener cbkOnDismissLstnr
            = new DialogInterface.OnDismissListener() {
                    public void onDismiss( DialogInterface di ) {
                        dropState( state );
                        if ( SKIP_CALLBACK != state.m_cbckID ) {
                            m_clickCallback.dlgButtonClicked( state.m_cbckID, 
                                                              DISMISS_BUTTON );
                        }
                        m_activity.removeDialog( id );
                    }
                };

        dialog.setOnDismissListener( cbkOnDismissLstnr );
        return dialog;
    }

    private class DlgState implements java.io.Serializable {
        public int m_id;
        public String m_msg;
        public int m_posButton;
        public int m_cbckID = 0;
        public int m_prefsKey;

        public DlgState( int id, String msg, int cbckID )
        {
            this( id, msg, 0, cbckID, 0 );
        }

        public DlgState( int id, int msgID, int cbckID, int prefsKey )
        {
            this( id, m_activity.getString(msgID), 0, cbckID, prefsKey );
        }

        public DlgState( int id, String msg, int posButton, 
                         int cbckID, int prefsKey )
        {
            m_id = id;
            m_msg = msg;
            m_posButton = posButton;
            m_cbckID = cbckID;
            m_prefsKey = prefsKey;
            DbgUtils.logf( "DlgState(%d)=>%H", id, this );
        }

        public DlgState( int id, int cbckID )
        {
            this( id, null, 0, cbckID, 0 );
        }

    }

    private DlgState findForID( int id )
    {
        DlgState state = m_dlgStates.get( id );
        DbgUtils.logf( "findForID(%d)=>%H", id, state );
        return state;
    }

    private void dropState( DlgState state )
    {
        int nDlgs = m_dlgStates.size();
        Assert.assertNotNull( state );
        Assert.assertTrue( state == m_dlgStates.get( state.m_id ) );
        m_dlgStates.remove( state.m_id );
        DbgUtils.logf( "dropState: active dialogs now %d from %d ", 
                       m_dlgStates.size(), nDlgs );
    }

    private void addState( DlgState state )
    {
        m_dlgStates.put( state.m_id, state );
        DbgUtils.logf( "addState: there are now %d active dialogs", 
                       m_dlgStates.size() );
    }

}

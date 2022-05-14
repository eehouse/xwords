/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.content.Context;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.Builder;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public abstract class DelegateBase implements DlgClickNotify,
                                              DlgDelegate.HasDlgDelegate,
                                              MultiService.MultiEventListener {
    private static final String TAG = DelegateBase.class.getSimpleName();

    private DlgDelegate m_dlgDelegate;
    private Delegator m_delegator;
    private Activity m_activity;
    private int m_optionsMenuID;
    private int m_layoutID;
    private boolean m_finishCalled;
    private View m_rootView;
    private boolean m_isVisible;
    private ArrayList<Runnable> m_visibleProcs = new ArrayList<>();
    private static Map<Class, WeakReference<DelegateBase>> s_instances
        = new HashMap<Class, WeakReference<DelegateBase>>();

    public DelegateBase( Delegator delegator, Bundle bundle, int layoutID )
    {
        this( delegator, bundle, layoutID, R.menu.empty );
    }

    public DelegateBase( Delegator delegator, Bundle bundle,
                         int layoutID, int menuID )
    {
        Assert.assertTrue( 0 < menuID );
        m_delegator = delegator;
        m_activity = delegator.getActivity();
        Assert.assertTrueNR( null != m_activity );
        m_dlgDelegate = new DlgDelegate( m_activity, this, this );
        m_layoutID = layoutID;
        m_optionsMenuID = menuID;
        LocUtils.xlateTitle( m_activity );
    }

    public Activity getActivity() { return m_activity; }

    // Does nothing unless overridden. These belong in an interface.
    protected abstract void init( Bundle savedInstanceState );
    protected void onSaveInstanceState( Bundle outState ) {}
    public boolean onPrepareOptionsMenu( Menu menu ) { return false; }
    public boolean onOptionsItemSelected( MenuItem item ) { return false; }
    protected void onCreateContextMenu( ContextMenu menu, View view,
                                        ContextMenuInfo menuInfo ) {}
    protected boolean onContextItemSelected( MenuItem item ) { return false; }
    protected void onStop() {}
    protected void onDestroy() {}
    protected void onWindowFocusChanged( boolean hasFocus ) {}
    protected boolean handleBackPressed() { return false; }

    protected void requestWindowFeature( int feature ) {}

    protected void tryGetPerms( Perms23.Perm perm, int rationale,
                                Action action, Object... params )
    {
        Perms23.tryGetPerms( this, perm, rationale, action, params );
    }

    // Fragments only
    protected View inflateView( LayoutInflater inflater, ViewGroup container )
    {
        View view = null;
        int layoutID = getLayoutID();
        if ( 0 < layoutID ) {
            view = inflater.inflate( layoutID, container, false );
            LocUtils.xlateView( m_activity, view );
            setContentView( view );
        }
        return view;
    }

    protected void onActivityResult( RequestCode requestCode, int resultCode,
                                     Intent data )
    {
        Log.i( TAG, "onActivityResult(): subclass responsibility!!!" );
    }

    protected void onStart()
    {
        synchronized (s_instances) {
            Class clazz = getClass();
            if ( s_instances.containsKey( clazz ) ) {
                Log.d( TAG, "onStart(): replacing curThis" );
            }
            s_instances.put( clazz, new WeakReference<>(this) );
        }
    }

    protected void onResume()
    {
        m_isVisible = true;
        XWServiceHelper.setListener( this );
        runIfVisible();
        BTUtils.setAmForeground();
    }

    protected void onPause()
    {
        m_isVisible = false;
        XWServiceHelper.clearListener( this );
        m_dlgDelegate.onPausing();
    }

    protected DelegateBase curThis()
    {
        DelegateBase result = null;
        WeakReference<DelegateBase> ref;
        synchronized (s_instances) {
            ref = s_instances.get( getClass() );
        }
        if ( null != ref ) {
            result = ref.get();
        }
        if ( this != result ) {
            Log.d( TAG, "%s.curThis() => " + result, this.toString() );
            Assert.failDbg();
        }
        return result;
    }

    public boolean onCreateOptionsMenu( Menu menu, MenuInflater inflater )
    {
        boolean handled = 0 < m_optionsMenuID;
        if ( handled ) {
            inflater.inflate( m_optionsMenuID, menu );
            LocUtils.xlateMenu( m_activity, menu );
        } else {
            Assert.failDbg();
        }

        return handled;
    }

    public boolean onCreateOptionsMenu( Menu menu )
    {
        MenuInflater inflater = m_activity.getMenuInflater();
        return onCreateOptionsMenu( menu, inflater );
    }

    protected boolean isFinishing()
    {
        boolean result = m_activity.isFinishing();
        // Log.d( TAG, "%s.isFinishing() => %b", getClass().getSimpleName(), result );
        return result;
    }

    protected Intent getIntent()
    {
        return m_activity.getIntent();
    }

    protected Delegator getDelegator() { return m_delegator; }

    protected int getLayoutID()
    {
        return m_layoutID;
    }

    protected Bundle getArguments()
    {
        return m_delegator.getArguments();
    }

    protected View getContentView()
    {
        return m_rootView;
    }

    protected void setContentView( View view )
    {
        LocUtils.xlateView( m_activity, view );
        m_rootView = view;
    }

    protected void setContentView( int resID )
    {
        m_activity.setContentView( resID );
        m_rootView = Utils.getContentView( m_activity );
        LocUtils.xlateView( m_activity, m_rootView );
    }

    protected View findViewById( int resID )
    {
        return m_rootView.findViewById( resID );
    }

    protected void setVisibility( int id, int visibility )
    {
        findViewById( id ).setVisibility( visibility );
    }

    protected void setTitle() {}

    protected void setTitle( String title )
    {
        m_activity.setTitle( title );
    }

    protected String getTitle()
    {
        return m_activity.getTitle().toString();
    }

    protected void startActivityForResult( Intent intent,
                                           RequestCode requestCode )
    {
        m_activity.startActivityForResult( intent, requestCode.ordinal() );
    }

    protected void setResult( int result, Intent intent )
    {
        if ( m_activity instanceof MainActivity ) {
            MainActivity main = (MainActivity)m_activity;
            XWFragment fragment = (XWFragment)m_delegator;
            main.setFragmentResult( fragment, result, intent );
        } else {
            m_activity.setResult( result, intent );
        }
    }

    protected void setResult( int result )
    {
        m_activity.setResult( result );
    }

    protected void onContentChanged()
    {
        m_activity.onContentChanged();
    }

    protected void startActivity( Intent intent )
    {
        m_activity.startActivity( intent );
    }

    protected void finish()
    {
        boolean handled = false;
        if ( m_activity instanceof MainActivity ) {
            MainActivity main = (MainActivity)m_activity;
            if ( !m_finishCalled ) {
                m_finishCalled = true;
                main.finishFragment( (XWFragment)m_delegator );
            }
            handled = true;
        }

        if ( !handled ) {
            m_activity.finish();
        }
    }

    private void runIfVisible()
    {
        if ( isVisible() ) {
            for ( Runnable proc : m_visibleProcs ) {
                post( proc );
            }
            m_visibleProcs.clear();
        }
    }

    void showFaq( String[] params )
    {
        Context context = getActivity();
        String uri = getString( R.string.faq_uri_fmt, params[0], params[1] );
        NetUtils.launchWebBrowserWith( context, uri );
    }

    protected String getString( int resID, Object... params )
    {
        return LocUtils.getString( m_activity, resID, params );
    }

    protected String xlateLang( String langCode )
    {
        return LocUtils.xlateLang( m_activity, langCode );
    }

    protected String xlateLang( String langCode, boolean caps )
    {
        return LocUtils.xlateLang( m_activity, langCode, caps );
    }

    protected String getQuantityString( int resID, int quantity,
                                        Object... params )
    {
        return LocUtils.getQuantityString( m_activity, resID, quantity,
                                           params );
    }

    protected String[] getStringArray( int resID )
    {
        return LocUtils.getStringArray( m_activity, resID );
    }

    protected View inflate( int resID )
    {
        return LocUtils.inflate( m_activity, resID );
    }

    public void invalidateOptionsMenuIf()
    {
        ABUtils.invalidateOptionsMenuIf( m_activity );
    }

    public void showToast( int msg )
    {
        Utils.showToast( m_activity, msg );
    }

    public void showToast( String msg )
    {
        Utils.showToast( m_activity, msg );
    }

    public Object getSystemService( String name )
    {
        return m_activity.getSystemService( name );
    }

    public void runOnUiThread( Runnable runnable )
    {
        m_activity.runOnUiThread( runnable );
    }

    public void setText( View parent, int id, String value )
    {
        EditText editText = (EditText)parent.findViewById( id );
        if ( null != editText ) {
            editText.setText( value, TextView.BufferType.EDITABLE );
        }
    }

    public void setText( int id, String value )
    {
        setText( m_rootView, id, value );
    }

    public String getText( View parent, int id )
    {
        EditText editText = (EditText)parent.findViewById( id );
        return editText.getText().toString();
    }

    public String getText( int id )
    {
        return getText( m_rootView, id );
    }

    public void setInt( int id, int value )
    {
        String str = Integer.toString( value );
        setText( id, str );
    }

    public int getInt( int id )
    {
        int result = 0;
        String str = getText( id );
        try {
            result = Integer.parseInt( str );
        } catch ( NumberFormatException nfe ) {
        }
        return result;
    }


    public void setChecked( int id, boolean value )
    {
        CheckBox cbx = (CheckBox)findViewById( id );
        cbx.setChecked( value );
    }

    public boolean getChecked( int id )
    {
        CheckBox cbx = (CheckBox)findViewById( id );
        return cbx.isChecked();
    }

    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        Dialog dialog = null;
        DlgID dlgID = alert.getDlgID();
        switch ( dlgID ) {
        case DLG_CONNSTAT: {
            AlertDialog.Builder ab = makeAlertBuilder();
            GameSummary summary = (GameSummary)params[0];
            String msg = (String)params[1];
            final CommsConnTypeSet conTypes = summary.conTypes;
            ab.setMessage( msg )
                .setPositiveButton( android.R.string.ok, null );

            boolean showDbg = BuildConfig.NON_RELEASE
                || XWPrefs.getDebugEnabled( m_activity );
            if ( showDbg && null != conTypes ) {
                OnClickListener lstnr = null;
                int buttonTxt = 0;
                if ( conTypes.contains( CommsConnType.COMMS_CONN_MQTT ) ) {
                    buttonTxt = R.string.list_item_relaypage;
                    final int gameID = summary.gameID;
                    if ( BuildConfig.NON_RELEASE ) {
                        NetUtils.gameURLToClip( m_activity, gameID );
                    }
                    lstnr = new OnClickListener() {
                            @Override
                            public void onClick( DialogInterface dlg, int whichButton ) {
                                NetUtils.copyAndLaunchGamePage( m_activity, gameID );
                            }
                        };
                } else if ( conTypes.contains( CommsConnType.COMMS_CONN_RELAY )
                            || conTypes.contains( CommsConnType.COMMS_CONN_P2P ) ) {
                    buttonTxt = R.string.button_reconnect;
                    lstnr = new OnClickListener() {
                            @Override
                            public void onClick( DialogInterface dlg, int buttn ) {
                                NetStateCache.reset( m_activity );
                                if ( conTypes.contains( CommsConnType.COMMS_CONN_RELAY ) ) {
                                    Assert.failDbg();
                                }
                                if ( conTypes.contains( CommsConnType.COMMS_CONN_P2P ) ) {
                                    WiDirService.reset( getActivity() );
                                }
                            }
                        };
                }
                if ( null != lstnr ) {
                    ab.setNegativeButton( buttonTxt, lstnr );
                }
            }
            dialog = ab.create();
        }
            break;
        default:
            Log.d( TAG, "%s.makeDialog(): not handling %s", getClass().getSimpleName(),
                   dlgID.toString() );
            break;
        }
        return dialog;
    }

    protected void showDialogFragment( final DlgID dlgID, final Object... params )
    {
        runOnUiThread( new Runnable() {
                @Override
                public void run() {
                    if ( isFinishing() ) {
                        Log.e( TAG, "not posting dlgID %s b/c %s finishing",
                               dlgID, this );
                        DbgUtils.printStack( TAG );
                    } else {
                        show( DBAlert.newInstance( dlgID, params ) );
                    }
                }
            } );
    }

    protected void show( DlgState state )
    {
        DlgDelegateAlert df = null;
        switch ( state.m_id ) {
        case CONFIRM_THEN:
        case DIALOG_OKONLY:
        case DIALOG_NOTAGAIN:
            df = DlgDelegateAlert.newInstance( state );
            break;
        case DIALOG_ENABLESMS:
            df = EnableSMSAlert.newInstance( state );
            break;
        case INVITE_CHOICES_THEN:
            df = InviteChoicesAlert.newInstance( state );
            break;
        default:
            Assert.failDbg();
        }

        show( df );
    }

    protected void show( XWDialogFragment df )
    {
        DbgUtils.assertOnUIThread();
        if ( null != df && m_activity instanceof XWActivity ) {
            ((XWActivity)m_activity).show( df );
        } else {
            Assert.failDbg();
        }
    }

    protected Dialog buildNamerDlg( Renamer namer, int titleID,
                                    OnClickListener lstnr1, OnClickListener lstnr2,
                                    DlgID dlgID )
    {
        Dialog dialog = makeAlertBuilder()
            .setTitle( titleID )
            .setPositiveButton( android.R.string.ok, lstnr1 )
            .setNegativeButton( android.R.string.cancel, lstnr2 )
            .setView( namer )
            .create();
        return dialog;
    }

    protected AlertDialog.Builder makeAlertBuilder()
    {
        return LocUtils.makeAlertBuilder( m_activity );
    }

    public Builder
        makeNotAgainBuilder( String msg, int key, Action action )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msg, key, action );
    }

    public Builder
        makeNotAgainBuilder( int msgId, int key, Action action )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msgId, key, action );
    }

    public Builder makeNotAgainBuilder( String msg, int key )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msg, key );
    }

    public Builder makeNotAgainBuilder( int msgID, int key )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msgID, key );
    }

    public Builder makeConfirmThenBuilder( String msg, Action action ) {
        return m_dlgDelegate.makeConfirmThenBuilder( msg, action );
    }

    public Builder makeConfirmThenBuilder( int msgId, Action action ) {
        return m_dlgDelegate.makeConfirmThenBuilder( msgId, action );
    }

    protected boolean post( Runnable runnable )
    {
        return m_dlgDelegate.post( runnable );
    }

    protected void launchLookup( String[] words, int lang, boolean noStudy )
    {
        m_dlgDelegate.launchLookup( words, lang, noStudy );
    }

    protected void launchLookup( String[] words, int lang )
    {
        boolean studyOn = XWPrefs.getStudyEnabled( m_activity );
        m_dlgDelegate.launchLookup( words, lang, !studyOn );
    }

    protected void showInviteChoicesThen( Action action, NetLaunchInfo nli,
                                          int nMissing )
    {
        m_dlgDelegate.showInviteChoicesThen( action, nli, nMissing );
    }

    public Builder makeOkOnlyBuilder( int msgID )
    {
        return m_dlgDelegate.makeOkOnlyBuilder( msgID );
    }

    public Builder makeOkOnlyBuilder( String msg )
    {
        return m_dlgDelegate.makeOkOnlyBuilder( msg );
    }

    protected void startProgress( int titleID, int msgID )
    {
        m_dlgDelegate.startProgress( titleID, msgID, null );
    }

    protected void startProgress( int titleID, String msg )
    {
        m_dlgDelegate.startProgress( titleID, msg, null );
    }

    protected void startProgress( String title, String msg )
    {
        m_dlgDelegate.startProgress( title, msg, null );
    }

    protected void startProgress( int titleID, int msgID,
                                  OnCancelListener lstnr )
    {
        m_dlgDelegate.startProgress( titleID, msgID, lstnr );
    }

    protected void startProgress( int titleID, String msg,
                                  OnCancelListener lstnr )
    {
        m_dlgDelegate.startProgress( titleID, msg, lstnr );
    }

    protected void setProgressMsg( int id )
    {
        m_dlgDelegate.setProgressMsg( id );
    }

    protected void stopProgress()
    {
        m_dlgDelegate.stopProgress();
    }

    protected void showSMSEnableDialog( Action action )
    {
        m_dlgDelegate.showSMSEnableDialog( action );
    }

    protected boolean isVisible() { return m_isVisible; }

    protected boolean canHandleNewIntent( Intent intent )
    {
        Log.d( TAG, "canHandleNewIntent() => false" );
        return false;
    }

    protected void handleNewIntent( Intent intent )
    {
        Log.d( TAG, "handleNewIntent(%s): not handling", intent.toString() );
    }

    protected void runWhenActive( Runnable proc )
    {
        m_visibleProcs.add( proc );
        runIfVisible();
    }

    public void onStatusClicked( GamePtr gamePtr )
    {
        Context context = getActivity();
        CommsAddrRec[] addrs = XwJNI.comms_getAddrs( gamePtr );
        CommsAddrRec addr = null != addrs && 0 < addrs.length ? addrs[0] : null;
        final GameSummary summary = GameUtils.getSummary( context, gamePtr.getRowid(), 1 );
        if ( null != summary ) {
            final String msg = ConnStatusHandler
                .getStatusText( context, gamePtr, summary.gameID,
                                summary.conTypes, addr );

            post( new Runnable() {
                    @Override
                    public void run() {
                        if ( null == msg ) {
                            askNoAddrsDelete();
                        } else {
                            showDialogFragment( DlgID.DLG_CONNSTAT, summary, msg );
                        }
                    }
                } );
        }
    }

    public void onStatusClicked( long rowid )
    {
        Log.d( TAG, "onStatusClicked(%d)", rowid );
        try ( GameLock lock = GameLock.tryLockRO( rowid ) ) {
            if ( null != lock ) {
                try ( GamePtr gamePtr = GameUtils
                      .loadMakeGame( getActivity(), lock ) ) {
                    if ( null != gamePtr ) {
                        onStatusClicked( gamePtr );
                    }
                }
            }
        }
    }

    protected void askNoAddrsDelete()
    {
        makeConfirmThenBuilder( R.string.connstat_net_noaddr,
                                Action.DELETE_AND_EXIT )
            .setPosButton( R.string.list_item_delete )
            .setNegButton( R.string.button_close_game )
            .show();
    }

    //////////////////////////////////////////////////
    // MultiService.MultiEventListener interface
    //////////////////////////////////////////////////
    public void eventOccurred( MultiEvent event, final Object ... args )
    {
        int fmtId = 0;
        int notAgainKey = 0;
        switch( event ) {
        case BAD_PROTO_BT:
            fmtId = R.string.bt_bad_proto_fmt;
            notAgainKey = R.string.key_na_bt_badproto;
            break;
        case BAD_PROTO_SMS:
            fmtId = R.string.sms_bad_proto_fmt;
            break;
        case APP_NOT_FOUND_BT:
            fmtId = R.string.app_not_found_fmt;
            break;
        case RELAY_ALERT:
            m_dlgDelegate.eventOccurred( event, args );
            break;
        default:
            Log.d( TAG, "eventOccurred(event=%s) (DROPPED)", event.toString() );
            break;
        }

        if ( 0 != fmtId ) {
            final String msg = getString( fmtId, (String)args[0] );
            final int key = notAgainKey;
            runOnUiThread( new Runnable() {
                    public void run() {
                        Builder builder = 0 == key
                            ? makeOkOnlyBuilder( msg )
                            : makeNotAgainBuilder( msg, key )
                            ;
                        builder.show();
                    }
                });
        }
    }

    //////////////////////////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////////////////////////
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        // Log.d( TAG, "%s.onPosButton(%s)", getClass().getSimpleName(), action );
        switch( action ) {
        case ENABLE_NBS_ASK:
            showSMSEnableDialog( Action.ENABLE_NBS_DO );
            break;
        case ENABLE_NBS_DO:
            XWPrefs.setNBSEnabled( m_activity, true );
            break;
        case ENABLE_BT_DO:
            BTUtils.enable( m_activity );
            break;
        case ENABLE_MQTT_DO:
            XWPrefs.setMQTTEnabled( m_activity, true );
            MQTTUtils.setEnabled( m_activity, true );
            break;
        case PERMS_QUERY:
            Perms23.onGotPermsAction( this, true, params );
            break;
        case PERMS_BANNED_INFO:
            NetUtils.launchWebBrowserWith( m_activity, R.string.nbs_ban_url );
            break;
        case SHOW_FAQ:
            showFaq( (String[])params[0] );
            break;
        default:
            Log.d( TAG, "onPosButton(): unhandled action %s", action.toString() );
            // Assert.assertTrue( !BuildConfig.DEBUG );
            handled = false;
            break;
        }
        return handled;
    }

    public boolean onNegButton( Action action, Object[] params )
    {
        boolean handled = true;
        // Log.d( TAG, "%s.negButtonClicked(%s)", getClass().getSimpleName(),
        //                action.toString() );
        switch ( action ) {
        case PERMS_QUERY:
            Perms23.onGotPermsAction( this, false, params );
            break;
        default:
            Log.d( TAG, "onNegButton: unhandled action %s", action.toString() );
            handled = false;
            break;
        }
        return handled;
    }

    public boolean onDismissed( Action action, Object[] params )
    {
        boolean handled = false;
        Log.d( TAG, "%s.onDismissed(%s)", getClass().getSimpleName(),
               action.toString() );

        switch( action ) {
        case PERMS_QUERY:
            handled = true;
            Perms23.onGotPermsAction( this, false, params );
            break;
        default:
            Log.e( TAG, "onDismissed(): not handling action %s", action );
        }

        return handled;
    }

    public void inviteChoiceMade( Action action, DlgClickNotify.InviteMeans means, Object... params )
    {
        // Assert.fail();
    }

    public static Activity getHasLooper()
    {
        Activity result = null;
        synchronized (s_instances) {
            for ( WeakReference<DelegateBase> ref : s_instances.values() ) {
                DelegateBase base = ref.get();
                if ( null != base ) {
                    result = base.getActivity();
                    break;
                }
            }
        }
        return result;
    }
}

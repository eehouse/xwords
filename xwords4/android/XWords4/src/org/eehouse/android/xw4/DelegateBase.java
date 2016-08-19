/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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
import android.content.DialogInterface.OnCancelListener;
import android.content.Intent;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.DlgDelegate.ConfirmThenBuilder;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify;
import org.eehouse.android.xw4.DlgDelegate.NotAgainBuilder;
import org.eehouse.android.xw4.DlgDelegate.OkOnlyBuilder;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.loc.LocUtils;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public class DelegateBase implements DlgClickNotify,
                                     DlgDelegate.HasDlgDelegate,
                                     MultiService.MultiEventListener {

    private DlgDelegate m_dlgDelegate;
    private Delegator m_delegator;
    private Activity m_activity;
    private int m_optionsMenuID;
    private int m_layoutID;
    private View m_rootView;
    private boolean m_isVisible;
    private ArrayList<Runnable> m_visibleProcs = new ArrayList<Runnable>();
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
        m_dlgDelegate = new DlgDelegate( m_activity, this, this, bundle );
        m_layoutID = layoutID;
        m_optionsMenuID = menuID;
        LocUtils.xlateTitle( m_activity );
    }

    // Does nothing unless overridden. These belong in an interface.
    protected void init( Bundle savedInstanceState ) { Assert.fail(); }
    protected void onSaveInstanceState( Bundle outState ) {}
    public boolean onPrepareOptionsMenu( Menu menu ) { return false; }
    public boolean onOptionsItemSelected( MenuItem item ) { return false; }
    protected void onCreateContextMenu( ContextMenu menu, View view,
                                        ContextMenuInfo menuInfo ) {}
    protected boolean onContextItemSelected( MenuItem item ) { return false; }
    protected void onDestroy() {}
    protected void onWindowFocusChanged( boolean hasFocus ) {}
    protected boolean handleBackPressed() { return false; }
    public void orientationChanged() {}

    protected void requestWindowFeature( int feature ) {}

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
        DbgUtils.logf( "%s.onActivityResult(): subclass responsibility!!!", 
                       getClass().getSimpleName() );
    }

    protected void onStart()
    {
        if ( s_instances.containsKey(getClass()) ) {
            DbgUtils.logdf( "%s.onStart(): replacing curThis", 
                            getClass().getSimpleName() );
        }
        s_instances.put( getClass(), new WeakReference<DelegateBase>(this) );
    }

    protected void onResume()
    {
        m_isVisible = true;
        XWService.setListener( this );
        runIfVisible();
    }

    protected void onPause()
    {
        m_isVisible = false;
        XWService.setListener( null );
    }

    protected void onStop()
    {
        // Alerts disappear on their own if not in dualpane mode
        if ( m_activity instanceof MainActivity
             && ((MainActivity)m_activity).inDPMode() ) {
            DlgDelegate.closeAlerts( m_activity, this );
        }
    }

    protected DelegateBase curThis()
    {
        DelegateBase result = null;
        WeakReference<DelegateBase> ref = s_instances.get( getClass() );
        if ( null != ref ) {
            result = ref.get();
        }
        if ( this != result ) {
            DbgUtils.logdf( "%s.curThis() => " + result, this.toString() );
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
            Assert.fail();
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
        // DbgUtils.logf( "%s.isFinishing() => %b", getClass().getSimpleName(), result );
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
            if ( main.inDPMode() ) {
                main.finishFragment();
                handled = true;
            }
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

    public void setText( int id, String value )
    {
        EditText editText = (EditText)findViewById( id );
        if ( null != editText ) {
            editText.setText( value, TextView.BufferType.EDITABLE );
        }
    }

    public String getText( int id )
    {
        EditText editText = (EditText)findViewById( id );
        return editText.getText().toString();
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

    protected void showDialog( DlgID dlgID )
    {
        m_dlgDelegate.showDialog( dlgID );
    }

    protected void removeDialog( DlgID dlgID )
    {
        removeDialog( dlgID.ordinal() );
    }

    protected void dismissDialog( DlgID dlgID )
    {
        try {
            m_activity.dismissDialog( dlgID.ordinal() );
        } catch ( Exception ex ) {
            // DbgUtils.loge( ex );
        }
    }

    protected void removeDialog( int id )
    {
        m_activity.removeDialog( id );
    }

    protected Dialog onCreateDialog( int id )
    {
        return m_dlgDelegate.createDialog( id );
    }

    protected void prepareDialog( DlgID dlgId, Dialog dialog )
    {
        m_dlgDelegate.prepareDialog( dlgId, dialog );
    }

    protected AlertDialog.Builder makeAlertBuilder()
    {
        return LocUtils.makeAlertBuilder( m_activity );
    }

    protected void setRemoveOnDismiss( Dialog dialog, DlgID dlgID )
    {
        Utils.setRemoveOnDismiss( m_activity, dialog, dlgID );
    }

    public NotAgainBuilder
        makeNotAgainBuilder( String msg, int key, Action action )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msg, key, action );
    }

    public NotAgainBuilder
        makeNotAgainBuilder( int msgId, int key, Action action )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msgId, key, action );
    }

    public NotAgainBuilder makeNotAgainBuilder( String msg, int key )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msg, key );
    }

    public NotAgainBuilder makeNotAgainBuilder( int msgId, int key )
    {
        return m_dlgDelegate.makeNotAgainBuilder( msgId, key );
    }

    // It sucks that these must be duplicated here and XWActivity
    protected void showAboutDialog()
    {
        m_dlgDelegate.showAboutDialog();
    }

    public ConfirmThenBuilder makeConfirmThenBuilder( String msg, Action action ) {
        return m_dlgDelegate.makeConfirmThenBuilder( msg, action );
    }

    public ConfirmThenBuilder makeConfirmThenBuilder( int msgId, Action action ) {
        return m_dlgDelegate.makeConfirmThenBuilder( msgId, action );
    }

    protected boolean post( Runnable runnable )
    {
        return m_dlgDelegate.post( runnable );
    }

    protected void doSyncMenuitem()
    {
        m_dlgDelegate.doSyncMenuitem();
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

    protected void showInviteChoicesThen( Action action,
                                          DBUtils.SentInvitesInfo info )
    {
        m_dlgDelegate.showInviteChoicesThen( action, info );
    }

    public OkOnlyBuilder makeOkOnlyBuilder( int msgId )
    {
        return m_dlgDelegate.makeOkOnlyBuilder( msgId );
    }

    public OkOnlyBuilder makeOkOnlyBuilder( String msg )
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

    protected void showDictGoneFinish()
    {
        m_dlgDelegate.showDictGoneFinish();
    }

    protected void showSMSEnableDialog( Action action, Object... params )
    {
        m_dlgDelegate.showSMSEnableDialog( action, params );
    }

    protected boolean isVisible() { return m_isVisible; }

    protected boolean handleNewIntent( Intent intent ) {
        DbgUtils.logf( "%s.handleNewIntent(%s): not handling",
                       getClass().getSimpleName(), intent.toString() );
        return false;           // not handled
    }

    protected void runWhenActive( Runnable proc )
    {
        m_visibleProcs.add( proc );
        runIfVisible();
    }

    //////////////////////////////////////////////////
    // MultiService.MultiEventListener interface
    //////////////////////////////////////////////////
    public void eventOccurred( MultiEvent event, final Object ... args )
    {
        int fmtId = 0;
        switch( event ) {
        case BT_ERR_COUNT:
            int count = (Integer)args[0];
            DbgUtils.logf( "Bluetooth error count: %d", count );
            break;
        case BAD_PROTO_BT:
            fmtId = R.string.bt_bad_proto_fmt;
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
            DbgUtils.logdf( "DelegateBase.eventOccurred(event=%s) (DROPPED)",
                            event.toString() );
            break;
        }

        if ( 0 != fmtId ) {
            final String msg = getString( fmtId, (String)args[0] );
            runOnUiThread( new Runnable() {
                    public void run() {
                        makeOkOnlyBuilder( msg ).show();
                    }
                });
        }
    }

    //////////////////////////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////////////////////////
    public void dlgButtonClicked( Action action, int button, Object[] params )
    {
        boolean handled = false;
        if ( AlertDialog.BUTTON_POSITIVE == button ) {
            switch( action ) {
            case ENABLE_SMS_ASK:
                showSMSEnableDialog( Action.ENABLE_SMS_DO, params );
                handled = true;
                break;
            case ENABLE_SMS_DO:
                XWPrefs.setSMSEnabled( m_activity, true );
                break;
            case ENABLE_BT_DO:
                BTService.enable();
                break;
            case ENABLE_RELAY_DO:
                RelayService.setEnabled( m_activity, true );
                handled = true;
                break;
            default:
                Assert.fail();
                break;
            }
        }

        if ( !handled && BuildConfig.DEBUG ) {
            String buttonName = null;
            switch( button ) {
            case AlertDialog.BUTTON_POSITIVE:
                buttonName = "positive";
                break;
            case AlertDialog.BUTTON_NEGATIVE:
                buttonName = "negative";
                break;
            case AlertDialog.BUTTON_NEUTRAL:
                buttonName = "neutral";
                break;
            case DlgDelegate.DISMISS_BUTTON:
                buttonName = "dismiss";
                break;
            default:
                Assert.fail();
                break;
            }
            DbgUtils.logf( "DelegateBase.dlgButtonClicked(action=%s button=%s): UNHANDLED",
                           action.toString(), buttonName );
        }
    }

    public void inviteChoiceMade( Action action, DlgClickNotify.InviteMeans means, Object[] params )
    {
        Assert.fail();
    }
}

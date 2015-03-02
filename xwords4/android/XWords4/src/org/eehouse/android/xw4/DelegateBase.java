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
import android.os.Bundle;
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
import org.eehouse.android.xw4.DlgDelegate.ActionPair;
import org.eehouse.android.xw4.loc.LocUtils;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify;

import junit.framework.Assert;

public class DelegateBase implements DlgClickNotify,
                                     DlgDelegate.HasDlgDelegate,
                                     MultiService.MultiEventListener {

    private DlgDelegate m_dlgDelegate;
    private Delegator m_delegator;
    private Activity m_activity;
    private int m_optionsMenuID;
    private int m_layoutID;
    private View m_rootView;

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
    protected void onStart() {}
    protected void onStop() {}
    protected void onDestroy() {}
    protected void onWindowFocusChanged( boolean hasFocus ) {}
    protected boolean onBackPressed() { return false; }
    protected void onActivityResult( int requestCode, int resultCode, 
                                     Intent data ) 
    {
        DbgUtils.logf( "DelegateBase.onActivityResult(): subclass responsibility!!!" );
    }

    protected void onResume() 
    {
        XWService.setListener( this );
    }

    protected void onPause()
    {
        XWService.setListener( null );
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
        return m_activity.isFinishing();
    }

    protected Intent getIntent()
    {
        return m_activity.getIntent();
    }

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

    protected void setTitle( String title )
    {
        m_activity.setTitle( title );
    }

    protected String getTitle()
    {
        return m_activity.getTitle().toString();
    }

    protected void startActivityForResult( Intent intent, int requestCode )
    {
        m_activity.startActivityForResult( intent, requestCode );
    }

    protected void setResult( int result, Intent intent )
    {
        m_activity.setResult( result, intent );
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
        m_activity.finish();
    }

    protected String getString( int resID, Object... params )
    {
        return LocUtils.getString( m_activity, resID, params );
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
        m_activity.dismissDialog( dlgID.ordinal() );
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

    protected void showNotAgainDlgThen( int msgID, int prefsKey,
                                        Action action, Object... params )
    {
        m_dlgDelegate.showNotAgainDlgThen( msgID, prefsKey, action, null, params );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, Action action )
    {
        m_dlgDelegate.showNotAgainDlgThen( msgID, prefsKey, action );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, Action action,
                                     ActionPair more )
    {
        m_dlgDelegate.showNotAgainDlgThen( msgID, prefsKey, action, more );
    }

    protected void showNotAgainDlgThen( String msg, int prefsKey,
                                        Action action )
    {
        m_dlgDelegate.showNotAgainDlgThen( msg, prefsKey, action, null, null );
    }

    protected void showNotAgainDlg( int msgID, int prefsKey )
    {
        m_dlgDelegate.showNotAgainDlgThen( msgID, prefsKey );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey )
    {
        m_dlgDelegate.showNotAgainDlgThen( msgID, prefsKey );
    }

    // It sucks that these must be duplicated here and XWActivity
    protected void showAboutDialog()
    {
        m_dlgDelegate.showAboutDialog();
    }

    public void showOKOnlyDialog( int msgID )
    {
        m_dlgDelegate.showOKOnlyDialog( msgID );
    }

    public void showOKOnlyDialog( String msg )
    {
        m_dlgDelegate.showOKOnlyDialog( msg );
    }

    protected void showConfirmThen( String msg, Action action, Object... params )
    {
        m_dlgDelegate.showConfirmThen( msg, action, params );
    }

    protected void showConfirmThen( String msg, int posButton, Action action,
                                    Object... params )
    {
        m_dlgDelegate.showConfirmThen( msg, posButton, action, params );
    }

    protected void showConfirmThen( int msg, int posButton, int negButton, Action action )
    {
        m_dlgDelegate.showConfirmThen( msg, posButton, negButton, action );
    }

    protected void showConfirmThen( int msg, int posButton, Action action, 
                                    Object... params )
    {
        m_dlgDelegate.showConfirmThen( msg, posButton, action, params );
    }

    protected void showConfirmThen( int msgID, Action action )
    {
        m_dlgDelegate.showConfirmThen( msgID, action );
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

    protected void showInviteChoicesThen( Action action )
    {
        m_dlgDelegate.showInviteChoicesThen( action );
    }

    protected void showOKOnlyDialogThen( String msg, Action action )
    {
        m_dlgDelegate.showOKOnlyDialog( msg, action );
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

    protected void showSMSEnableDialog( Action action )
    {
        m_dlgDelegate.showSMSEnableDialog( action );
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
        case APP_NOT_FOUND:
            fmtId = R.string.app_not_found_fmt;
            break;
        default:
            DbgUtils.logf( "DelegateBase.eventOccurred(event=%s) (DROPPED)", event.toString() );
        }

        if ( 0 != fmtId ) {
            final String msg = getString( fmtId, (String)args[0] );
            runOnUiThread( new Runnable() {
                    public void run() {
                        showOKOnlyDialog( msg );
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
                showSMSEnableDialog( Action.ENABLE_SMS_DO );
                handled = true;
                break;
            case ENABLE_SMS_DO:
                boolean enabled = (Boolean)params[0];
                if ( enabled ) {
                    XWPrefs.setSMSEnabled( m_activity, true );
                }
                break;
            case ENABLE_BT_DO:
                BTService.enable();
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

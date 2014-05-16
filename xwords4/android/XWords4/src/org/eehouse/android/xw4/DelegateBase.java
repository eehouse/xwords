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
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.loc.LocUtils;

import junit.framework.Assert;

public class DelegateBase implements DlgDelegate.DlgClickNotify,
                                     DlgDelegate.HasDlgDelegate,
                                     MultiService.MultiEventListener {

    private DlgDelegate m_delegate;
    private Activity m_activity;
    private int m_optionsMenuID;
    private View m_rootView;

    public DelegateBase( Activity activity, Bundle bundle )
    {
        this( activity, bundle, R.menu.empty );
    }

    public DelegateBase( Activity activity, Bundle bundle, int optionsMenu )
    {
        Assert.assertTrue( 0 < optionsMenu );
        m_activity = activity;
        m_delegate = new DlgDelegate( activity, this, bundle );
        m_optionsMenuID = optionsMenu;
        LocUtils.xlateTitle( activity );
    }

    // Does nothing unless overridden. These belong in an interface.
    protected void init( Bundle savedInstanceState ) { Assert.fail(); }
    public boolean onPrepareOptionsMenu( Menu menu ) { return false; }
    public boolean onOptionsItemSelected( MenuItem item ) { return false; }
    protected void onStart() {}
    protected void onPause() {}
    protected void onStop() {}
    protected void onDestroy() {}

    protected void onResume()
    {
        LocUtils.setLatestContext( m_activity );
    }

    // public boolean onOptionsItemSelected( MenuItem item ) 
    // {
    // }

    public boolean onCreateOptionsMenu( Menu menu )
    {
        DbgUtils.logf( "DelegateBase.onCreateOptionsMenu()" );
        boolean handled = 0 < m_optionsMenuID;
        if ( handled ) {
            m_activity.getMenuInflater().inflate( m_optionsMenuID, menu );
            LocUtils.xlateMenu( m_activity, menu );
        } else {
            Assert.fail();
        }

        return handled;
    }

    protected Intent getIntent()
    {
        return m_activity.getIntent();
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

    protected void showDialog( DlgID dlgID )
    {
        m_delegate.showDialog( dlgID );
    }

    protected void removeDialog( DlgID dlgID )
    {
        removeDialog( dlgID.ordinal() );
    }

    protected void removeDialog( int id )
    {
        m_activity.removeDialog( id );
    }

    protected Dialog onCreateDialog( int id )
    {
        return m_delegate.createDialog( id );
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
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action, params );
    }

    public void showNotAgainDlgThen( int msgID, int prefsKey, Action action )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action );
    }

    protected void showNotAgainDlgThen( String msg, int prefsKey,
                                        Action action )
    {
        m_delegate.showNotAgainDlgThen( msg, prefsKey, action, null );
    }

    protected void showNotAgainDlg( int msgID, int prefsKey )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey );
    }

    // It sucks that these must be duplicated here and XWActivity
    protected void showAboutDialog()
    {
        m_delegate.showAboutDialog();
    }

    public void showOKOnlyDialog( int msgID )
    {
        m_delegate.showOKOnlyDialog( msgID );
    }

    public void showOKOnlyDialog( String msg )
    {
        m_delegate.showOKOnlyDialog( msg );
    }

    protected void showConfirmThen( String msg, Action action, Object... params )
    {
        m_delegate.showConfirmThen( msg, action, params );
    }

    protected void showConfirmThen( String msg, int posButton, Action action,
                                    Object... params )
    {
        m_delegate.showConfirmThen( msg, posButton, action, params );
    }

    protected void showConfirmThen( int msg, int posButton, Action action, 
                                    Object... params )
    {
        m_delegate.showConfirmThen( msg, posButton, action, params );
    }

    protected void showConfirmThen( int msgID, Action action )
    {
        m_delegate.showConfirmThen( msgID, action );
    }

    protected boolean post( Runnable runnable )
    {
        return m_delegate.post( runnable );
    }

    protected void doSyncMenuitem()
    {
        m_delegate.doSyncMenuitem();
    }

    protected void launchLookup( String[] words, int lang, boolean noStudy )
    {
        m_delegate.launchLookup( words, lang, noStudy );
    }

    protected void launchLookup( String[] words, int lang )
    {
        m_delegate.launchLookup( words, lang, false );
    }

    protected void showInviteChoicesThen( Action action )
    {
        m_delegate.showInviteChoicesThen( action );
    }

    protected void showOKOnlyDialogThen( String msg, Action action )
    {
        m_delegate.showOKOnlyDialog( msg, action );
    }

    protected void startProgress( int id )
    {
        m_delegate.startProgress( id );
    }

    protected void stopProgress()
    {
        m_delegate.stopProgress();
    }

    protected void showDictGoneFinish()
    {
        m_delegate.showDictGoneFinish();
    }

    //////////////////////////////////////////////////
    // MultiService.MultiEventListener interface
    //////////////////////////////////////////////////
    public void eventOccurred( MultiService.MultiEvent event, final Object ... args )
    {
        Assert.fail();
    }

    //////////////////////////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////////////////////////
    public void dlgButtonClicked( Action action, int button, Object[] params )
    {
        Assert.fail();
    }

}

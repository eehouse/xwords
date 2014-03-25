/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;
import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class XWActivity extends Activity
    implements DlgDelegate.DlgClickNotify, DlgDelegate.HasDlgDelegate,
               MultiService.MultiEventListener {

    private DlgDelegate m_delegate;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        DbgUtils.logf( "%s.onCreate(this=%H)", getClass().getName(), this );
        super.onCreate( savedInstanceState );
        m_delegate = new DlgDelegate( this, this, savedInstanceState );
    }

    @Override
    protected void onStart()
    {
        DbgUtils.logf( "%s.onStart(this=%H)", getClass().getName(), this );
        super.onStart();
    }

    @Override
    protected void onResume()
    {
        DbgUtils.logf( "%s.onResume(this=%H)", getClass().getName(), this );
        XWService.setListener( this );
        super.onResume();
    }

    @Override
    protected void onPause()
    {
        DbgUtils.logf( "%s.onPause(this=%H)", getClass().getName(), this );
        XWService.setListener( null );
        super.onPause();
    }

    @Override
    protected void onStop()
    {
        DbgUtils.logf( "%s.onStop(this=%H)", getClass().getName(), this );
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        DbgUtils.logf( "%s.onDestroy(this=%H); isFinishing=%b",
                       getClass().getName(), this, isFinishing() );
        super.onDestroy();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        m_delegate.onSaveInstanceState( outState );
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            DbgUtils.logf( "%s.onCreateDialog() called", getClass().getName() );
            dialog = m_delegate.createDialog( id );
        }
        return dialog;
    }

    // these are duplicated in XWListActivity -- sometimes multiple
    // inheritance would be nice to have...
    protected void showAboutDialog()
    {
        m_delegate.showAboutDialog();
    }

    protected void showNotAgainDlgThen( String msg, int prefsKey,
                                        Action action )
    {
        m_delegate.showNotAgainDlgThen( msg, prefsKey, action, null );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey,
                                        Action action )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey );
    }

    public void showOKOnlyDialog( int msgID )
    {
        m_delegate.showOKOnlyDialog( msgID );
    }

    public void showOKOnlyDialog( String msg )
    {
        m_delegate.showOKOnlyDialog( msg );
    }

    protected void showDictGoneFinish()
    {
        m_delegate.showDictGoneFinish();
    }

    protected void showConfirmThen( int msgID, Action action )
    {
        m_delegate.showConfirmThen( getString(msgID), action );
    }

    protected void showConfirmThen( String msg, Action action )
    {
        m_delegate.showConfirmThen( msg, action );
    }

    protected void showConfirmThen( int msg, int posButton, Action action )
    {
        m_delegate.showConfirmThen( getString(msg), posButton, action );
    }

    public void showInviteChoicesThen( Action action )
    {
        m_delegate.showInviteChoicesThen( action );
    }

    protected void doSyncMenuitem()
    {
        m_delegate.doSyncMenuitem();
    }

    protected void launchLookup( String[] words, int lang )
    {
        m_delegate.launchLookup( words, lang, false );
    }

    protected void startProgress( int id )
    {
        m_delegate.startProgress( id );
    }

    protected void stopProgress()
    {
        m_delegate.stopProgress();
    }

    protected boolean post( Runnable runnable )
    {
        return m_delegate.post( runnable );
    }

    // DlgDelegate.DlgClickNotify interface
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        Assert.fail();
    }

    // BTService.MultiEventListener interface
    public void eventOccurred( MultiService.MultiEvent event, 
                               final Object ... args )
    {
        m_delegate.eventOccurred( event, args );
    }

}

/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import junit.framework.Assert;
import android.view.View;
import android.widget.TextView;
import android.app.AlertDialog;
import android.os.Bundle;

import org.eehouse.android.xw4.jni.CommonPrefs;

public class XWActivity extends Activity
    implements DlgDelegate.DlgClickNotify {

    private DlgDelegate m_delegate;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        Utils.logf( "%s.onCreate(this=%H)", getClass().getName(), this );
        super.onCreate( savedInstanceState );
        m_delegate = new DlgDelegate( this, this, savedInstanceState );
    }

    @Override
    protected void onStart()
    {
        Utils.logf( "%s.onStart(this=%H)", getClass().getName(), this );
        super.onStart();
        DispatchNotify.SetRunning( this );
    }

    @Override
    protected void onResume()
    {
        Utils.logf( "%s.onResume(this=%H)", getClass().getName(), this );
        super.onResume();
    }

    @Override
    protected void onPause()
    {
        Utils.logf( "%s.onPause(this=%H)", getClass().getName(), this );
        super.onPause();
    }

    @Override
    protected void onStop()
    {
        Utils.logf( "%s.onStop(this=%H)", getClass().getName(), this );
        DispatchNotify.ClearRunning( this );
        super.onStop();
    }

    @Override
    protected void onDestroy()
    {
        Utils.logf( "%s.onDestroy(this=%H); isFinishing=%b",
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
            Utils.logf( "%s.onCreateDialog() called", getClass().getName() );
            dialog = m_delegate.onCreateDialog( id );
        }
        return dialog;
    }

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog ); // docs say should call through
        m_delegate.onPrepareDialog( id, dialog );
    }

    // these are duplicated in XWListActivity -- sometimes multiple
    // inheritance would be nice to have...
    protected void showAboutDialog()
    {
        m_delegate.showAboutDialog();
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey,
                                        int action )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey );
    }

    protected void showOKOnlyDialog( int msgID )
    {
        m_delegate.showOKOnlyDialog( msgID );
    }

    protected void showDictGoneFinish()
    {
        m_delegate.showDictGoneFinish();
    }

    protected void showConfirmThen( int msgID, int action )
    {
        m_delegate.showConfirmThen( getString(msgID), action );
    }

    public void showTextOrHtmlThen( int action )
    {
        m_delegate.showTextOrHtmlThen( action );
    }

    protected void doSyncMenuitem()
    {
        m_delegate.doSyncMenuitem();
    }

    protected void launchLookup( String[] words, int lang )
    {
        m_delegate.launchLookup( words, lang, false );
    }

    // DlgDelegate.DlgClickNotify interface
    public void dlgButtonClicked( int id, int which )
    {
        Assert.fail();
    }

}

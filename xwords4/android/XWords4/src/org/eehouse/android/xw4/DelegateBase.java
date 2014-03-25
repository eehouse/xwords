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
import android.app.Dialog;
import android.os.Bundle;

import org.eehouse.android.xw4.DlgDelegate.Action;

import junit.framework.Assert;

public class DelegateBase implements DlgDelegate.DlgClickNotify {

    private DlgDelegate m_delegate;

    public DelegateBase( Activity activity, Bundle bundle )
    {
        m_delegate = new DlgDelegate( activity, this, bundle );
    }

    protected void showDialog( DlgID dlgID )
    {
        m_delegate.showDialog( dlgID );
    }

    protected Dialog createDialog( int id )
    {
        return m_delegate.createDialog( id );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey,
                                        Action action, Object... params )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action, params );
    }

    protected void showNotAgainDlgThen( int msgID, int prefsKey,
                                        Action action )
    {
        m_delegate.showNotAgainDlgThen( msgID, prefsKey, action );
    }

    protected void showNotAgainDlg( int msgID, int prefsKey )
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

    //////////////////////////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////////////////////////
    public void dlgButtonClicked( Action action, int button, Object[] params )
    {
        Assert.fail();
    }

}

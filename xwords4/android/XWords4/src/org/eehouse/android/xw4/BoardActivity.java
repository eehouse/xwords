/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2013 by Eric House (xwords@eehouse.org).  All
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

import android.content.DialogInterface.OnDismissListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;

import android.graphics.Bitmap;
import android.graphics.Rect;

import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;

import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.HashSet;
import java.util.concurrent.Semaphore;
import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.JNIThread.*;


public class BoardActivity extends Activity {

    public static final String INTENT_KEY_CHAT = "chat";

    private static final int CHAT_REQUEST = 1;
    private static final int BT_INVITE_RESULT = 2;
    private static final int SMS_INVITE_RESULT = 3;

    private static final int SCREEN_ON_TIME = 10 * 60 * 1000; // 10 mins

    private static final String DLG_TITLE = "DLG_TITLE";
    private static final String DLG_TITLESTR = "DLG_TITLESTR";
    private static final String DLG_BYTES = "DLG_BYTES";
    private static final String ROOM = "ROOM";
    private static final String PWDNAME = "PWDNAME";
    private static final String TOASTSTR = "TOASTSTR";
    private static final String WORDS = "WORDS";
    private static final String GETDICT = "GETDICT";

    private BoardDelegate m_dlgt;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            dialog = m_dlgt.createDialog( id );
        }
        return dialog;
    } // onCreateDialog

    @Override
    public void onPrepareDialog( int id, Dialog dialog )
    {
        super.onPrepareDialog( id, dialog );
        m_dlgt.prepareDialog( id, dialog );
    }

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        m_dlgt = new BoardDelegate( this, savedInstanceState );
        m_dlgt.init( savedInstanceState );
    } // onCreate

    @Override
    protected void onPause()
    {
        m_dlgt.onPause();
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        m_dlgt.onResume();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        m_dlgt.onSaveInstanceState( outState );
    }

    @Override
    protected void onActivityResult( int requestCode, int resultCode, Intent data )
    {
        m_dlgt.onActivityResult( requestCode, resultCode, data );
    }

    @Override
    public void onWindowFocusChanged( boolean hasFocus )
    {
        super.onWindowFocusChanged( hasFocus );
        m_dlgt.onWindowFocusChanged( hasFocus );
    }

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        return m_dlgt.onKeyDown( keyCode, event )
            || super.onKeyDown( keyCode, event );
    }

    @Override
    public boolean onKeyUp( int keyCode, KeyEvent event )
    {
        return m_dlgt.onKeyUp( keyCode, event ) || super.onKeyUp( keyCode, event );
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu ) 
    {
        return m_dlgt.onCreateOptionsMenu( menu );
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        return m_dlgt.onPrepareOptionsMenu( menu )
            || super.onPrepareOptionsMenu( menu );
    } // onPrepareOptionsMenu

    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        return m_dlgt.onOptionsItemSelected( item )
            || super.onOptionsItemSelected( item );
    }

    private static void noteSkip()
    {
        String msg = "BoardActivity.feedMessage[s](): skipped because "
            + "too many open Boards";
        DbgUtils.logf(msg );
    }
} // class BoardActivity

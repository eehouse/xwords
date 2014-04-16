/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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
// import android.content.Intent;
import android.os.Bundle;
// import java.io.File;
// import java.util.ArrayList;
// import java.util.Arrays;
// import android.view.Gravity;
// import android.view.Menu;
// import android.view.MenuItem;
// import android.widget.TextView;
// import android.widget.AdapterView;
// import android.widget.AdapterView.OnItemSelectedListener;
// import android.view.View;
// import android.view.ViewGroup;
// import android.widget.Button;
import android.app.Dialog;
// import android.app.AlertDialog;
// import android.content.DialogInterface;
// import android.widget.CheckBox;
// import android.widget.CompoundButton;
// import android.widget.ImageButton;
// import android.view.MenuInflater;
import android.view.KeyEvent;
// import android.widget.Spinner;
// import android.widget.ArrayAdapter;
// import android.widget.LinearLayout;
// import android.widget.ListView;
// import android.widget.ListAdapter;
// import android.widget.SpinnerAdapter;
// import android.widget.Toast;
// import android.database.DataSetObserver;
// import junit.framework.Assert;

// import org.eehouse.android.xw4.DlgDelegate.Action;
// import org.eehouse.android.xw4.jni.*;
// import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
// import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
// import org.eehouse.android.xw4.loc.LocUtils;

public class GameConfig extends Activity {

    private GameConfigDelegate m_dlgt;

    @Override
    protected Dialog onCreateDialog( int id )
    {
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            dialog = m_dlgt.onCreateDialog( id );
        }
        return dialog;
    } // onCreateDialog

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    { 
        m_dlgt.onPrepareDialog( id, dialog );
        super.onPrepareDialog( id, dialog );
    }

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState );
        m_dlgt = new GameConfigDelegate( this, savedInstanceState );
    } // onCreate

    @Override
    protected void onStart()
    {
        super.onStart();
        m_dlgt.onStart();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        m_dlgt.onResume();
    }

    @Override
    protected void onPause()
    {
        m_dlgt.onPause();
        super.onPause();
    }

    @Override
    protected void onSaveInstanceState( Bundle outState ) 
    {
        super.onSaveInstanceState( outState );
        m_dlgt.onSaveInstanceState( outState );
    }

    @Override
    public boolean onKeyDown( int keyCode, KeyEvent event )
    {
        boolean consumed = m_dlgt.onKeyDown( keyCode, event );
        return consumed || super.onKeyDown( keyCode, event );
    }
}

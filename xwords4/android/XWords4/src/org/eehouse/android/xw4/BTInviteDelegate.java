/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2015 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

public class BTInviteDelegate extends InviteDelegate {
    private static final String TAG = BTInviteDelegate.class.getSimpleName();
    private static final int[] BUTTONIDS = { R.id.button_add,
                                             R.id.button_clear,
    };
    private Activity m_activity;

    public static void launchForResult( Activity activity, int nMissing,
                                        SentInvitesInfo info,
                                        RequestCode requestCode )
    {
        Assert.assertTrue( 0 < nMissing ); // don't call if nMissing == 0
        Intent intent = new Intent( activity, BTInviteActivity.class );
        intent.putExtra( INTENT_KEY_NMISSING, nMissing );
        if ( null != info ) {
            String lastDev = info.getLastDev( InviteMeans.BLUETOOTH );
            if ( null != lastDev ) {
                intent.putExtra( INTENT_KEY_LASTDEV, lastDev );
            }
        }
        activity.startActivityForResult( intent, requestCode.ordinal() );
    }

    protected BTInviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        String msg = getString( R.string.bt_pick_addall_button );
        msg = getQuantityString( R.plurals.invite_bt_desc_fmt, m_nMissing,
                                 m_nMissing, msg );
        super.init( msg, 0 );
        addButtonBar( R.layout.bt_buttons, BUTTONIDS );
        BTService.clearDevices( m_activity, null ); // will return names
    }

    @Override
    protected void onBarButtonClicked( int id )
    {
        switch( id ) {
        case R.id.button_add:
            scan();
            break;
        case R.id.button_clear:
            Utils.notImpl( m_activity );
            break;
        }
    }

    // MultiService.MultiEventListener interface
    @Override
    public void eventOccurred( MultiService.MultiEvent event, final Object ... args )
    {
        switch( event ) {
        case SCAN_DONE:
            post( new Runnable() {
                    public void run() {
                        synchronized( BTInviteDelegate.this ) {

                            TwoStringPair[] pairs = null;
                            if ( 0 < args.length ) {
                                pairs = TwoStringPair.make( (String[])(args[0]),
                                                            (String[])(args[1]) );
                            }

                            updateListAdapter( pairs );
                            tryEnable();
                        }
                    }
                } );
            break;
        default:
            super.eventOccurred( event, args );
        }
    }

    @Override
    protected void onChildAdded( View child, InviterItem data )
    {
        TwoStrsItem item = (TwoStrsItem)child; // change class name!
        TwoStringPair pair = (TwoStringPair)data;
        ((TwoStrsItem)child).setStrings( pair.str2, pair.str1 );
    }

    @Override
    protected void listSelected( InviterItem[] selected, String[] devs )
    {
        for ( int ii = 0; ii < selected.length; ++ii ) {
            TwoStringPair rec = (TwoStringPair)selected[ii];
            devs[ii] = rec.str1;
            DbgUtils.logd( TAG, "selecting address %s", devs[ii] );
        }
    }

    private void scan()
    {
        int count = BTService.getPairedCount( m_activity );
        if ( 0 < count ) {
            BTService.scan( m_activity );
        } else {
            makeConfirmThenBuilder( R.string.bt_no_devs,
                                    Action.OPEN_BT_PREFS_ACTION )
                .setPosButton( R.string.button_go_settings )
                .show();
        }
    }

    // @Override
    // protected void clearSelected( Integer[] itemIndices )
    // {
    //     // String[][] selected = new String[1][];
    //     // listSelected( selected, null );
    //     // BTService.clearDevices( m_activity, selected[0] );

    //     // super.clearSelected( itemIndices );
    // }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        switch( action ) {
        case OPEN_BT_PREFS_ACTION:
            if ( AlertDialog.BUTTON_POSITIVE == which ) {
                BTService.openBTSettings( m_activity );
            }
            break;
        default:
            super.dlgButtonClicked( action, which, params );
        }
    }
}

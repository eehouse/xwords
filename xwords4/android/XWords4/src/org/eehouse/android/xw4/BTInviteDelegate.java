/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All rights
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
import android.widget.Button;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;

import java.util.Iterator;
import java.util.Set;

public class BTInviteDelegate extends InviteDelegate {
    private static final int[] BUTTONIDS = { R.id.button_add,
                                             R.id.button_settings,
                                             R.id.button_clear,
    };
    private Activity m_activity;
    private TwoStringPair[] m_pairs;

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
        case R.id.button_settings:
            BTService.openBTSettings( m_activity );
            break;
        case R.id.button_clear:
            removeSelected();
            clearChecked();
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

                            m_pairs = null;
                            if ( 0 < args.length ) {
                                m_pairs = TwoStringPair.make( (String[])(args[0]),
                                                              (String[])(args[1]) );
                            }

                            updateListAdapter( m_pairs );
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
        TwoStrsItem item = (TwoStrsItem)child;
        TwoStringPair pair = (TwoStringPair)data;
        // null: we don't display mac address
        ((TwoStrsItem)child).setStrings( pair.str2, null );
    }

    @Override
    protected void tryEnable()
    {
        super.tryEnable();

        Button button = (Button)findViewById( R.id.button_clear );
        if ( null != button ) { // may not be there yet
            button.setEnabled( 0 < getChecked().size() );
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
    private void removeSelected()
    {
        Set<InviterItem> checked = getChecked();
        String[] devs = new String[checked.size()];
        Iterator<InviterItem> iter = checked.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            TwoStringPair pair = (TwoStringPair)iter.next();
            devs[ii] = pair.str1;
        }
        BTService.clearDevices( m_activity, devs );
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void onPosButton( Action action, Object[] params )
    {
        switch( action ) {
        case OPEN_BT_PREFS_ACTION:
            BTService.openBTSettings( m_activity );
            break;
        default:
            super.onPosButton( action, params );
        }
    }
}

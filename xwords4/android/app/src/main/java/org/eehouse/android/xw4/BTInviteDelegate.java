/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.app.ProgressDialog;
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.view.View;
import android.widget.Button;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;

import java.io.Serializable;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;

public class BTInviteDelegate extends InviteDelegate {
    private static final String TAG = BTInviteDelegate.class.getSimpleName();
    private static final String KEY_PERSIST = TAG + "_persist";
    private static final int[] BUTTONIDS = { R.id.button_scan,
                                             R.id.button_settings,
    };
    private Activity m_activity;
    private ProgressDialog m_progress;

    private static class Persisted implements Serializable {
        TwoStringPair[] pairs;
        // HashMap: m_stamps is serialized, so can't be abstract type
        HashMap<String, Long> stamps = new HashMap<>();

        void add( BluetoothDevice dev ) {
            pairs = TwoStringPair.add( pairs, dev.getAddress(), dev.getName() );
            stamps.put( dev.getName(), System.currentTimeMillis() );
        }

        boolean empty() { return pairs == null || pairs.length == 0; }
    }
    private Persisted mPersisted;

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
        String msg = getQuantityString( R.plurals.invite_bt_desc_fmt_2, m_nMissing,
                                        m_nMissing );
        super.init( msg, 0 );
        addButtonBar( R.layout.bt_buttons, BUTTONIDS );

        load();
        if ( mPersisted.empty() ) {
            scan();
        } else {
            updateListAdapter( mPersisted.pairs );
        }
    }

    @Override
    protected void onBarButtonClicked( int id )
    {
        switch( id ) {
        case R.id.button_scan:
            scan();
            break;
        case R.id.button_settings:
            BTService.openBTSettings( m_activity );
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
                        m_progress.cancel();

                        if ( mPersisted.empty() ) {
                            makeNotAgainBuilder( R.string.not_again_emptybtscan,
                                                 R.string.key_notagain_emptybtscan )
                                .show();
                        }
                    }
                } );
            break;
        case HOST_PONGED:
            post( new Runnable() {
                    @Override
                    public void run() {
                        processScanResult( (BluetoothDevice)args[0] );
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
        String devName = ((TwoStringPair)data).str2;

        String msg = null;
        if ( mPersisted.stamps.containsKey( devName ) ) {
            long lastSeenMS = mPersisted.stamps.get( devName );
            CharSequence elapsed = DateUtils.getRelativeTimeSpanString( lastSeenMS );
            msg = getString( R.string.bt_scan_age_fmt, elapsed );
        }

        ((TwoStrsItem)child).setStrings( devName, msg );
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
            mPersisted.pairs = null;
            updateListAdapter( null );

            String msg = getQuantityString( R.plurals.bt_scan_progress_fmt, count, count );
            m_progress = ProgressDialog.show( m_activity, msg, null, true, true );

            BTService.scan( m_activity, 5000 );
        } else {
            makeConfirmThenBuilder( R.string.bt_no_devs,
                                    Action.OPEN_BT_PREFS_ACTION )
                .setPosButton( R.string.button_go_settings )
                .show();
        }
    }

    private void processScanResult( BluetoothDevice dev )
    {
        DbgUtils.assertOnUIThread();

        mPersisted.add( dev );
        store();

        updateListAdapter( mPersisted.pairs );
        tryEnable();
    }

    private void load()
    {
        try {
            String str64 = DBUtils.getStringFor( m_activity, KEY_PERSIST, null );
            mPersisted = (Persisted)Utils.string64ToSerializable( str64 );
        } catch ( Exception ex ) {} // NPE, de-serialization problems, etc.

        if ( null == mPersisted ) {
            mPersisted = new Persisted();
        }
    }

    private void store()
    {
        String str64 = mPersisted == null
            ? "" : Utils.serializableToString64( mPersisted );
        DBUtils.setStringFor( m_activity, KEY_PERSIST, str64 );
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch( action ) {
        case OPEN_BT_PREFS_ACTION:
            BTService.openBTSettings( m_activity );
            break;
        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
    }
}

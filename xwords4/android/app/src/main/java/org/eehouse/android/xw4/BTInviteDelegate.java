/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2009 - 2019 by Eric House (xwords@eehouse.org).  All rights
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
import android.bluetooth.BluetoothDevice;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.text.format.DateUtils;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;
import org.eehouse.android.xw4.DlgDelegate.Action;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

public class BTInviteDelegate extends InviteDelegate {
    private static final String TAG = BTInviteDelegate.class.getSimpleName();
    private static final String KEY_PERSIST = TAG + "_persist";
    private static final int[] BUTTONIDS = { R.id.button_scan,
                                             R.id.button_settings,
                                             R.id.button_clear,
    };
    private static final boolean ENABLE_FAKER = false;
    private static final int SCAN_SECONDS = 5;

    private Activity m_activity;
    private ProgressBar mProgressBar;
    private Handler m_handler = new Handler();
    private int mNDevsThisScan;

    private static class Persisted implements Serializable {
        List<TwoStringPair> pairs;
        // HashMap: m_stamps is serialized, so can't be abstract type
        HashMap<String, Long> stamps = new HashMap<>();

        void add( String devAddress, String devName ) {
            // If it's already there, update it. Otherwise create new
            boolean alreadyHave = false;
            if ( null == pairs ) {
                pairs = new ArrayList<>();
            } else {
                for ( TwoStringPair pair : pairs ) {
                    alreadyHave = pair.str2.equals(devName);
                    if ( alreadyHave ) {
                        break;
                    }
                }
            }

            if ( !alreadyHave ) {
                pairs.add( new TwoStringPair( devAddress, devName ) );
            }
            stamps.put( devName, System.currentTimeMillis() );
            sort();
        }

        void remove(final Set<? extends InviterItem> checked)
        {
            for ( InviterItem item : checked ) {
                TwoStringPair pair = (TwoStringPair)item;
                stamps.remove( pair.str2 );
                pairs.remove( pair );
            }
        }

        boolean empty() { return pairs == null || pairs.size() == 0; }

        private void sort()
        {
            Collections.sort( pairs, new Comparator<TwoStringPair>() {
                    @Override
                    public int compare( TwoStringPair rec1, TwoStringPair rec2 ) {
                        long val1 = stamps.get( rec1.str2 );
                        long val2 = stamps.get( rec2.str2 );
                        return (int)(val2 - val1);
                    }
                });
        }
    }
    private Persisted mPersisted;

    public static void launchForResult( Activity activity, int nMissing,
                                        SentInvitesInfo info,
                                        RequestCode requestCode )
    {
        Assert.assertTrue( 0 < nMissing ); // don't call if nMissing == 0
        Intent intent = InviteDelegate
            .makeIntent( activity, BTInviteActivity.class,
                         nMissing, info );
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
                                        m_nMissing )
            + getString( R.string.invite_bt_desc_postscript );
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
        case R.id.button_clear:
            int count = getChecked().size();
            String msg = getQuantityString( R.plurals.confirm_clear_bt_fmt,
                                            count, count )
                + getString( R.string.confirm_clear_bt_postscript );
            makeConfirmThenBuilder( msg, Action.CLEAR_ACTION ).show();
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
                        hideProgress();

                        if ( mPersisted.empty() || 0 == mNDevsThisScan ) {
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
            CharSequence elapsed = DateUtils
                .getRelativeTimeSpanString( mPersisted.stamps.get( devName ),
                                            System.currentTimeMillis(),
                                            DateUtils.SECOND_IN_MILLIS );
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
        if ( ENABLE_FAKER && Utils.nextRandomInt() % 5 == 0 ) {
            mPersisted.add( "00:00:00:00:00:00", "Do Not Invite Me" );
        }

        int count = BTService.getPairedCount( m_activity );
        if ( 0 < count ) {
            mNDevsThisScan = 0;
            showProgress( count, 2 * SCAN_SECONDS );
            BTService.scan( m_activity, 1000 * SCAN_SECONDS );
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

        ++mNDevsThisScan;
        mPersisted.add( dev.getAddress(), dev.getName() );
        store();

        updateListAdapter( mPersisted.pairs );
        tryEnable();
    }

    private void showProgress( int nDevs, int nSeconds )
    {
        mProgressBar = (ProgressBar)findViewById( R.id.progress );
        mProgressBar.setProgress( 0 );
        mProgressBar.setMax( nSeconds );

        String msg = getQuantityString( R.plurals.bt_scan_progress_fmt,
                                        nDevs, nDevs );
        ((TextView)findViewById( R.id.progress_msg )).setText( msg );

        findViewById( R.id.progress_line ).setVisibility( View.VISIBLE );
        incrementProgressIn( 1 );
    }

    private void hideProgress()
    {
        findViewById( R.id.progress_line ).setVisibility( View.GONE );
        mProgressBar = null;
    }

    private void incrementProgressIn( int inSeconds )
    {
        m_handler.postDelayed( new Runnable() {
                @Override
                public void run() {
                    if ( null != mProgressBar ) {
                        int curProgress = mProgressBar.getProgress();
                        if ( curProgress >= mProgressBar.getMax() ) {
                            hideProgress(); // create illusion it's done
                        } else {
                            mProgressBar.setProgress( curProgress + 1, true );
                            incrementProgressIn( 1 );
                        }
                    }
                }
            }, 1000 * inSeconds );
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
        case CLEAR_ACTION:
            mPersisted.remove( getChecked() );
            store();

            clearChecked();
            updateListAdapter( mPersisted.pairs );
            tryEnable();
            break;
        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
    }
}

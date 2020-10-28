/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.text.TextUtils;
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
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

public class BTInviteDelegate extends InviteDelegate
    implements BTUtils.ScanListener {

    private static final String TAG = BTInviteDelegate.class.getSimpleName();
    private static final String KEY_PERSIST = TAG + "_persist";
    private static final int[] BUTTONIDS = { R.id.button_scan,
                                             R.id.button_settings,
                                             R.id.button_clear,
    };
    private static final boolean ENABLE_FAKER = false;
    private static final int SCAN_SECONDS = 5;

    private static Persisted[] sPersistedRef = {null};

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
                    alreadyHave = TextUtils.equals(pair.str2, devName);
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

        void remove( final Set<String> checked )
        {
            for ( String dev : checked ) {
                stamps.remove( dev );

                for ( Iterator<TwoStringPair> iter = pairs.iterator();
                      iter.hasNext(); ) {
                    TwoStringPair pair = iter.next();
                    if ( TextUtils.equals( pair.getDev(), dev ) ) {
                        iter.remove();
                        break;
                    }
                }
            }
        }

        private void removeNulls()
        {
            for ( Iterator<TwoStringPair> iter = pairs.iterator();
                  iter.hasNext(); ) {
                TwoStringPair pair = iter.next();
                if ( TextUtils.isEmpty( pair.str2 ) ) {
                    Log.d( TAG, "removeNulls(): removing!!" );
                    iter.remove();
                }
            }
        }

        boolean empty() { return pairs == null || pairs.size() == 0; }

        private void sort()
        {
            Collections.sort( pairs, new Comparator<TwoStringPair>() {
                    @Override
                    public int compare( TwoStringPair rec1, TwoStringPair rec2 ) {
                        int result = 0;
                        try {
                            long val1 = stamps.get( rec1.str2 );
                            long val2 = stamps.get( rec2.str2 );
                            if ( val2 > val1 ) {
                                result = 1;
                            } else if ( val1 > val2 ) {
                                result = -1;
                            }
                        } catch ( Exception ex ) {
                            Log.e( TAG, "ex %s on %s vs %s", ex, rec1, rec2 );
                        }
                        return result;
                    }
                });
        }
    }

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
        super.init( savedInstanceState );

        String msg = getQuantityString( R.plurals.invite_bt_desc_fmt_2, m_nMissing,
                                        m_nMissing )
            + getString( R.string.invite_bt_desc_postscript );
        super.init( msg, R.string.empty_bt_inviter );

        addButtonBar( R.layout.bt_buttons, BUTTONIDS );

        load( m_activity );
        if ( sPersistedRef[0].empty() ) {
            scan();
        } else {
            updateListIn( 0 );
        }
    }

    @Override
    protected void onResume()
    {
        BTUtils.addScanListener( this );
        super.onResume();
    }

    @Override
    protected void onPause()
    {
        BTUtils.removeScanListener( this );
        super.onResume();
    }

    @Override
    protected void onBarButtonClicked( int id )
    {
        switch( id ) {
        case R.id.button_scan:
            scan();
            break;
        case R.id.button_settings:
            BTUtils.openBTSettings( m_activity );
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

    @Override
    protected void onChildAdded( View child, InviterItem data )
    {
        String devName = ((TwoStringPair)data).str2;

        String msg = null;
        if ( sPersistedRef[0].stamps.containsKey( devName ) ) {
            CharSequence elapsed = DateUtils
                .getRelativeTimeSpanString( sPersistedRef[0].stamps.get( devName ),
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

    // interface ScanListener
    @Override
    public void onDeviceScanned( final BluetoothDevice dev )
    {
        post( new Runnable() {
                @Override
                public void run() {
                    processScanResult( dev );
                }
            } );
    }

    @Override
    public void onScanDone()
    {
        post( new Runnable() {
                @Override
                public void run() {
                    hideProgress();

                    if ( sPersistedRef[0].empty() || 0 == mNDevsThisScan ) {
                        makeNotAgainBuilder( R.string.not_again_emptybtscan,
                                             R.string.key_notagain_emptybtscan )
                            .show();
                    }
                }
            } );
    }

    private void scan()
    {
        if ( ENABLE_FAKER && Utils.nextRandomInt() % 5 == 0 ) {
            sPersistedRef[0].add( "00:00:00:00:00:00", "Do Not Invite Me" );
        }

        int count = BTUtils.scan( m_activity, 1000 * SCAN_SECONDS );
        if ( 0 < count ) {
            mNDevsThisScan = 0;
            showProgress( count, 2 * SCAN_SECONDS );
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
        sPersistedRef[0].add( dev.getAddress(), dev.getName() );
        store( m_activity );

        updateList();
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
                            mProgressBar.setProgress( curProgress + 1 );
                            incrementProgressIn( 1 );
                        }
                    }
                }
            }, 1000 * inSeconds );
    }

    private static void removeNotPaired( Persisted prs )
    {
        Log.d( TAG, "removeNotPaired()" );
        BluetoothAdapter adapter = BTUtils.getAdapterIf();
        if ( null != adapter ) {
            Set<BluetoothDevice> pairedDevs = BTUtils.getCandidates();
            Set<String> paired = new HashSet<>();
            for ( BluetoothDevice dev : pairedDevs ) {
                Log.d( TAG, "removeNotPaired(): paired dev: %s", dev.getName() );
                paired.add( dev.getName() );
            }

            Set<String> toRemove = new HashSet<>();
            for ( TwoStringPair pair : prs.pairs ) {
                String name = pair.str2;
                if ( ! paired.contains( name ) ) {
                    Log.d( TAG, "%s no longer paired; removing", name );
                    toRemove.add( name );
                } else {
                    Log.d( TAG, "%s STILL paired", name );
                }
            }

            if ( ! toRemove.isEmpty() ) {
                prs.remove( toRemove );
            }
        } else {
            Log.e( TAG, "removeNotPaired(): adapter null" );
        }
    }

    private void updateListIn( final long inSecs )
    {
        m_handler.postDelayed( new Runnable() {
                @Override
                public void run() {
                    updateList();
                    updateListIn( 10 );
                }
            }, inSecs * 1000 );
    }

    private void updateList()
    {
        updateList( sPersistedRef[0].pairs );
    }

    private synchronized static void load( Context context )
    {
        if ( null == sPersistedRef[0] ) {
            Persisted prs;
            try {
                prs = (Persisted)DBUtils.getSerializableFor( context, KEY_PERSIST );
                prs.removeNulls(); // clean up earlier mistakes
                removeNotPaired( prs );
            } catch ( Exception ex ) {
                prs = null;
            } // NPE, de-serialization problems, etc.

            if ( null == prs ) {
                prs = new Persisted();
            }
            sPersistedRef[0] = prs;
        }
    }

    private synchronized static void store( Context context )
    {
        DBUtils.setSerializableFor( context, KEY_PERSIST, sPersistedRef[0] );
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch( action ) {
        case OPEN_BT_PREFS_ACTION:
            BTUtils.openBTSettings( m_activity );
            break;
        case CLEAR_ACTION:
            sPersistedRef[0].remove( getChecked() );
            store( m_activity );

            clearChecked();
            updateList();
            tryEnable();
            break;
        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
    }

    public static void onHeardFromDev( BluetoothDevice dev )
    {
        Context context = XWApp.getContext();
        load( context );
        sPersistedRef[0].add( dev.getAddress(), dev.getName() );
        store( context );
    }
}

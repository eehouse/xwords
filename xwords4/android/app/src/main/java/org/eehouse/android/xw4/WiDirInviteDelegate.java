/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2016 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo;

public class WiDirInviteDelegate extends InviteDelegate
    implements WiDirService.DevSetListener {
    private static final String SAVE_NAME = "SAVE_NAME";
    private Map<String, String> m_macsToName;
    private Activity m_activity;

    public static void launchForResult( Activity activity, int nMissing,
                                        SentInvitesInfo info,
                                        RequestCode requestCode )
    {
        Intent intent =
            InviteDelegate.makeIntent( activity, WiDirInviteActivity.class,
                                       nMissing, info );
        activity.startActivityForResult( intent, requestCode.ordinal() );
    }

    public WiDirInviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState );
        m_activity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        super.init( savedInstanceState );

        String msg = getString( R.string.button_invite );
        msg = getQuantityString( R.plurals.invite_p2p_desc_fmt, m_nMissing,
                                 m_nMissing, msg );
        msg += "\n\n" + getString( R.string.invite_p2p_desc_extra );
        super.init( msg, R.string.empty_p2p_inviter );
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        WiDirService.registerDevSetListener( this );
    }

    @Override
    protected void onPause()
    {
        super.onPause();
        WiDirService.unregisterDevSetListener( this );
    }

    protected void onBarButtonClicked( int id )
    {
        // not implemented yet as there's no bar button
        Assert.assertFalse( BuildConfig.DEBUG );
    }

    @Override
    protected void onChildAdded( View child, InviterItem data )
    {
        TwoStringPair pair = (TwoStringPair)data;
        ((TwoStrsItem)child).setStrings( pair.str2, pair.getDev() );
    }

    // DevSetListener interface
    public void setChanged( Map<String, String> macToName )
    {
        m_macsToName = macToName;
        runOnUiThread( new Runnable() {
                @Override
                public void run() {
                    rebuildList();
                }
            } );
    }

    private void rebuildList()
    {
        int count = m_macsToName.size();
        List<TwoStringPair> pairs = new ArrayList<>();
        // String[] names = new String[count];
        // String[] addrs = new String[count];
        Iterator<String> iter = m_macsToName.keySet().iterator();
        for ( int ii = 0; ii < count; ++ii ) {
            String mac = iter.next();
            pairs.add( new TwoStringPair(mac, m_macsToName.get(mac) ) );
            // addrs[ii] = mac;
            // names[ii] = m_macsToName.get(mac);
        }

        updateList( pairs );
    }
}

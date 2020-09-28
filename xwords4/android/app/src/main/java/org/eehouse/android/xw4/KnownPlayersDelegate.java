/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.ClipboardManager;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import android.widget.ListView;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class KnownPlayersDelegate extends DelegateBase {
    private static final String TAG = KnownPlayersDelegate.class.getSimpleName();

    private Activity mActivity;
    private ViewGroup mList;

    protected KnownPlayersDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, R.layout.knownplayrs );
        mActivity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle sis )
    {
        mList = (ViewGroup)findViewById( R.id.players_list );
        populateList();
    }

    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch ( action ) {
        case KNOWN_PLAYER_DELETE:
            String name = (String)params[0];
            XwJNI.kplr_deletePlayer( name );
            mList.removeAllViews();
            populateList();
            break;
        default:
            handled = super.onPosButton( action, params );
            break;
        }
        return handled;
    }

    private void populateList()
    {
        String[] players = XwJNI.kplr_getPlayers();
        if ( null == players ) {
            finish();
        } else {
            for ( String player : players ) {
                View child = makePlayerElem( player );
                if ( null != child ) {
                    mList.addView( child );
                }
            }
        }
    }

    private View makePlayerElem( final String player )
    {
        View view = null;
        CommsAddrRec addr = XwJNI.kplr_getAddr( player );
        if ( null != addr ) {
            final ViewGroup item = (ViewGroup)LocUtils.inflate( mActivity, R.layout.knownplayrs_item );
            TextView tv = (TextView)item.findViewById( R.id.player_name );
            tv.setText( player );
            view = item;

            // Iterate over address types
            CommsConnTypeSet conTypes = addr.conTypes;
            ViewGroup list = (ViewGroup)item.findViewById( R.id.items );
            if ( conTypes.contains( CommsAddrRec.CommsConnType.COMMS_CONN_BT ) ) {
                addListing( list, R.string.knowns_bt_fmt, addr.bt_hostName );
            }
            if ( conTypes.contains( CommsAddrRec.CommsConnType.COMMS_CONN_SMS ) ) {
                addListing( list, R.string.knowns_smsphone_fmt, addr.sms_phone );
            }
            if ( BuildConfig.NON_RELEASE ) {
                if ( conTypes.contains( CommsAddrRec.CommsConnType.COMMS_CONN_MQTT ) ) {
                    addListing( list, R.string.knowns_mqtt_fmt, addr.mqtt_devID );
                }
                // if ( conTypes.contains( CommsAddrRec.CommsConnType.COMMS_CONN_RELAY ) ) {
                //     addListing( item, R.string.knowns_relay_fmt, addr.relay_devID );
                // }
            }

            item.findViewById( R.id.player_edit_name )
                .setOnClickListener( new View.OnClickListener() {
                        @Override
                        public void onClick( View view ) {
                            Utils.notImpl( mActivity );
                        }
                    } );
            item.findViewById( R.id.player_delete )
                .setOnClickListener( new View.OnClickListener() {
                        @Override
                        public void onClick( View view ) {
                            confirmAndDelete( player );
                        }
                    } );

            ExpandImageButton eib = (ExpandImageButton)item.findViewById( R.id.expander );
            eib.setOnExpandChangedListener( new ExpandChangeListener() {
                    @Override
                    public void expandedChanged( boolean nowExpanded )
                    {
                        item.findViewById(R.id.hidden_part)
                            .setVisibility(nowExpanded?View.VISIBLE:View.GONE);
                    }
                } );

        }
        return view;
    }

    private void addListing( ViewGroup parent, int fmtID, String elem )
    {
        String content = LocUtils.getString( mActivity, fmtID, elem );
        TextView item = (TextView)LocUtils.inflate( mActivity, R.layout.knownplayrs_item_line );
        item.setText( content );
        parent.addView( item );
    }

    private void editName( String name )
    {
        Log.d( TAG, "editName(%s) not implemented yet", name );
    }

    private void confirmAndDelete( String name )
    {
        String msg = LocUtils.getString( mActivity,
                                         R.string.player_delete_confirm_fmt,
                                         name );
        makeConfirmThenBuilder( msg, Action.KNOWN_PLAYER_DELETE )
            .setParams( name )
            .show();
    }

    public static void launchOrAlert( Delegator delegator, 
                                      DlgDelegate.HasDlgDelegate dlg )
    {
        Activity activity = delegator.getActivity();

        if ( XwJNI.hasKnownPlayers() ) {
            if ( delegator.inDPMode() ) {
                delegator.addFragment( KnownPlayersFrag.newInstance( delegator ),
                                       null );
            } else {
                Intent intent = new Intent( activity, KnownPlayersActivity.class );
                activity.startActivity( intent );
            }
        } else {
            dlg.makeOkOnlyBuilder( R.string.no_knowns_expl )
                .show();
        }
    }
}

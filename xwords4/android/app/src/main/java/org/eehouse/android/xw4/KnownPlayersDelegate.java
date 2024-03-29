/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2020 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.TextView;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class KnownPlayersDelegate extends DelegateBase {
    private static final String TAG = KnownPlayersDelegate.class.getSimpleName();
    private static final String KEY_EXPSET = TAG + "/expset";
    private static final String KEY_BY_DATE = TAG + "/bydate";

    private Activity mActivity;
    private ViewGroup mList;
    private List<ViewGroup> mChildren;
    private HashSet<String> mExpSet;
    private boolean mByDate;

    protected KnownPlayersDelegate( Delegator delegator, Bundle sis )
    {
        super( delegator, sis, R.layout.knownplayrs );
        mActivity = delegator.getActivity();
    }

    @Override
    protected void init( Bundle sis )
    {
        mList = (ViewGroup)findViewById( R.id.players_list );

        loadExpanded();

        mByDate = DBUtils.getBoolFor( mActivity, KEY_BY_DATE, false );

        CheckBox sortCheck = (CheckBox)findViewById( R.id.sort_box );
        sortCheck.setOnCheckedChangeListener( new OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged( CompoundButton buttonView,
                                              boolean checked ) {
                    DBUtils.setBoolFor( mActivity, KEY_BY_DATE, checked );
                    mByDate = checked;
                    populateList();
                }
            } );
        sortCheck.setChecked( mByDate );
        populateList();
    }

    @Override
    public boolean onPosButton( Action action, Object... params )
    {
        boolean handled = true;
        switch ( action ) {
        case KNOWN_PLAYER_DELETE:
            String name = (String)params[0];
            XwJNI.kplr_deletePlayer( name );
            populateList();
            break;
        default:
            handled = super.onPosButton( action, params );
            break;
        }
        return handled;
    }

    @Override
    protected Dialog makeDialog( DBAlert alert, Object... params )
    {
        Dialog dialog = null;

        DlgID dlgID = alert.getDlgID();
        switch ( dlgID ) {
        case RENAME_PLAYER:
            final String oldName = (String)params[0];
            final Renamer namer = ((Renamer)inflate( R.layout.renamer ))
                .setName( oldName )
                ;

            OnClickListener lstnr = new OnClickListener() {
                    @Override
                    public void onClick( DialogInterface dlg, int item )
                    {
                        tryRename( oldName, namer.getName() );
                    }
                };
            dialog = buildNamerDlg( namer, lstnr, null, dlgID );
            break;
        }

        if ( null == dialog ) {
            dialog = super.makeDialog( alert, params );
        }
        return dialog;
    }

    private void tryRename( String oldName, String newName )
    {
        if ( ! newName.equals(oldName) && 0 < newName.length() ) {
            if ( XwJNI.kplr_renamePlayer( oldName, newName ) ) {
                populateList();
            } else {
                makeOkOnlyBuilder( R.string.knowns_dup_name_fmt,
                                   oldName, newName )
                    .show();
            }
        }
    }

    private void populateList()
    {
        String[] players = XwJNI.kplr_getPlayers( mByDate );
        if ( null == players ) {
            finish();
        } else {
            mChildren = new ArrayList<>();
            for ( String player : players ) {
                Log.d( TAG, "populateList(): player: %s", player );
                ViewGroup child = makePlayerElem( player );
                if ( null != child ) {
                    mChildren.add( child );
                }
            }
            addInOrder();
            pruneExpanded();
        }
    }

    private void addInOrder()
    {
        mList.removeAllViews();
        for ( ViewGroup child : mChildren ) {
            mList.addView( child );
        }
    }

    private void pruneExpanded()
    {
        boolean doSave = false;

        Set<String> children = new HashSet<>();
        for ( ViewGroup child : mChildren ) {
            children.add( getName(child) );
        }

        for ( Iterator<String> iter = mExpSet.iterator(); iter.hasNext(); ) {
            String child = iter.next();
            if ( !children.contains(child) ) {
                iter.remove();
                doSave = true;
            }
        }
        if ( doSave ) {
            saveExpanded();
        }
    }

    private void setName( ViewGroup item, String name )
    {
        TextView tv = (TextView)item.findViewById( R.id.player_name );
        tv.setText( name );
    }

    private String getName( ViewGroup item )
    {
        TextView tv = (TextView)item.findViewById( R.id.player_name );
        return tv.getText().toString();
    }

    private ViewGroup makePlayerElem( final String player )
    {
        ViewGroup view = null;
        int lastMod[] = {0};
        CommsAddrRec addr = XwJNI.kplr_getAddr( player, lastMod );

        if ( null != addr ) {
            final ViewGroup item = (ViewGroup)LocUtils
                .inflate( mActivity, R.layout.knownplayrs_item );
            setName( item, player );
            view = item;

            // Iterate over address types
            CommsConnTypeSet conTypes = addr.conTypes;
            ViewGroup list = (ViewGroup)item.findViewById( R.id.items );

            long timeStmp = 1000L * lastMod[0];
            if ( BuildConfig.NON_RELEASE && 0 < timeStmp ) {
                String str = DateFormat.getDateTimeInstance()
                    .format(new Date(timeStmp));
                addListing( list, R.string.knowns_ts_fmt, str );
            }

            if ( conTypes.contains( CommsConnType.COMMS_CONN_BT ) ) {
                addListing( list, R.string.knowns_bt_fmt, addr.bt_hostName );
                if ( BuildConfig.NON_RELEASE ) {
                    addListing( list, R.string.knowns_bta_fmt, addr.bt_btAddr );
                }
            }
            if ( conTypes.contains( CommsConnType.COMMS_CONN_SMS ) ) {
                addListing( list, R.string.knowns_smsphone_fmt, addr.sms_phone );
            }
            if ( BuildConfig.NON_RELEASE ) {
                if ( conTypes.contains( CommsConnType.COMMS_CONN_MQTT ) ) {
                    addListing( list, R.string.knowns_mqtt_fmt, addr.mqtt_devID );
                }
            }

            item.findViewById( R.id.player_edit_name )
                .setOnClickListener( new View.OnClickListener() {
                        @Override
                        public void onClick( View view ) {
                            showDialogFragment( DlgID.RENAME_PLAYER, getName(item) );
                        }
                    } );
            item.findViewById( R.id.player_delete )
                .setOnClickListener( new View.OnClickListener() {
                        @Override
                        public void onClick( View view ) {
                            confirmAndDelete( getName(item) );
                        }
                    } );

            final ExpandImageButton eib = (ExpandImageButton)item.findViewById( R.id.expander );
            eib.setOnExpandChangedListener( new ExpandChangeListener() {
                    @Override
                    public void expandedChanged( boolean nowExpanded )
                    {
                        item.findViewById(R.id.hidden_part)
                            .setVisibility(nowExpanded?View.VISIBLE:View.GONE);
                        if ( nowExpanded ) {
                            mExpSet.add( player );
                        } else {
                            mExpSet.remove( player );
                        }
                        saveExpanded();
                    }
                } );
            eib.setExpanded( mExpSet.contains(player) );

            item.findViewById( R.id.player_line )
                .setOnClickListener( new View.OnClickListener() {
                        @Override
                        public void onClick( View view ) {
                            eib.toggle();
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
        makeConfirmThenBuilder( Action.KNOWN_PLAYER_DELETE,
                                R.string.knowns_delete_confirm_fmt, name )
            .setParams( name )
            .show();
    }

    private void loadExpanded()
    {
        HashSet<String> expSet;
        try {
            expSet = (HashSet<String>)DBUtils.getSerializableFor( mActivity, KEY_EXPSET );
        } catch ( Exception ex ) {
            Log.ex( TAG, ex );
            expSet = null;
        }
        if ( null == expSet ) {
            expSet = new HashSet<>();
        }

        mExpSet = expSet;
    }

    private void saveExpanded()
    {
        DBUtils.setSerializableFor( mActivity, KEY_EXPSET, mExpSet );
    }

    public static void launchOrAlert( Delegator delegator, 
                                      DlgDelegate.HasDlgDelegate dlg )
    {
        Activity activity = delegator.getActivity();

        if ( XwJNI.hasKnownPlayers() ) {
            delegator.addFragment( KnownPlayersFrag.newInstance( delegator ),
                                   null );
        } else {
            Assert.failDbg();
        }
    }
}

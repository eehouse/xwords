/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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
import android.app.Dialog;
import android.app.ListActivity;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract;
import android.text.method.DialerKeyListener;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.Spinner;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;

public class RelayInviteDelegate extends InviteDelegate {

    // private static final int GET_CONTACT = 1;
    private static final String SAVE_NAME = "SAVE_NAME";
    private static final String SAVE_NUMBER = "SAVE_NUMBER";

    private ArrayList<DevIDRec> m_devIDRecs;
    private SMSPhonesAdapter m_adapter;
    private ImageButton m_addButton;
    private boolean m_immobileConfirmed;
    private Activity m_activity;

    public static void launchForResult( Activity activity, int nMissing, 
                                        int requestCode )
    {
        Intent intent = new Intent( activity, RelayInviteActivity.class );
        intent.putExtra( INTENT_KEY_NMISSING, nMissing );
        activity.startActivityForResult( intent, requestCode );
    }

    public RelayInviteDelegate( ListDelegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.relayinviter );
        m_activity = delegator.getActivity();
    }

    protected void init( Bundle savedInstanceState )
    {
        String msg = getString( R.string.button_invite );
        msg = getQuantityString( R.plurals.invite_relay_desc_fmt, m_nMissing, 
                                 m_nMissing, msg );
        super.init( R.id.button_invite, R.id.button_add, R.id.button_clear, 
                    R.id.invite_desc, msg );

        // getBundledData( savedInstanceState );

        m_addButton = (ImageButton)findViewById( R.id.manual_add_button );
        m_addButton.setOnClickListener( new View.OnClickListener() {
                public void onClick( View view )
                {
                    showDialog( DlgID.GET_NUMBER );
                }
            } );

        if ( XWPrefs.getRelayInviteToSelfEnabled( m_activity ) ) {
            ImageButton addMe = (ImageButton)findViewById( R.id.add_self_button );
            addMe.setVisibility( View.VISIBLE );
            addMe.setOnClickListener( new View.OnClickListener() {
                    public void onClick( View view ) {
                        int devIDInt = XWPrefs.getRelayDevIDInt( m_activity );
                        String devID = String.format( "%d", devIDInt );
                        DevIDRec rec = new DevIDRec( "self", devID );
                        addChecked( rec );
                        saveAndRebuild();
                    }
                } );
        }

        getSavedState();
        rebuildList( true );
    }

    // protected void onSaveInstanceState( Bundle outState ) 
    // {
    //     outState.putString( SAVE_NAME, m_pendingName );
    //     outState.putString( SAVE_NUMBER, m_pendingNumber );
    // }

    // private void getBundledData( Bundle bundle )
    // {
    //     if ( null != bundle ) {
    //         m_pendingName = bundle.getString( SAVE_NAME );
    //         m_pendingNumber = bundle.getString( SAVE_NUMBER );
    //     }
    // }
    
    // protected void onActivityResult( int requestCode, int resultCode, 
    //                                  Intent data )
    // {
    //     // super.onActivityResult( requestCode, resultCode, data );
    //     if ( Activity.RESULT_CANCELED != resultCode && data != null ) {
    //         switch (requestCode) {
    //         case GET_CONTACT:
    //             addPhoneNumbers( data );
    //             break;
    //         }
    //     }
    // }

    protected Dialog onCreateDialog( int id )
    {        
        Dialog dialog = super.onCreateDialog( id );
        if ( null == dialog ) {
            DialogInterface.OnClickListener lstnr;
            DlgID dlgID = DlgID.values()[id];
            switch( dlgID ) {
            case GET_NUMBER:
                final GameNamer namerView =
                    (GameNamer)inflate( R.layout.rename_game );
                namerView.setLabel( R.string.get_relay_number );
                namerView.setKeyListener(DialerKeyListener.getInstance());
                lstnr = new DialogInterface.OnClickListener() {
                        public void onClick( DialogInterface dlg, int item ) {
                            String devID = namerView.getName();
                            if ( 0 < devID.length() ) {
                                DevIDRec rec = new DevIDRec( devID );
                                addChecked( new DevIDRec( devID ) );
                                saveAndRebuild();
                            }
                        }
                    };
                dialog = makeAlertBuilder()
                    .setNegativeButton( android.R.string.cancel, null )
                    .setPositiveButton( android.R.string.ok, lstnr )
                    .setView( namerView )
                    .create();
                break;
            }
            setRemoveOnDismiss( dialog, dlgID );
        }
        return dialog;
    }

    protected void scan()
    {
        Utils.notImpl( m_activity );
        // Intent intent = new Intent( Intent.ACTION_PICK, 
        //                             ContactsContract.Contacts.CONTENT_URI );
        // intent.setType( Phone.CONTENT_TYPE );
        // startActivityForResult( intent, GET_CONTACT );
    }

    protected void clearSelected()
    {
        showConfirmThen( R.string.confirm_clear_relay, Action.CLEAR_ACTION );
    }

    protected void listSelected( String[][] devsP, int[][] countsP )
    {
        int count = m_adapter.getCount();
        String[] result = new String[countChecks()];
        int[] counts = new int[result.length];

        int index = 0;
        Iterator<DevIDRec> iter = m_devIDRecs.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            DevIDRec rec = iter.next();
            if ( rec.m_isChecked ) {
                counts[index] = rec.m_nPlayers;
                result[index] = ((SMSListItem)m_adapter.getItem(ii)).getNumber();
                index++;
            }
        }
        devsP[0] = result;
        if ( null != countsP ) {
            countsP[0] = counts;
        }
    }

    @Override
    protected void tryEnable() 
    {
        if ( null != m_devIDRecs ) {
            int nPlayers = 0;
            int nDevs = 0;
            Iterator<DevIDRec> iter = m_devIDRecs.iterator();
            while ( iter.hasNext() ) {
                DevIDRec rec = iter.next();
                if ( rec.m_isChecked ) {
                    ++nDevs;
                    nPlayers += rec.m_nPlayers;
                }
            }
            m_okButton.setEnabled( 0 < nPlayers && nPlayers <= m_nMissing );
            m_clearButton.setEnabled( 0 < nDevs );
        }
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        switch( which ) {
        case AlertDialog.BUTTON_POSITIVE:
            switch( action ) {
            case CLEAR_ACTION:
                clearSelectedImpl();
                break;
            case USE_IMMOBILE_ACTION:
                m_immobileConfirmed = true;
                break;
            }
            break;
        case DlgDelegate.DISMISS_BUTTON:
            if ( Action.USE_IMMOBILE_ACTION == action && m_immobileConfirmed ) {
                showConfirmThen( R.string.warn_unlimited, 
                                 R.string.button_yes, 
                                 Action.POST_WARNING_ACTION );
            }
            break;
        }
    }

    private int countChecks()
    {
        int count = 0;
        if ( null != m_devIDRecs ) {
            Iterator<DevIDRec> iter = m_devIDRecs.iterator();
            while ( iter.hasNext() ) {
                if ( iter.next().m_isChecked ) {
                    ++count;
                }
            }
        }
        return count;
    }

    // private void addPhoneNumbers( Intent intent )
    // {
    //     Uri data = intent.getData();
    //     Cursor cursor = m_activity
    //         .managedQuery( data, 
    //                        new String[] { Phone.DISPLAY_NAME, 
    //                                       Phone.NUMBER, 
    //                                       Phone.TYPE },
    //                        null, null, null );
    //     // Have seen a crash reporting
    //     // "android.database.StaleDataException: Attempted to access a
    //     // cursor after it has been closed." when the query takes a
    //     // long time to return.  Be safe.
    //     if ( null != cursor && !cursor.isClosed() ) {
    //         if ( cursor.moveToFirst() ) {
    //             String name = 
    //                 cursor.getString( cursor.
    //                                   getColumnIndex( Phone.DISPLAY_NAME));
    //             String number = 
    //                 cursor.getString( cursor.
    //                                   getColumnIndex( Phone.NUMBER ) );

    //             int type = cursor.getInt( cursor.
    //                                       getColumnIndex( Phone.TYPE ) );
    //             // m_pendingName = name;
    //             // m_pendingNumber = number;
    //             if ( Phone.TYPE_MOBILE == type ) {
    //                 showConfirmThen( R.string.warn_unlimited, 
    //                                  R.string.button_yes, 
    //                                  Action.POST_WARNING_ACTION );
    //             } else {
    //                 m_immobileConfirmed = false;
    //                 String msg = getString( R.string.warn_nomobile_fmt, 
    //                                         number, name );
    //                 showConfirmThen( msg, R.string.button_yes, 
    //                                  Action.USE_IMMOBILE_ACTION );
    //             }
    //         }
    //     }
    // } // addPhoneNumbers

    private void rebuildList( boolean checkIfAll )
    {
        Collections.sort( m_devIDRecs, new Comparator<DevIDRec>() {
                public int compare( DevIDRec rec1, DevIDRec rec2 ) {
                    return rec1.m_name.compareTo(rec2.m_name);
                }
            });
        m_adapter = new SMSPhonesAdapter();
        setListAdapter( m_adapter );
        if ( checkIfAll && m_devIDRecs.size() <= m_nMissing ) {
            Iterator<DevIDRec> iter = m_devIDRecs.iterator();
            while ( iter.hasNext() ) {
                iter.next().m_isChecked = true;
            }
        }
        tryEnable();
    }

    private void getSavedState()
    {
        String[] devIDs = XWPrefs.getRelayIDs( m_activity );

        m_devIDRecs = new ArrayList<DevIDRec>(devIDs.length);
        for ( String devID : devIDs ) {
            DevIDRec rec = new DevIDRec( devID );
            m_devIDRecs.add( rec );
        }
    }

    private void saveAndRebuild()
    {
        String[] devIDs = new String[m_devIDRecs.size()];
        Iterator<DevIDRec> iter = m_devIDRecs.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            DevIDRec rec = iter.next();
            devIDs[ii] = rec.m_devID;
        }
        XWPrefs.setRelayIDs( m_activity, devIDs );

        rebuildList( false );
    }

    private void addChecked( DevIDRec rec )
    {
        if ( m_nMissing <= countChecks() ) {
            Iterator<DevIDRec> iter = m_devIDRecs.iterator();
            while ( iter.hasNext() ) {
                iter.next().m_isChecked = false;
            }
        }

        rec.m_isChecked = true;
        m_devIDRecs.add( rec );
    }

    private void clearSelectedImpl()
    {
        int count = m_adapter.getCount();
        for ( int ii = count - 1; ii >= 0; --ii ) {
            if ( m_devIDRecs.get( ii ).m_isChecked ) {
                m_devIDRecs.remove( ii );
            }
        }
        saveAndRebuild();
    }

    private class DevIDRec {
        public String m_devID;
        public String m_name;
        public boolean m_isChecked;
        public int m_nPlayers;
        public DevIDRec( String name, String devID )
        {
            this( name, devID, false );
        }
        public DevIDRec( String devID )
        {
            this( null, devID, false );
        }

        public DevIDRec( String name, String devID, boolean checked )
        {
            m_devID = devID;
            m_isChecked = checked;
            m_nPlayers = 1;

            if ( null == name ) {
                name = "<anon>";
            }
            m_name = name;
        }
    }

    private class SMSPhonesAdapter extends XWListAdapter {
        private SMSListItem[] m_items;

        public SMSPhonesAdapter()
        {
            super( m_devIDRecs.size() );
            m_items = new SMSListItem[m_devIDRecs.size()];
        }

        public Object getItem( final int position ) 
        {
            // For some reason I can't cache items to be returned.
            // Checking/unchecking breaks for some but not all items,
            // with some relation to whether they were scrolled into
            // view.  So build them anew each time (but still cache
            // for by-index access.)

            SMSListItem item = 
                (SMSListItem)inflate( R.layout.smsinviter_item );
            item.setChecked( m_devIDRecs.get(position).m_isChecked );

            CompoundButton.OnCheckedChangeListener lstnr =
                new CompoundButton.OnCheckedChangeListener() {
                    public void onCheckedChanged( CompoundButton bv, 
                                                  boolean isChecked ) {
                        m_devIDRecs.get(position).m_isChecked = isChecked;
                        tryEnable();
                    }
                };
            item.setOnCheckedChangeListener( lstnr );
            final DevIDRec rec = m_devIDRecs.get( position );
            item.setContents( rec.m_name, rec.m_devID );
            m_items[position] = item;

            // Set up spinner
            Assert.assertTrue( 1 == rec.m_nPlayers );
            if ( XWPrefs.getCanInviteMulti( m_activity ) && 1 < m_nMissing ) {
                Spinner spinner = (Spinner)
                    item.findViewById(R.id.nperdev_spinner);
                ArrayAdapter<String> adapter = 
                    new ArrayAdapter<String>( m_activity, android.R.layout
                                              .simple_spinner_item );
                for ( int ii = 1; ii <= m_nMissing; ++ii ) {
                    String str = getQuantityString( R.plurals.nplayers_fmt, ii, ii );
                    adapter.add( str );
                }
                spinner.setAdapter( adapter );
                spinner.setVisibility( View.VISIBLE );
                spinner.setOnItemSelectedListener( new OnItemSelectedListener() {
                        public void onItemSelected( AdapterView<?> parent, 
                                                    View view, int pos, 
                                                    long id )
                        {
                            rec.m_nPlayers = 1 + pos;
                            tryEnable();
                        }

                        public void onNothingSelected( AdapterView<?> parent ) {}
                    } );
            }

            return item;
        }

        public View getView( final int position, View convertView, 
                             ViewGroup parent ) {
            return (View)getItem( position );
        }
    }
}

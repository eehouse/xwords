/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2012 - 2020 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.View;
import android.widget.EditText;
import android.widget.Button;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.Collections;
import java.util.Set;
import java.util.Iterator;

import org.eehouse.android.xw4.DlgDelegate.Action;

abstract class DevIDInviteDelegate extends InviteDelegate {
    private static final String TAG = DevIDInviteDelegate.class.getSimpleName();

    private static int[] BUTTONIDS = {
        R.id.button_relay_add,
        R.id.manual_add_button,
        R.id.button_clear,
        R.id.button_edit,
    };

    protected ArrayList<DevIDRec> m_devIDRecs;
    private Activity m_activity;
    private boolean m_immobileConfirmed; // WTF is this?

    abstract String getRecsKey();
    abstract String getMeDevID();

    public DevIDInviteDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState );
        m_activity = delegator.getActivity();
    }

    static class DevIDRec implements InviterItem, Serializable {
        public String m_devID;
        public String m_opponent;
        public int m_nPlayers;

        public DevIDRec( String opponent, String devID )
        {
            m_devID = devID;
            m_nPlayers = 1;
            m_opponent = opponent;
        }

        public String getDev() { return m_devID; }

        public boolean equals( InviterItem item )
        {
            return item != null
                && ((DevIDRec)item).m_devID == m_devID;
        }
    }

    void saveAndRebuild()
    {
        DBUtils.setSerializableFor( m_activity, getRecsKey(), m_devIDRecs );
        rebuildList( false );
    }

    void rebuildList( boolean checkIfAll )
    {
        Collections.sort( m_devIDRecs, new Comparator<DevIDRec>() {
                public int compare( DevIDRec rec1, DevIDRec rec2 ) {
                    return rec1.m_opponent.compareTo(rec2.m_opponent);
                }
            });

        addSelf();
        updateList( m_devIDRecs );
        tryEnable();
    }

    @Override
    protected void init( Bundle savedInstanceState )
    {
        super.init( savedInstanceState );

        String msg = getString( R.string.button_invite );
        msg = getQuantityString( R.plurals.invite_relay_desc_fmt, m_nMissing,
                                 m_nMissing, msg );
        super.init( msg, R.string.empty_relay_inviter );
        addButtonBar( R.layout.relay_buttons, BUTTONIDS );

        getSavedState();
        rebuildList( true );
    }

    @Override
    protected void onBarButtonClicked( int id )
    {
        switch( id ) {
        case R.id.button_relay_add:
            Utils.notImpl( getActivity() );
            break;
        case R.id.manual_add_button:
            showDialogFragment( DlgID.GET_NUMBER );
            break;
        case R.id.button_clear:
            int count = getChecked().size();
            String msg = getQuantityString( R.plurals.confirm_clear_relay_fmt,
                                            count, count );
            makeConfirmThenBuilder( msg, Action.CLEAR_ACTION ).show();
            break;
        case R.id.button_edit:
            Object obj = getChecked().iterator().next();
            Log.d( TAG, "passing %s", obj );
            showDialogFragment( DlgID.GET_NUMBER, obj );
            break;
        }
    }

    @Override
    protected Dialog makeDialog( DBAlert alert, Object[] params )
    {
        Dialog dialog = null;
        DialogInterface.OnClickListener lstnr;
        switch( alert.getDlgID() ) {
        case GET_NUMBER: {
            final DevIDRec curRec = 
                1 <= params.length && params[0] instanceof String
                ? getHasID((String)params[0]) : null;
            Log.d( TAG, "curRec: %s", curRec );
            final View getNumView = inflate( R.layout.get_relay );
            final EditText numField = (EditText)
                getNumView.findViewById( R.id.num_field );
            final EditText nameField = (EditText)
                getNumView.findViewById( R.id.name_field );
            if ( null != curRec ) {
                numField.setText( curRec.m_devID );
                nameField.setText( curRec.m_opponent );
            }
            lstnr = new DialogInterface.OnClickListener() {
                    public void onClick( DialogInterface dlg, int item ) {
                        String number = numField.getText().toString();
                        if ( null != number && 0 < number.length() ) {
                            String name = nameField.getText().toString();
                            if ( curRec != null ) {
                                curRec.m_opponent = name;
                                curRec.m_devID = number;
                            } else {
                                DevIDRec rec = new DevIDRec( name, number );
                                m_devIDRecs.add( rec );
                                clearChecked();
                                onItemChecked( rec, true );
                            }
                            saveAndRebuild();
                        }
                    }
                };
            dialog = makeAlertBuilder()
                .setTitle( R.string.get_sms_title )
                .setView( getNumView )
                .setPositiveButton( android.R.string.ok, lstnr )
                .setNegativeButton( android.R.string.cancel, null )
                .create();
        }
            break;
        default:
            dialog = super.makeDialog( alert, params );
            break;
        }
        return dialog;
    }

    @Override
    protected void onChildAdded( View child, InviterItem data )
    {
        DevIDRec rec = (DevIDRec)data;
        ((TwoStrsItem)child).setStrings( rec.m_opponent, rec.m_devID );
    }

    @Override
    protected void tryEnable()
    {
        super.tryEnable();

        Button button = (Button)findViewById( R.id.button_clear );
        if ( null != button ) { // may not be there yet
            button.setEnabled( 0 < getChecked().size() );
        }
        button = (Button)findViewById( R.id.button_edit );
        if ( null != button ) {
            button.setEnabled( 1 == getChecked().size() );
        }
    }

    // DlgDelegate.DlgClickNotify interface
    @Override
    public boolean onPosButton( Action action, Object[] params )
    {
        boolean handled = true;
        switch( action ) {
        case CLEAR_ACTION:
            clearSelectedImpl();
            break;
        case USE_IMMOBILE_ACTION:
            m_immobileConfirmed = true;
            break;
        default:
            handled = super.onPosButton( action, params );
            break;
        }
        return handled;
    }

    @Override
    public boolean onDismissed( Action action, Object[] params )
    {
        boolean handled = true;
        if ( Action.USE_IMMOBILE_ACTION == action && m_immobileConfirmed ) {
            makeConfirmThenBuilder( R.string.warn_unlimited,
                                    Action.POST_WARNING_ACTION )
                .setPosButton( R.string.button_yes )
                .show();
        } else {
            handled = false;
        }
        return handled;
    }

    void addSelf()
    {
        boolean hasSelf = false;
        String me = getMeDevID();
        for ( DevIDRec rec : m_devIDRecs ) {
            if ( rec.m_devID.equals( me ) ) {
                hasSelf = true;
                break;
            }
        }
        if ( !hasSelf ) {
            DevIDRec rec = new DevIDRec( "self", me );
            m_devIDRecs.add( rec );
        }
    }

    void getSavedState()
    {
        m_devIDRecs = (ArrayList<DevIDRec>)DBUtils.getSerializableFor( m_activity, getRecsKey() );
        if ( null == m_devIDRecs ) {
            m_devIDRecs = new ArrayList<>();
        }
    }

    void clearSelectedImpl()
    {
        Set<String> checked = getChecked();
        for ( Iterator<DevIDRec> iter = m_devIDRecs.iterator(); iter.hasNext(); ) {
            if ( checked.contains( iter.next().getDev() ) ) {
                iter.remove();
            }
        }
        clearChecked();
        saveAndRebuild();
    }

    DevIDRec getHasID( String id )
    {
        DevIDRec result = null;
        for ( DevIDRec rec : m_devIDRecs ) {
            if ( rec.m_devID.equals(id) ) {
                result = rec;
                break;
            }
        }
        return result;
    }
}

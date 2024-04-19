/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2013 by Eric House (xwords@eehouse.org).  All
 * rights reserved.
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

import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;


public class DlgState implements Parcelable {
    private static final String TAG = DlgState.class.getSimpleName();
    private static final String BUNDLE_KEY = "bk";

    public DlgID m_id;
    public String m_msg;
    public int m_posButton;
    public int m_negButton;
    public Action m_action = null;
    public ActionPair m_pair = null;
    public int m_prefsNAKey;
    // These can't be serialized!!!!
    private Object[] m_params = {};
    public String m_title;

    public DlgState( DlgID dlgID )
    {
        m_id = dlgID;
    }

    public DlgState setMsg( String msg )
    { m_msg = msg; return this; }
    public DlgState setPrefsNAKey( int key )
    { m_prefsNAKey = key; return this; }
    public DlgState setAction( Action action )
    { m_action = action; return this; }
    public DlgState setParams( Object... params )
    {
        if ( BuildConfig.DEBUG && null != params ) {
            for ( Object obj : params ) {
                if ( null != obj && !(obj instanceof Serializable) ) {
                    Log.d( TAG, "OOPS: %s not Serializable",
                           obj.getClass().getName() );
                    Assert.failDbg();
                }
            }
        }
        m_params = params;
        return this;
    }
    public DlgState setActionPair( ActionPair pair )
    { m_pair = pair; return this; }
    public DlgState setPosButton( int id )
    { m_posButton = id; return this; }
    public DlgState setNegButton( int id )
    { m_negButton = id; return this; }
    public DlgState setTitle( String title )
    { m_title = title; return this; }

    public Object[] getParams() { return m_params; }

    @Override
    public String toString()
    {
        String result;
        if ( BuildConfig.DEBUG) {
            String params = "";
            if ( null != m_params ) {
                List<String> strs = new ArrayList<>();
                for (Object obj : m_params) {
                    strs.add(String.format("%s", obj));
                }
                params = TextUtils.join( ",", strs );
            }
            result = new StringBuffer()
                .append("{id: ").append(m_id)
                .append(", msg: \"").append(m_msg)
                .append("\", naKey: ").append(m_prefsNAKey)
                .append(", action: ").append(m_action)
                .append(", pair ").append(m_pair)
                .append(", pos: ").append(m_posButton)
                .append(", neg: ").append(m_negButton)
                .append(", title: ").append(m_title)
                .append(", params: [").append(params)
                .append("]}")
                .toString();
        } else {
            result = super.toString();
        }
        return result;
    }

    // I only need this if BuildConfig.DEBUG is true...
    @Override
    public boolean equals(Object it)
    {
        boolean result;
        if ( BuildConfig.DEBUG ) {
            result = it != null && it instanceof DlgState;
            if ( result ) {
                DlgState other = (DlgState)it;
                result = other != null
                    && m_id.equals(other.m_id)
                    && TextUtils.equals( m_msg, other.m_msg)
                    && m_posButton == other.m_posButton
                    && m_negButton == other.m_negButton
                    && m_action == other.m_action
                    && ((null == m_pair) ? (null == other.m_pair) : m_pair.equals(other.m_pair))
                    && m_prefsNAKey == other.m_prefsNAKey
                    && Arrays.deepEquals( m_params, other.m_params )
                    && TextUtils.equals( m_title,other.m_title)
                    ;
            }
        } else {
            result = super.equals( it );
        }
        return result;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public Bundle toBundle()
    {
        testCanParcelize();

        Bundle result = new Bundle();
        result.putParcelable( BUNDLE_KEY, this );
        return result;
    }

    public static DlgState fromBundle( Bundle bundle )
    {
        return (DlgState)bundle.getParcelable( BUNDLE_KEY );
    }

    @Override
    public void writeToParcel( Parcel out, int flags )
    {
        out.writeInt( m_id.ordinal() );
        out.writeInt( m_posButton );
        out.writeInt( m_negButton );
        out.writeInt( null == m_action ? -1 : m_action.ordinal() );
        out.writeInt( m_prefsNAKey );
        out.writeString( m_title );
        out.writeString( m_msg );
        out.writeSerializable( m_params );
        out.writeSerializable( m_pair );
    }

    private void testCanParcelize()
    {
        if ( BuildConfig.DEBUG ) {
            Parcel parcel = Parcel.obtain();
            writeToParcel(parcel, 0);

            parcel.setDataPosition(0);

            DlgState newState = DlgState.CREATOR.createFromParcel(parcel);
            Assert.assertFalse(newState == this);
            if ( !this.equals(newState) ) {
                Log.d( TAG, "restore failed!!: %s => %s", this, newState );
                Assert.failDbg();
            }
        }
    }

    public static final Parcelable.Creator<DlgState> CREATOR
        = new Parcelable.Creator<DlgState>() {
                @Override
                public DlgState createFromParcel(Parcel in) {
                    DlgID id = DlgID.values()[in.readInt()];
                    int posButton = in.readInt();
                    int negButton = in.readInt();
                    int tmp = in.readInt();
                    Action action = 0 > tmp ? null : Action.values()[tmp];
                    int prefsKey = in.readInt();
                    String title = in.readString();
                    String msg = in.readString();
                    Object[] params = (Object[])in.readSerializable();
                    ActionPair pair = (ActionPair)in.readSerializable();
                    DlgState state = new DlgState(id)
                    .setMsg( msg )
                    .setPosButton( posButton )
                    .setNegButton( negButton )
                    .setAction( action )
                    .setPrefsNAKey( prefsKey )
                    .setTitle(title)
                    .setParams(params)
                    .setActionPair(pair)
                    ;
                    return state;
                }

                @Override
                public DlgState[] newArray(int size) {
                    return new DlgState[size];
                }
            };
}

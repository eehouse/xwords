/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

import android.os.Parcel;
import android.os.Parcelable;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;

public class DlgState implements Parcelable {
    public DlgID m_id;
    public String m_msg;
    public int m_posButton;
    public int m_negButton;
    public Action m_action = null;
    public ActionPair m_pair = null;
    public int m_prefsKey;
    // These can't be serialized!!!!
    public Object[] m_params;
    public Runnable m_onNAChecked;

    public DlgState( DlgID dlgID )
    {
        m_id = dlgID;
    }

    public DlgState setMsg( String msg )
    { m_msg = msg; return this; }
    public DlgState setPrefsKey( int key )
    { m_prefsKey = key; return this; }
    public DlgState setAction( Action action )
    { m_action = action; return this; }
    public DlgState setParams( Object... params )
    { m_params = params; return this; }
    public DlgState setActionPair( ActionPair pair )
    { m_pair = pair; return this; }
    public DlgState setOnNA( Runnable na )
    { m_onNAChecked = na; return this; }
    public DlgState setPosButton( int id )
    { m_posButton = id; return this; }
    public DlgState setNegButton( int id )
    { m_negButton = id; return this; }

    public int describeContents() {
        return 0;
    }

    public void writeToParcel( Parcel out, int flags ) {
        out.writeInt( m_id.ordinal() );
        out.writeInt( m_posButton );
        out.writeInt( m_negButton );
        out.writeInt( null == m_action ? -1 : m_action.ordinal() );
        out.writeInt( m_prefsKey );
        out.writeString( m_msg );
    }

    public static final Parcelable.Creator<DlgState> CREATOR
        = new Parcelable.Creator<DlgState>() {
                public DlgState createFromParcel(Parcel in) {
                    DlgID id = DlgID.values()[in.readInt()];
                    int posButton = in.readInt();
                    int negButton = in.readInt();
                    int tmp = in.readInt();
                    Action action = 0 > tmp ? null : Action.values()[tmp];
                    int prefsKey = in.readInt();
                    String msg = in.readString();
                    DlgState state = new DlgState(id)
                    .setMsg( msg )
                    .setPosButton( posButton )
                    .setNegButton( negButton )
                    .setAction( action )
                    .setPrefsKey( prefsKey );
                    return state;
                }

                public DlgState[] newArray(int size) {
                    return new DlgState[size];
                }
            };
}

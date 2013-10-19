/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import android.os.Parcelable;
import android.os.Parcel;

public class DlgState implements Parcelable {
    public int m_id;
    public String m_msg;
    public int m_posButton;
    public int m_cbckID = 0;
    public int m_prefsKey;
    public Object[] m_params;

    public DlgState( int id, String msg, int cbckID )
    {
        this( id, msg, R.string.button_ok, cbckID, 0 );
    }

    public DlgState( int id, String msg, int cbckID, int prefsKey )
    {
        this( id, msg, R.string.button_ok, cbckID, prefsKey );
    }

    public DlgState( int id, String msg, int cbckID, int prefsKey, 
                     Object[] params )
    {
        this( id, msg, R.string.button_ok, cbckID, prefsKey );
        m_params = params;
    }

    public DlgState( int id, String msg, int posButton, 
                     int cbckID, int prefsKey )
    {
        this( id, msg, posButton, cbckID, prefsKey, null );
    }

    public DlgState( int id, String msg, int posButton, 
                     int cbckID, int prefsKey, Object[] params )
    {
        m_id = id;
        m_msg = msg;
        m_posButton = posButton;
        m_cbckID = cbckID;
        m_prefsKey = prefsKey;
        m_params = params;
    }

    public DlgState( int id, int cbckID )
    {
        this( id, null, 0, cbckID, 0 );
    }

    public int describeContents() {
        return 0;
    }

    public void writeToParcel( Parcel out, int flags ) {
        out.writeInt( m_id );
        out.writeInt( m_posButton );
        out.writeInt( m_cbckID );
        out.writeInt( m_prefsKey );
        out.writeString( m_msg );
    }

    public static final Parcelable.Creator<DlgState> CREATOR
        = new Parcelable.Creator<DlgState>() {
        public DlgState createFromParcel(Parcel in) {
            int id = in.readInt();
            int posButton = in.readInt();
            int cbckID = in.readInt();
            int prefsKey = in.readInt();
            String msg = in.readString();
            return new DlgState( id, msg, posButton, cbckID, prefsKey );
        }

        public DlgState[] newArray(int size) {
            return new DlgState[size];
        }
    };
}

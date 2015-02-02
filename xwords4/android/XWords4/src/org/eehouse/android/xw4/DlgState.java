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

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.ActionPair;

import android.os.Parcelable;
import android.os.Parcel;

public class DlgState implements Parcelable {
    public DlgID m_id;
    public String m_msg;
    public int m_posButton;
    public int m_negButton;
    public Action m_action = null;
    public ActionPair m_pair = null;
    public int m_prefsKey;
    public Object[] m_params;

    public DlgState( DlgID dlgID, String msg, Action action )
    {
        this( dlgID, msg, R.string.button_ok, action, 0 );
    }

    public DlgState( DlgID dlgID, String msg, Action action, int prefsKey )
    {
        this( dlgID, msg, R.string.button_ok, action, prefsKey );
    }

    public DlgState( DlgID dlgID, String msg, int prefsKey, Action action, 
                     ActionPair more, Object[] params )
    {
        this( dlgID, msg, R.string.button_ok, action, prefsKey );
        m_params = params;
        m_pair = more;
    }

    public DlgState( DlgID dlgID, String msg, int posButton, 
                     Action action, int prefsKey )
    {
        this( dlgID, msg, posButton, action, prefsKey, null );
    }

    public DlgState( DlgID dlgID, String msg, int posButton, 
                     Action action, int prefsKey, Object[] params )
    {
        this( dlgID, msg, posButton, R.string.button_cancel, 
              action, prefsKey, params );
    }
    
    public DlgState( DlgID dlgID, String msg, int posButton, int negButton, 
                     Action action, int prefsKey, Object[] params )
    {
        m_id = dlgID;
        m_msg = msg;
        m_posButton = posButton;
        m_negButton = negButton;
        m_action = action;
        m_prefsKey = prefsKey;
        m_params = params;
    }

    public DlgState( DlgID dlgID, Action action )
    {
        this( dlgID, null, 0, action, 0 );
    }

    public DlgState( DlgID dlgID, Object[] params )
    {
        this( dlgID, null, 0, null, 0, params );
    }

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
            return new DlgState( id, msg, posButton, negButton, action, prefsKey, null );
        }

        public DlgState[] newArray(int size) {
            return new DlgState[size];
        }
    };
}

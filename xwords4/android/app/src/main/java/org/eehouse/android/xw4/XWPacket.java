/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
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

import org.json.JSONObject;
import org.json.JSONException;
import org.json.JSONArray;

public class XWPacket {
    private static final String TAG = XWPacket.class.getSimpleName();
    private static final String KEY_CMD = "cmd";

    // This can't change after ship!!!!
    private static final boolean CMDS_AS_STRINGS = true;
    
    private JSONObject m_obj;

    public enum CMD {
        PING,
        PONG,
        MSG,
        INVITE,
        NOGAME,
    }

    public XWPacket( CMD cmd ) {
        try {
            m_obj = new JSONObject();
            if ( CMDS_AS_STRINGS ) {
                m_obj.put( KEY_CMD, cmd.toString() );
            } else {
                m_obj.put( KEY_CMD, cmd.ordinal() );
            }
        } catch ( JSONException ex ) {
            Log.d( TAG, ex.toString() );
        }
    }

    public XWPacket( String str )
    {
        try {
            m_obj = new JSONObject( str );
        } catch ( JSONException ex ) {
            Log.d( TAG, ex.toString() );
        }
    }

    public CMD getCommand()
    {
        CMD cmd = null;
        if ( CMDS_AS_STRINGS ) {
            String str = m_obj.optString( KEY_CMD );
            for ( CMD one : CMD.values() ) {
                if ( one.toString().equals(str)) {
                    cmd = one;
                    break;
                }
            }
        } else {
            int ord = m_obj.optInt( KEY_CMD, -1 ); // let's blow up :-)
            cmd = CMD.values()[ord];
        }
        return cmd;
    }

    public XWPacket put( String key, String value )
    {
        try {
            m_obj.put( key, value );
        } catch ( JSONException ex ) {
            Log.d( TAG, ex.toString() );
        }
        return this;
    }

    public XWPacket put( String key, int value )
    {
        try {
            m_obj.put( key, value );
        } catch ( JSONException ex ) {
            Log.d( TAG, ex.toString() );
        }
        return this;
    }

    public XWPacket put( String key, JSONArray value )
    {
        try {
            m_obj.put( key, value );
        } catch ( JSONException ex ) {
            Log.d( TAG, ex.toString() );
        }
        return this;
    }

    public String getString( String key )
    {
        String str = m_obj.optString( key );
        return str;
    }

    public int getInt( String key, int dflt )
    {
        int ii = m_obj.optInt( key, dflt );
        return ii;
    }

    public JSONArray getJSONArray( String key )
    {
        JSONArray array = null;
        try {
            array = m_obj.getJSONArray( key );
        } catch ( JSONException ex ) {
        }
        return array;
    }


    @Override
    public String toString() { return m_obj.toString(); }
}

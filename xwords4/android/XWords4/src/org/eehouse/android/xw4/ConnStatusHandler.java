/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import org.eehouse.android.xw4.jni.CommsAddrRec;


public class ConnStatusHandler {
    private static CommsAddrRec.CommsConnType s_connType = 
        CommsAddrRec.CommsConnType.COMMS_CONN_NONE;
    private static Rect s_rect;
    private static boolean s_downOnMe = false;

    public static void setRect( int left, int top, int right, int bottom )
    {
        s_rect = new Rect( left, top, right, bottom );
    }

    public static void setType( CommsAddrRec.CommsConnType connType )
    {
        s_connType = connType;
    }

    public static boolean handleDown( int xx, int yy )
    {
        s_downOnMe = s_rect.contains( xx, yy );
        return s_downOnMe;
    }

    public static boolean handleUp( int xx, int yy )
    {
        boolean result = s_downOnMe && s_rect.contains( xx, yy );
        s_downOnMe = false;
        return result;
    }

    public static boolean handleMove( int xx, int yy )
    {
        return s_downOnMe && s_rect.contains( xx, yy );
    }

    public static String getStatusText()
    {
        // To be improved :-)
        return "You tapped the connection icon.";
    }

    public static void draw( Canvas canvas, Resources res, 
                             int offsetX, int offsetY )
    {
        if ( null != s_rect ) {
            int iconID;
            switch( s_connType ) {
            case COMMS_CONN_RELAY:
                iconID = R.drawable.relaygame;
                break;
            case COMMS_CONN_SMS:
                iconID = android.R.drawable.sym_action_chat;
                break;
            case COMMS_CONN_BT:
                iconID = android.R.drawable.stat_sys_data_bluetooth;
                break;
            case COMMS_CONN_NONE:
            default:
                iconID = R.drawable.sologame;
                break;
            }
            Drawable icon = res.getDrawable( iconID );
            Rect rect = new Rect( s_rect );
            rect.offset( offsetX, offsetY );
            icon.setBounds( rect );
            icon.draw( canvas );
        }
    }
}

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
import android.text.format.Time;
import java.util.HashMap;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class ConnStatusHandler {
    private static CommsConnType s_connType = CommsConnType.COMMS_CONN_NONE;
    private static Rect s_rect;
    private static boolean s_downOnMe = false;
    // private static Object s_syncMe = new Object();

    private static class SuccessRecord {
        // man strftime for these
        private static final String TIME_FMT = "%X %x";
        private static final Time s_zero = new Time();
        public Time lastSuccess;
        public Time lastFailure;
        public boolean successNewer;

        public SuccessRecord()
        {
            lastSuccess = new Time();
            lastFailure = new Time();
            successNewer = false;
        }

        public boolean haveFailure()
        {
            return lastFailure.after( s_zero );
        }

        public boolean haveSuccess()
        {
            return lastSuccess.after( s_zero );
        }

        public String newerStr()
        {
            Time which = successNewer? lastSuccess : lastFailure;
            return which.format( TIME_FMT );
        }

        public String olderStr()
        {
            Time which = successNewer? lastFailure : lastSuccess;
            return which.format( TIME_FMT );
        }

        public void update( boolean success ) 
        {
            Time last = success? lastSuccess : lastFailure;
            last.setToNow();
            successNewer = success;
        }
    }

    private static HashMap<CommsConnType,SuccessRecord[]> s_records = 
        new HashMap<CommsConnType,SuccessRecord[]>();

    public static void setRect( int left, int top, int right, int bottom )
    {
        s_rect = new Rect( left, top, right, bottom );
    }

    public static void setType( CommsConnType connType )
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
        String msg;
        if ( CommsConnType.COMMS_CONN_NONE == s_connType ) {
            msg = "This is a standalone game. There is no network status.";
        } else {
            synchronized( s_records ) {
                msg = "Network status for game connected via " + connType2Str();
                msg += ":\n\n";
                SuccessRecord record = recordFor( s_connType, false );
                msg += 
                    String.format( "Last send was %s (at %s)\n",
                                   record.successNewer? "successful":"unsuccessful", 
                                   record.newerStr() );

                String fmt = null;
                if ( record.successNewer ) {
                    if ( record.haveFailure() ) {
                        fmt = "(Last failure was at %s)\n";
                    }
                } else {
                    if ( record.haveSuccess() ) {
                        fmt = "(Last successful send was at %s)\n";
                    }
                }
                if ( null != fmt ) {
                    msg += String.format( fmt, record.olderStr() );
                }
                msg += "\n";

                record = recordFor( s_connType, true );
                if ( record.haveSuccess() ) {
                    msg += 
                        String.format( "Last receipt was at %s",
                                       record.newerStr() );
                } else {
                    msg += "No messages have been received.";
                }
            }
        }
        return msg;
    }

    public static void updateStatusIn( CommsConnType connType, boolean success )
    {
        synchronized( s_records ) {
            SuccessRecord record = recordFor( connType, true );
            record.update( success );
        }
    }

    public static void updateStatusOut( CommsConnType connType, boolean success )
    {
        synchronized( s_records ) {
            SuccessRecord record = recordFor( connType, false );
            record.update( success );
        }
    }

    public static void draw( Canvas canvas, Resources res, 
                             int offsetX, int offsetY )
    {
        synchronized( s_records ) {
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

                Rect rect = new Rect( s_rect );
                int quarterHeight = rect.height() / 4;
                rect.offset( offsetX, offsetY );

                rect.top += quarterHeight;
                rect.bottom = rect.top + (2 * quarterHeight);
                drawIn( canvas, res, iconID, rect );

                if ( CommsConnType.COMMS_CONN_NONE != s_connType ) {
                    SuccessRecord record = recordFor( s_connType, true );
                    int inID = record.successNewer ? R.drawable.in_success
                        : R.drawable.in_failure;
                    record = recordFor( s_connType, false );
                    int outID = record.successNewer ? R.drawable.out_success
                        : R.drawable.out_failure;

                    rect.bottom = rect.top;
                    rect.top -= quarterHeight;
                    drawIn( canvas, res, outID, rect );

                    rect.top += 3 * quarterHeight;
                    rect.bottom = rect.top + quarterHeight;
                    drawIn( canvas, res, inID, rect );
                } 
            }
        }
    }

    private static void drawIn( Canvas canvas, Resources res, int id, Rect rect )
    {
        Drawable icon = res.getDrawable( id );
        icon.setBounds( rect );
        icon.draw( canvas );
    }

    private static String connType2Str()
    {
        String result = null;
        switch( s_connType ) {
        case COMMS_CONN_RELAY:
            result = "internet/relay";
            break;
        case COMMS_CONN_SMS:
            result = "sms/texting";
            break;
        case COMMS_CONN_BT:
            result = "bluetooth";
            break;
        default:
            Assert.fail();
        }
        return result;
    }

    private static SuccessRecord recordFor( CommsConnType connType, boolean isIn )
    {
        SuccessRecord[] records = s_records.get(connType);
        if ( null == records ) {
            records = new SuccessRecord[2];
            s_records.put( connType, records );
            for ( int ii = 0; ii < 2; ++ii ) {
                records[ii] = new SuccessRecord();
            }
        }
        return records[isIn?0:1];
    }

}

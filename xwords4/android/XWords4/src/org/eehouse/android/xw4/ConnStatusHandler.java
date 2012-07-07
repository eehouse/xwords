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


import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Message;
import android.text.format.DateUtils;
import android.text.format.Time;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.HashMap;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.XwJNI;

public class ConnStatusHandler {
    // private static final int GREEN = 0x7F00FF00;
    // private static final int RED = 0x7FFF0000;
    private static final int GREEN = 0xFF00FF00;
    private static final int RED = 0xFFFF0000;

    private static CommsConnType s_connType = CommsConnType.COMMS_CONN_NONE;
    private static Rect s_rect;
    private static boolean s_downOnMe = false;
    private static Handler s_handler;
    private static Paint s_fillPaint = new Paint( Paint.ANTI_ALIAS_FLAG );

    private static class SuccessRecord implements java.io.Serializable {
        // man strftime for these
        // private static final String TIME_FMT = "%X %x";
        public long lastSuccess;
        public long lastFailure;
        public boolean successNewer;
        transient private Time m_time;

        public SuccessRecord()
        {
            m_time = new Time();
            lastSuccess = 0;
            lastFailure = 0;
            successNewer = false;
        }

        public boolean haveFailure()
        {
            return lastFailure > 0;
        }

        public boolean haveSuccess()
        {
            return lastSuccess > 0;
        }

        public String newerStr( Context context )
        {
            m_time.set( successNewer? lastSuccess : lastFailure );
            return format( context, m_time );
        }

        public String olderStr( Context context )
        {
            m_time.set( successNewer? lastFailure : lastSuccess );
            return format( context, m_time );
        }

        public void update( boolean success ) 
        {
            long now = System.currentTimeMillis();
            if ( success ) {
                lastSuccess = now;
            } else {
                lastFailure = now;
            }
            successNewer = success;
        }

        private String format( Context context, Time time ) 
        {
            CharSequence seq = 
                DateUtils.getRelativeDateTimeString( context, 
                                                     time.toMillis(true),
                                                     DateUtils.MINUTE_IN_MILLIS, 
                                                     DateUtils.WEEK_IN_MILLIS, 
                                                     0 );
            return seq.toString();
        }

        // called during deserialization
        private void readObject( ObjectInputStream in ) 
            throws java.io.IOException, java.lang.ClassNotFoundException
        {
            in.defaultReadObject();
            m_time = new Time();
        }
    }

    private static HashMap<CommsConnType,SuccessRecord[]> s_records = 
        new HashMap<CommsConnType,SuccessRecord[]>();
    private static Object s_lockObj = new Object();

    public static void setRect( int left, int top, int right, int bottom )
    {
        s_rect = new Rect( left, top, right, bottom );
    }

    public static void setType( CommsConnType connType )
    {
        s_connType = connType;
    }

    public static void setHandler( Handler handler )
    {
        s_handler = handler;
    }

    public static boolean handleDown( int xx, int yy )
    {
        s_downOnMe = null != s_rect && s_rect.contains( xx, yy );
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

    public static String getStatusText( Context context )
    {
        String msg;
        if ( CommsConnType.COMMS_CONN_NONE == s_connType ) {
            msg = "This is a standalone game. There is no network status.";
        } else {
            synchronized( s_lockObj ) {
                msg = "Network status for game connected via " + connType2Str();
                msg += ":\n\n";
                SuccessRecord record = recordFor( s_connType, false );
                msg += 
                    String.format( "Last send was %s (at %s)\n",
                                   record.successNewer? "successful":"unsuccessful", 
                                   record.newerStr( context ) );

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
                    msg += String.format( fmt, record.olderStr( context ) );
                }
                msg += "\n";

                record = recordFor( s_connType, true );
                if ( record.haveSuccess() ) {
                    msg += 
                        String.format( "Last receipt was at %s",
                                       record.newerStr( context ) );
                } else {
                    msg += "No messages have been received.";
                }
            }
        }
        return msg;
    }

    private static void invalidateParent()
    {
        if ( null != s_handler ) {
            Message.obtain( s_handler ).sendToTarget();
        }
    }
   
    public static void updateStatusIn( Context context, 
                                       CommsConnType connType, boolean success )
    {
        synchronized( s_lockObj ) {
            SuccessRecord record = recordFor( connType, true );
            record.update( success );
        }
        invalidateParent();
        saveState( context );
    }

    public static void updateStatusOut( Context context, 
                                        CommsConnType connType, boolean success )
    {
        synchronized( s_lockObj ) {
            SuccessRecord record = recordFor( connType, false );
            record.update( success );
        }
        invalidateParent();
        saveState( context );
    }

    public static void draw( Canvas canvas, Resources res, 
                             int offsetX, int offsetY )
    {
        synchronized( s_lockObj ) {
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

                if ( CommsConnType.COMMS_CONN_NONE != s_connType ) {
                    int saveTop = rect.top;
                    SuccessRecord record;

                    // Do the background coloring
                    rect.bottom = rect.top + quarterHeight * 2;
                    record = recordFor( s_connType, false );
                    s_fillPaint.setColor( record.successNewer ? GREEN : RED );
                    canvas.drawRect( rect, s_fillPaint );
                    rect.top = rect.bottom;
                    rect.bottom = rect.top + quarterHeight * 2;
                    record = recordFor( s_connType, true );
                    s_fillPaint.setColor( record.successNewer ? GREEN : RED );
                    canvas.drawRect( rect, s_fillPaint );

                    // now the icons
                    rect.top = saveTop;
                    rect.bottom = rect.top + quarterHeight;
                    drawIn( canvas, res, R.drawable.out_arrow, rect );

                    rect.top += 3 * quarterHeight;
                    rect.bottom = rect.top + quarterHeight;
                    drawIn( canvas, res, R.drawable.in_arrow, rect );

                    rect.top = saveTop;
                }

                rect.top += quarterHeight;
                rect.bottom = rect.top + (2 * quarterHeight);
                drawIn( canvas, res, iconID, rect );
            }
        }
    }

    public static void loadState( Context context )
    {
        synchronized( s_lockObj ) {
            String as64 = CommonPrefs.getPrefsString( context, 
                                                      R.string.key_connstat_data );
            if ( null != as64 && 0 < as64.length() ) {
                byte[] bytes = XwJNI.base64Decode( as64 );
                try {
                    ObjectInputStream ois = 
                        new ObjectInputStream( new ByteArrayInputStream(bytes) );
                    s_records = 
                        (HashMap<CommsConnType,SuccessRecord[]>)ois.readObject();
                // } catch ( java.io.StreamCorruptedException sce ) {
                //     DbgUtils.logf( "loadState: %s", sce.toString() );
                // } catch ( java.io.OptionalDataException ode ) {
                //     DbgUtils.logf( "loadState: %s", ode.toString() );
                // } catch ( java.io.IOException ioe ) {
                //     DbgUtils.logf( "loadState: %s", ioe.toString() );
                // } catch ( java.lang.ClassNotFoundException cnfe ) {
                //     DbgUtils.logf( "loadState: %s", cnfe.toString() );
                } catch ( Exception ex ) {
                    DbgUtils.logf( "loadState: %s", ex.toString() );
                }
            }
        }
    }

    private static void saveState( Context context )
    {
        DbgUtils.logf( "saveState called; need to coalesce these!!!" );
        synchronized( s_lockObj ) {
            ByteArrayOutputStream bas = new ByteArrayOutputStream();
            try {
                ObjectOutputStream out = new ObjectOutputStream( bas );
                out.writeObject(s_records);
                out.flush();
                String as64 = XwJNI.base64Encode( bas.toByteArray() );
                CommonPrefs.setPrefsString( context, 
                                            R.string.key_connstat_data, as64 );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.logf( "loadState: %s", ioe.toString() );
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

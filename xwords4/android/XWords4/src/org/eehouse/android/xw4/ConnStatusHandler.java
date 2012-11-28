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
import android.provider.Settings;
import android.text.format.DateUtils;
import android.text.format.Time;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.HashMap;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.XwJNI;

public class ConnStatusHandler {
    // private static final int GREEN = 0x7F00FF00;
    // private static final int RED = 0x7FFF0000;
    private static final int GREEN = 0xFF00FF00;
    private static final int RED = 0xFFFF0000;
    private static final int SUCCESS_IN = 0;
    private static final int SUCCESS_OUT = 1;
    private static final int SHOW_SUCCESS_INTERVAL = 1000;

    private static Rect s_rect;
    private static boolean s_downOnMe = false;
    private static Handler s_handler;
    private static Paint s_fillPaint = new Paint( Paint.ANTI_ALIAS_FLAG );
    private static boolean[] s_showSuccesses = { false, false };

    private static class SuccessRecord implements java.io.Serializable {
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
    private static Class s_lockObj = ConnStatusHandler.class;
    private static boolean s_needsSave = false;

    public static void setRect( int left, int top, int right, int bottom )
    {
        s_rect = new Rect( left, top, right, bottom );
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

    public static String getStatusText( Context context, CommsConnType connType )
    {
        String msg;
        if ( CommsConnType.COMMS_CONN_NONE == connType ) {
            msg = context.getString( R.string.connstat_nonet );
        } else {
            StringBuffer sb = new StringBuffer();
            synchronized( s_lockObj ) {
                String tmp = context.getString( connType2StrID( connType ) );
                sb.append( Utils.format( context, R.string.connstat_net, 
                                         tmp ) );
                sb.append("\n\n");

                SuccessRecord record = recordFor( connType, false );
                tmp = context.getString( record.successNewer? 
                                         R.string.connstat_succ :
                                         R.string.connstat_unsucc );
                sb.append( Utils.format( context, R.string.connstat_lastsend,
                                         tmp, record.newerStr( context ) ) );
                sb.append("\n");

                int fmtId = 0;
                if ( record.successNewer ) {
                    if ( record.haveFailure() ) {
                        fmtId = R.string.connstat_lastother_succ;
                    }
                } else {
                    if ( record.haveSuccess() ) {
                        fmtId = R.string.connstat_lastother_unsucc;
                    }
                }
                if ( 0 != fmtId ) {
                    sb.append( Utils.format( context, fmtId, 
                                            record.olderStr( context ) ) );
                }
                sb.append( "\n\n" );

                record = recordFor( connType, true );
                if ( record.haveSuccess() ) {
                    sb.append( Utils.format( context, 
                                             R.string.connstat_lastreceipt,
                                             record.newerStr( context ) ) );
                } else {
                    sb.append( context.getString(R.string.connstat_noreceipt) );
                }
            }
            msg = sb.toString();
        }
        return msg;
    }

    private static void invalidateParent()
    {
        if ( null != s_handler ) {
            Message.obtain( s_handler ).sendToTarget();
        }
    }
   
    public static void updateStatusIn( Context context, Handler handler,
                                       CommsConnType connType, boolean success )
    {
        synchronized( s_lockObj ) {
            SuccessRecord record = recordFor( connType, true );
            record.update( success );
        }
        invalidateParent();
        saveState( context, handler );
        if ( success ) {
            showSuccess( handler, true );
        }
    }

    public static void updateStatusOut( Context context, Handler handler,
                                        CommsConnType connType, boolean success )
    {
        synchronized( s_lockObj ) {
            SuccessRecord record = recordFor( connType, false );
            record.update( success );
        }
        invalidateParent();
        saveState( context, handler );
        if ( success ) {
            showSuccess( handler, false );
        }
    }

    public static void showSuccessIn( Handler handler )
    {
        showSuccess( handler, true );
    }

    public static void showSuccessOut( Handler handler )
    {
        showSuccess( handler, false );
    }

    public static void draw( Context context, Canvas canvas, Resources res, 
                             int offsetX, int offsetY, CommsConnType connType )
    {
        synchronized( s_lockObj ) {
            if ( null != s_rect ) {
                int iconID;
                switch( connType ) {
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

                if ( CommsConnType.COMMS_CONN_NONE != connType ) {
                    int saveTop = rect.top;
                    SuccessRecord record;
                    boolean enabled = connTypeEnabled( context, connType );

                    // Do the background coloring
                    rect.bottom = rect.top + quarterHeight * 2;
                    record = recordFor( connType, false );
                    s_fillPaint.setColor( enabled && record.successNewer
                                          ? GREEN : RED );
                    canvas.drawRect( rect, s_fillPaint );
                    rect.top = rect.bottom;
                    rect.bottom = rect.top + quarterHeight * 2;
                    record = recordFor( connType, true );
                    s_fillPaint.setColor( enabled && record.successNewer
                                          ? GREEN : RED );
                    canvas.drawRect( rect, s_fillPaint );

                    // now the icons
                    rect.top = saveTop;
                    rect.bottom = rect.top + quarterHeight;
                    int arrowID = s_showSuccesses[SUCCESS_OUT]? 
                        R.drawable.out_arrow_active : R.drawable.out_arrow;
                    drawIn( canvas, res, arrowID, rect );

                    rect.top += 3 * quarterHeight;
                    rect.bottom = rect.top + quarterHeight;
                    arrowID = s_showSuccesses[SUCCESS_IN]? 
                        R.drawable.in_arrow_active : R.drawable.in_arrow;
                    drawIn( canvas, res, arrowID, rect );

                    rect.top = saveTop;
                }

                rect.top += quarterHeight;
                rect.bottom = rect.top + (2 * quarterHeight);
                drawIn( canvas, res, iconID, rect );
            }
        }
    }

    // This gets rid of lint warning, but I don't like it as it
    // effects the whole method.
    // @SuppressWarnings("unchecked")
    public static void loadState( Context context )
    {
        synchronized( s_lockObj ) {
            String as64 = XWPrefs.getPrefsString( context, 
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
                    DbgUtils.loge( ex );
                }
            }
        }
    }

    private static void saveState( final Context context, Handler handler )
    {
        if ( null == handler ) {
            doSave( context );
        } else {
            boolean savePending;
            synchronized( s_lockObj ) {
                savePending = s_needsSave;
                if ( !savePending ) {
                    s_needsSave = true;
                }
            }

            if ( !savePending ) {
                Runnable proc = new Runnable() {
                        public void run() {
                            doSave( context );
                        }
                    };
                handler.postDelayed( proc, 5000 );
            }
        }
    }

    private static void showSuccess( Handler handler, boolean isIn )
    {
        if ( null != handler ) {
            synchronized( s_lockObj ) {
                if ( isIn && s_showSuccesses[SUCCESS_IN] ) {
                    // do nothing
                } else if ( !isIn && s_showSuccesses[SUCCESS_OUT] ) {
                    // do nothing
                } else {
                    final int index = isIn? SUCCESS_IN : SUCCESS_OUT;
                    s_showSuccesses[index] = true;

                    Runnable proc = new Runnable() {
                            public void run() {
                                synchronized( s_lockObj ) {
                                    s_showSuccesses[index] = false;
                                    invalidateParent();
                                }
                            }
                        };
                    handler.postDelayed( proc, SHOW_SUCCESS_INTERVAL );
                    invalidateParent();
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

    private static int connType2StrID( CommsConnType connType )
    {
        int resID = 0;
        switch( connType ) {
        case COMMS_CONN_RELAY:
            resID = R.string.connstat_relay;
            break;
        case COMMS_CONN_SMS:
            resID = R.string.connstat_sms;
            break;
        case COMMS_CONN_BT:
            resID = R.string.connstat_bt;
            break;
        default:
            Assert.fail();
        }
        return resID;
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

    private static void doSave( Context context )
    {
        synchronized( s_lockObj ) {
            DbgUtils.logf( "ConnStatusHandler:doSave() doing save" );
            ByteArrayOutputStream bas
                = new ByteArrayOutputStream();
            try {
                ObjectOutputStream out
                    = new ObjectOutputStream( bas );
                out.writeObject(s_records);
                out.flush();
                String as64 = 
                    XwJNI.base64Encode( bas.toByteArray() );
                XWPrefs.setPrefsString( context, R.string.key_connstat_data, 
                                        as64 );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }
            s_needsSave = false;
        }
    }
    
    private static boolean connTypeEnabled( Context context,
                                            CommsConnType connType )
    {
        boolean result = true;
        switch( connType ) {
        case COMMS_CONN_SMS:
            result = XWApp.SMSSUPPORTED && XWPrefs.getSMSEnabled( context )
                && !getAirplaneModeOn( context );
            break;
        }
        return result;
    }

    private static boolean getAirplaneModeOn( Context context ) 
    {
        boolean result =
            0 != Settings.System.getInt( context.getContentResolver(),
                                         Settings.System.AIRPLANE_MODE_ON, 0 );
        return result;
    }

}

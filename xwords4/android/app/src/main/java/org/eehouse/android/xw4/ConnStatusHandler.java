/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.provider.Settings;
import android.text.format.DateUtils;
import android.text.format.Time;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class ConnStatusHandler {
    private static final String TAG = ConnStatusHandler.class.getSimpleName();

    public interface ConnStatusCBacks {
        public void invalidateParent();
        public void onStatusClicked();
        public Handler getHandler();
    }

    private static final int GREEN = 0xFF00AF00;
    private static final int RED = 0xFFAF0000;
    private static final int BLACK = 0xFF000000;
    private static final int SUCCESS_IN = 0;
    private static final int SUCCESS_OUT = 1;
    private static final int SHOW_SUCCESS_INTERVAL = 1000;
    private static final boolean SOLO_NOGREEN = true;

    private static Rect s_rect;
    private static boolean s_downOnMe = false;
    private static ConnStatusCBacks s_cbacks;
    private static Paint s_fillPaint = new Paint( Paint.ANTI_ALIAS_FLAG );
    private static boolean[] s_showSuccesses = { false, false };
    private static Time s_time = new Time();

    private static class SuccessRecord implements java.io.Serializable {
        public long lastSuccess;
        public long lastFailure;
        public boolean successNewer;

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
            s_time.set( successNewer? lastSuccess : lastFailure );
            return format( context, s_time );
        }

        public String olderStr( Context context )
        {
            s_time.set( successNewer? lastFailure : lastSuccess );
            return format( context, s_time );
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
                                                     DateUtils.SECOND_IN_MILLIS,
                                                     DateUtils.WEEK_IN_MILLIS,
                                                     0 );
            return seq.toString();
        }
    }

    private ConnStatusHandler() {}

    private static Map<CommsConnType,SuccessRecord[]> s_records =
        new HashMap<CommsConnType,SuccessRecord[]>();
    private static Class s_lockObj = ConnStatusHandler.class;
    private static boolean s_needsSave = false;

    public static void setRect( int left, int top, int right, int bottom )
    {
        s_rect = new Rect( left, top, right, bottom );
    }

    public static void clearRect()
    {
        s_rect = null;
    }

    public static void setHandler( ConnStatusCBacks cbacks )
    {
        s_cbacks = cbacks;
    }

    public static boolean handleDown( int xx, int yy )
    {
        s_downOnMe = null != s_rect && s_rect.contains( xx, yy );
        return s_downOnMe;
    }

    public static boolean handleUp( int xx, int yy )
    {
        boolean result = s_downOnMe && s_rect.contains( xx, yy );
        if ( result && null != s_cbacks ) {
            s_cbacks.onStatusClicked();
        }
        s_downOnMe = false;
        return result;
    }

    public static boolean handleMove( int xx, int yy )
    {
        return s_downOnMe && s_rect.contains( xx, yy );
    }

    public static String getStatusText( Context context, CommsConnTypeSet connTypes )
    {
        String msg;
        if ( null == connTypes || 0 == connTypes.size() ) {
            msg = null;
        } else {
            StringBuffer sb = new StringBuffer();
            String tmp;
            synchronized( s_lockObj ) {
                sb.append( LocUtils.getString( context,
                                               R.string.connstat_net_fmt,
                                               connTypes.toString( context, true )));
                for ( CommsConnType typ : connTypes.getTypes() ) {
                    String did = addDebugInfo( context, typ );
                    sb.append( String.format( "\n\n*** %s %s***\n",
                                              typ.longName( context ), did ) );
                    SuccessRecord record = recordFor( typ, false );
                    tmp = LocUtils.getString( context, record.successNewer?
                                              R.string.connstat_succ :
                                              R.string.connstat_unsucc );
                    sb.append( LocUtils
                               .getString( context, R.string.connstat_lastsend_fmt,
                                           tmp, record.newerStr( context ) ) );

                    int fmtId = 0;
                    if ( record.successNewer ) {
                        if ( record.haveFailure() ) {
                            fmtId = R.string.connstat_lastother_succ_fmt;
                        }
                    } else {
                        if ( record.haveSuccess() ) {
                            fmtId = R.string.connstat_lastother_unsucc_fmt;
                        }
                    }
                    if ( 0 != fmtId ) {
                        sb.append( LocUtils.getString( context, fmtId,
                                                       record.olderStr( context )));
                    }
                    sb.append( "\n\n" );

                    record = recordFor( typ, true );
                    if ( record.haveSuccess() ) {
                        sb.append( LocUtils.getString( context,
                                                       R.string.connstat_lastreceipt_fmt,
                                                       record.newerStr( context ) ) );
                    } else {
                        sb.append( LocUtils.getString( context, R.string.connstat_noreceipt) );
                    }
                }
            }
            msg = sb.toString();
        }
        return msg;
    }

    private static void invalidateParent()
    {
        if ( null != s_cbacks ) {
            s_cbacks.invalidateParent();
        }
    }

    public static void updateStatus( Context context, ConnStatusCBacks cbacks,
                                     CommsConnType connType, boolean success )
    {
        updateStatusImpl( context, cbacks, connType, success, true );
        updateStatusImpl( context, cbacks, connType, success, false );
    }

    public static void updateStatusIn( Context context, ConnStatusCBacks cbacks,
                                       CommsConnType connType, boolean success )
    {
        updateStatusImpl( context, cbacks, connType, success, true );
    }

    public static void updateStatusOut( Context context, ConnStatusCBacks cbacks,
                                        CommsConnType connType, boolean success )
    {
        updateStatusImpl( context, cbacks, connType, success, false );
    }

    private static void updateStatusImpl( Context context, ConnStatusCBacks cbacks,
                                          CommsConnType connType, boolean success,
                                          boolean isIn )
    {
        if ( null == cbacks ) {
            cbacks = s_cbacks;
        }

        synchronized( s_lockObj ) {
            SuccessRecord record = recordFor( connType, isIn );
            record.update( success );
        }
        invalidateParent();
        saveState( context, cbacks );
        if ( success ) {
            showSuccess( cbacks, isIn );
        }
    }

    public static void showSuccessIn( ConnStatusCBacks cbcks )
    {
        showSuccess( cbcks, true );
    }

    public static void showSuccessIn()
    {
        showSuccessIn( s_cbacks );
    }

    public static void showSuccessOut( ConnStatusCBacks cbcks )
    {
        showSuccess( cbcks, false );
    }

    public static void showSuccessOut()
    {
        showSuccessOut( s_cbacks );
    }

    public static void draw( Context context, Canvas canvas, Resources res,
                             CommsConnTypeSet connTypes, boolean isSolo )
    {
        if ( !isSolo && null != s_rect ) {
            synchronized( s_lockObj ) {
                Rect scratchR = new Rect( s_rect );
                int quarterHeight = scratchR.height() / 4;

                boolean enabled = anyTypeEnabled( context, connTypes );

                // Do the background coloring and arrow. Top half first
                scratchR.bottom -= (2 * quarterHeight);
                fillHalf( canvas, scratchR, connTypes, enabled, false );
                scratchR.bottom -= quarterHeight;
                drawArrow( canvas, res, scratchR, false );

                // bottom half and arrow
                scratchR.top = s_rect.top + (2 * quarterHeight);
                scratchR.bottom = s_rect.bottom;
                fillHalf( canvas, scratchR, connTypes, enabled, true );
                scratchR.top += quarterHeight;
                drawArrow( canvas, res, scratchR, true );

                // Center the icon in the remaining (vertically middle) rect
                scratchR.top = s_rect.top + quarterHeight;
                scratchR.bottom = s_rect.bottom - quarterHeight;
                int minDim = Math.min( scratchR.width(), scratchR.height() );
                int dx = (scratchR.width() - minDim) / 2;
                int dy = (scratchR.height() - minDim) / 2;
                scratchR.inset( dx, dy );
                Assert.assertTrue( !BuildConfig.DEBUG
                                   || 1 >= Math.abs(scratchR.width()
                                                    - scratchR.height()) );
                drawIn( canvas, res, R.drawable.multigame__gen, scratchR );
            }
        }
    }

    private static void fillHalf( Canvas canvas, Rect rect,
                                  CommsConnTypeSet connTypes, boolean enabled,
                                  boolean isIn )
    {
        enabled = enabled && null != newestSuccess( connTypes, isIn );
        s_fillPaint.setColor( enabled ? GREEN : RED );
        canvas.drawRect( rect, s_fillPaint );
    }

    private static void drawArrow( Canvas canvas, Resources res, Rect rect,
                                   boolean isIn )
    {
        int arrowID;
        boolean showSuccesses = s_showSuccesses[isIn? SUCCESS_IN : SUCCESS_OUT];
        if ( isIn ) {
            arrowID = showSuccesses ?
                R.drawable.in_arrow_active : R.drawable.in_arrow;
        } else {
            arrowID = showSuccesses ?
                R.drawable.out_arrow_active : R.drawable.out_arrow;
        }
        drawIn( canvas, res, arrowID, rect );
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
                try {
                    byte[] bytes = Utils.base64Decode( as64 );
                    ObjectInputStream ois =
                        new ObjectInputStream( new ByteArrayInputStream(bytes) );
                    s_records =
                        (HashMap<CommsConnType,SuccessRecord[]>)ois.readObject();
                } catch ( Exception ex ) {
                    Log.ex( TAG, ex );
                    s_records = new HashMap<CommsConnType,SuccessRecord[]>();
                }
            }
        }
    }

    private static void saveState( final Context context,
                                   ConnStatusCBacks cbcks )
    {
        if ( null == cbcks ) {
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
                Handler handler = cbcks.getHandler();
                if ( null != handler ) {
                    Runnable proc = new Runnable() {
                            public void run() {
                                doSave( context );
                            }
                        };
                    handler.postDelayed( proc, 5000 );
                }
            }
        }
    }

    private static void showSuccess( ConnStatusCBacks cbcks, boolean isIn )
    {
        if ( null != cbcks ) {
            synchronized( s_lockObj ) {
                if ( isIn && s_showSuccesses[SUCCESS_IN] ) {
                    // do nothing
                } else if ( !isIn && s_showSuccesses[SUCCESS_OUT] ) {
                    // do nothing
                } else {
                    Handler handler = cbcks.getHandler();
                    if ( null != handler ) {
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
    }

    private static void drawIn( Canvas canvas, Resources res, int id, Rect rect )
    {
        Drawable icon = res.getDrawable( id );
        Assert.assertTrue( icon.getBounds().width() == icon.getBounds().height() );
        icon.setBounds( rect );
        icon.draw( canvas );
    }

    private static SuccessRecord newestSuccess( CommsConnTypeSet connTypes,
                                                boolean isIn )
    {
        SuccessRecord result = null;
        if ( null != connTypes ) {
            Iterator<CommsConnType> iter = connTypes.iterator();
            while ( iter.hasNext() ) {
                CommsConnType connType = iter.next();
                SuccessRecord record = recordFor( connType, isIn );
                if ( record.successNewer ) {
                    if ( null == result || result.lastSuccess < record.lastSuccess ) {
                        result = record;
                    }
                }
            }
        }
        return result;
    }

    private static SuccessRecord recordFor( CommsConnType connType, boolean isIn )
    {
        SuccessRecord[] records = s_records.get( connType );
        if ( null == records ) {
            records = new SuccessRecord[] { new SuccessRecord(),
                                            new SuccessRecord(),
            };
            s_records.put( connType, records );
        }
        return records[isIn?0:1];
    }

    private static void doSave( Context context )
    {
        synchronized( s_lockObj ) {
            // DbgUtils.logf( "ConnStatusHandler:doSave() doing save" );
            ByteArrayOutputStream bas
                = new ByteArrayOutputStream();
            try {
                ObjectOutputStream out
                    = new ObjectOutputStream( bas );
                out.writeObject( s_records );
                out.flush();
                String as64 = Utils.base64Encode( bas.toByteArray() );
                XWPrefs.setPrefsString( context, R.string.key_connstat_data,
                                        as64 );
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
            }
            s_needsSave = false;
        }
    }

    private static boolean anyTypeEnabled( Context context, CommsConnTypeSet connTypes )
    {
        boolean enabled = false;
        Iterator<CommsConnType> iter = connTypes.iterator();
        while ( !enabled && iter.hasNext() ) {
            enabled = connTypeEnabled( context, iter.next() );
        }
        return enabled;
    }

    private static boolean connTypeEnabled( Context context,
                                            CommsConnType connType )
    {
        boolean result = true;
        switch( connType ) {
        case COMMS_CONN_SMS:
            result = XWPrefs.getSMSEnabled( context )
                && !getAirplaneModeOn( context );
            break;
        case COMMS_CONN_BT:
            result = XWApp.BTSUPPORTED && BTService.BTEnabled()
                && BTService.BTEnabled();
            // No: we can be in airplane mode but with BT turned on manually.
            //!getAirplaneModeOn( context );
            break;
        case COMMS_CONN_RELAY:
            result = RelayService.relayEnabled( context )
                && NetStateCache.netAvail( context );
            break;
        case COMMS_CONN_P2P:
            result = WiDirService.connecting();
            break;
        default:
            Log.w( TAG, "connTypeEnabled: %s not handled", connType.toString() );
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

    private static String addDebugInfo( Context context, CommsConnType typ )
    {
        String result = "";
        if ( BuildConfig.DEBUG ) {
            switch ( typ ) {
            case COMMS_CONN_RELAY:
                result = String.format( "(DevID: %d; host: %s) ",
                                        DevID.getRelayDevIDInt(context),
                                        XWPrefs.getDefaultRelayHost(context) );
                break;
            case COMMS_CONN_P2P:
                result = WiDirService.formatNetStateInfo();
                break;
            default:
                break;
            }
        }
        return result;
    }
}

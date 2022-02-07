/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2021 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.os.Environment;
import android.os.Process;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.lang.ref.WeakReference;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Formatter;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.zip.GZIPOutputStream;

public class Log {
    private static final String TAG = Log.class.getSimpleName();
    private static final String PRE_TAG = BuildConfig.FLAVOR + "-";
    private static final String KEY_USE_DB = TAG + "/useDB";
    private static final boolean LOGGING_ENABLED = BuildConfig.NON_RELEASE;
    private static final boolean ERROR_LOGGING_ENABLED = true;
    private static final String LOGS_FILE_NAME_FMT = BuildConfig.FLAVOR + "_logsDB_%d.txt.gz";
    private static final String LOGS_DB_NAME = "xwlogs_db";
    private static final String LOGS_TABLE_NAME = "logs";
    private static final String COL_ENTRY = "entry";
    private static final String COL_THREAD = "tid";
    private static final String COL_PID = "pid";
    private static final String COL_ROWID = "rowid";
    private static final String COL_TAG = "tag";
    private static final String COL_LEVEL = "level";
    private static final String COL_TIMESTAMP = "ts";

    private static final int DB_VERSION = 2;
    private static boolean sEnabled = BuildConfig.NON_RELEASE;
    private static boolean sUseDB;
    private static WeakReference<Context> sContextRef;

    private static enum LOG_LEVEL {
        INFO,
        ERROR,
        WARN,
        DEBUG,
    }

    public static void init( Context context )
    {
        sContextRef = new WeakReference<>( context );
        sUseDB = DBUtils.getBoolFor( context, KEY_USE_DB, false );
    }

    public static void setStoreLogs( boolean enable )
    {
        Context context = sContextRef.get();
        Assert.assertTrueNR( null != context );
        if ( null != context ) {
            DBUtils.setBoolFor( context, KEY_USE_DB, enable );
        }
        sUseDB = enable;
    }

    public static boolean getStoreLogs()
    {
        return sUseDB;
    }

    public static void enable( boolean newVal )
    {
        sEnabled = newVal;
    }

    interface ResultProcs {
        void onDumping( int nRecords );
        void onDumped( File db );
        void onCleared( int nCleared );
    }

    public static void clearStored( ResultProcs procs )
    {
        int result = 0;
        LogDBHelper helper = initDB();
        if ( null != helper ) {
            helper.clear( procs );
        }
    }

    public static void dumpStored( ResultProcs procs )
    {
        File result = null;
        LogDBHelper helper = initDB();
        if ( null != helper ) {
            helper.dumpToFile( procs );
        }
    }

    public static void enable( Context context )
    {
        boolean on = LOGGING_ENABLED ||
            XWPrefs.getPrefsBoolean( context, R.string.key_logging_on,
                                     LOGGING_ENABLED );
        enable( on );
    }

    public static void d( String tag, String fmt, Object... args )
    {
        if ( sEnabled ) {
            dolog( LOG_LEVEL.DEBUG, tag, fmt, args );
        }
    }

    public static void w( String tag, String fmt, Object... args )
    {
        if ( sEnabled ) {
            dolog( LOG_LEVEL.WARN, tag, fmt, args );
        }
    }

    public static void e( String tag, String fmt, Object... args )
    {
        if ( ERROR_LOGGING_ENABLED ) {
            dolog( LOG_LEVEL.ERROR, tag, fmt, args );
        }
    }

    public static void i( String tag, String fmt, Object... args )
    {
        if ( sEnabled ) {
            dolog( LOG_LEVEL.INFO, tag, fmt, args );
        }
    }

    private static void dolog( LOG_LEVEL level, String tag, String fmt, Object[] args )
    {
        String str = new Formatter().format( fmt, args ).toString();
        String fullTag = PRE_TAG + tag;
        switch ( level ) {
        case DEBUG:
            android.util.Log.d( fullTag, str );
            break;
        case ERROR:
            android.util.Log.e( fullTag, str );
            break;
        case WARN:
            android.util.Log.w( fullTag, str );
            break;
        case INFO:
            android.util.Log.e( fullTag, str );
            break;
        default:
            Assert.failDbg();
        }
        store( level, fullTag, str );
    }

    public static void ex( String tag, Exception exception )
    {
        if ( sEnabled ) {
            w( tag, "Exception: %s", exception.toString() );
            DbgUtils.printStack( tag, exception.getStackTrace() );
        }
    }

    private static void llog( String fmt, Object... args )
    {
        String str = new Formatter().format( fmt, args ).toString();
        android.util.Log.d( TAG, str );
    }

    private static LogDBHelper s_dbHelper;
    private synchronized static LogDBHelper initDB()
    {
        if ( null == s_dbHelper ) {
            Context context = sContextRef.get();
            if ( null != context ) {
                s_dbHelper = new LogDBHelper( context );
                // force any upgrade
                s_dbHelper.getWritableDatabase().close();
            }
        }
        return s_dbHelper;
    }

    // Called from jni. Keep name and signature in sync with what's in
    // passToJava() in andutils.c
    public static void store( String tag, String msg )
    {
        store( LOG_LEVEL.DEBUG, tag, msg );
    }

    private static void store( LOG_LEVEL level, String tag, String msg )
    {
        if ( sUseDB ) {
            LogDBHelper helper = initDB();
            if ( null != helper ) {
                helper.store( level, tag, msg );
            }
        }
    }

    private static class LogDBHelper extends SQLiteOpenHelper {
        private Context mContext;

        LogDBHelper( Context context )
        {
            super( context, LOGS_DB_NAME, null, DB_VERSION );
            mContext = context;
        }

        @Override
        public void onCreate( SQLiteDatabase db )
        {
            String query = "CREATE TABLE " + LOGS_TABLE_NAME + "("
                + COL_ROWID + " INTEGER PRIMARY KEY AUTOINCREMENT"
                + "," + COL_ENTRY + " TEXT"
                + "," + COL_THREAD + " INTEGER"
                + "," + COL_PID + " INTEGER"
                + "," + COL_TAG + " TEXT"
                + "," + COL_LEVEL + " INTEGER(2)"
                + "," + COL_TIMESTAMP + " INTEGER"
                + ");";

            db.execSQL( query );
        }

        @Override
        @SuppressWarnings("fallthrough")
        public void onUpgrade( SQLiteDatabase db, int oldVersion, int newVersion )
        {
            String msg = String.format("onUpgrade(%s): old: %d; new: %d", db,
                                       oldVersion, newVersion );
            android.util.Log.i( TAG, msg );
            switch ( oldVersion ) {
            case 1:
                addColumn( db, COL_TIMESTAMP, "INTEGER DEFAULT 0" );
                break;
            default:
                Assert.failDbg();
            }
        }

        void store( LOG_LEVEL level, String tag, String msg )
        {
            int tid = Process.myTid();
            int pid = Process.myPid();

            final ContentValues values = new ContentValues();
            values.put( COL_ENTRY, msg );
            values.put( COL_THREAD, tid );
            values.put( COL_PID, pid );
            values.put( COL_TAG, tag );
            values.put( COL_LEVEL, level.ordinal() );
            values.put( COL_TIMESTAMP, System.currentTimeMillis() );
            enqueue( new Runnable() {
                    @Override
                    public void run() {
                        getWritableDatabase().insert( LOGS_TABLE_NAME, null, values );
                    }
                } );
        }

        void dumpToFile(final ResultProcs procs)
        {
            enqueue( new Runnable() {
                    @Override
                    public void run() {
                        SimpleDateFormat formatter = new SimpleDateFormat("yy/MM/dd HH:mm:ss.SSS");

                        File dir = Environment.getExternalStorageDirectory();
                        dir = new File( dir, Environment.DIRECTORY_DOWNLOADS );
                        File db;
                        for ( int ii = 1; ; ++ii ) {
                            db = new File( dir, String.format( LOGS_FILE_NAME_FMT, ii ) );
                            if ( !db.exists() ) {
                                break;
                            }
                        }

                        try {
                            FileOutputStream fos = new FileOutputStream( db );
                            GZIPOutputStream gzos = new GZIPOutputStream( fos );
                            OutputStreamWriter osw = new OutputStreamWriter( gzos );

                            String[] columns = { COL_ENTRY, COL_TAG, COL_THREAD, COL_PID,
                                                 COL_TIMESTAMP };
                            String selection = null;
                            String orderBy = COL_ROWID;
                            Cursor cursor = getReadableDatabase().query( LOGS_TABLE_NAME, columns,
                                                                         selection, null, null,
                                                                         null, orderBy );
                            procs.onDumping( cursor.getCount() );

                            llog( "dumpToFile(): db=%s; got %d results", db, cursor.getCount() );
                            int[] indices = new int[columns.length];
                            for ( int ii = 0; ii < indices.length; ++ii ) {
                                indices[ii] = cursor.getColumnIndex( columns[ii] );
                            }
                            while ( cursor.moveToNext() ) {
                                String data = cursor.getString(indices[0]);
                                String tag = cursor.getString(indices[1]);
                                int tid = cursor.getInt(indices[2]);
                                int pid = cursor.getInt(indices[3]);
                                long ts = cursor.getLong(indices[4]);
                                StringBuilder builder = new StringBuilder()
                                    .append(formatter.format( new Date(ts) ))
                                    .append(String.format(" % 5d % 5d", pid, tid)).append(":")
                                    .append(tag).append(":")
                                    .append(data).append("\n")
                                    ;
                                osw.write( builder.toString() );
                            }
                            osw.close();
                        } catch ( IOException ioe ) {
                            llog( "dumpToFile(): ioe: %s", ioe );
                            db = null;
                        }
                        if ( null != db ) {
                            procs.onDumped(db);
                        }
                    }
                } );
        }

        // Return the number of rows
        void clear( final ResultProcs procs )
        {
            enqueue( new Runnable() {
                    @Override
                    public void run() {
                        int result = getWritableDatabase()
                            .delete( LOGS_TABLE_NAME, "1", null );
                        procs.onCleared( result );
                    }
                } );
        }

        private void addColumn( SQLiteDatabase db, String colName, String colType )
        {
            String cmd = String.format( "ALTER TABLE %s ADD COLUMN %s %s;",
                                        LOGS_TABLE_NAME, colName, colType );
            db.execSQL( cmd );
        }

        private LinkedBlockingQueue<Runnable> mQueue;
        private void enqueue( Runnable runnable )
        {
            if ( null == mQueue ) {
                mQueue = new LinkedBlockingQueue<>();
                new Thread( new Runnable() {
                        @Override
                        public void run() {
                            for ( ; ; ) {
                                try {
                                    mQueue.take().run();
                                } catch ( InterruptedException ie ) {
                                    break;
                                }
                            }
                        }
                    }).start();
            }

            mQueue.add( runnable );
        }
    }
}

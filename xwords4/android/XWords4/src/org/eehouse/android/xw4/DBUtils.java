/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.net.Uri;
import android.os.Environment;
import android.text.TextUtils;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.StringTokenizer;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;


public class DBUtils {
    public static final int ROWID_NOTFOUND = -1;

    private static final String DICTS_SEP = ",";

    private static final String ROW_ID = "rowid";
    private static final String ROW_ID_FMT = "rowid=%d";

    private static long s_cachedRowID = -1;
    private static byte[] s_cachedBytes = null;

    public static interface DBChangeListener {
        public void gameSaved( long rowid );
    }
    private static HashSet<DBChangeListener> s_listeners = 
        new HashSet<DBChangeListener>();

    private static SQLiteOpenHelper s_dbHelper = null;

    public static class Obit {
        public Obit( String relayID, int seed ) {
            m_relayID = relayID; m_seed = seed;
        }
        String m_relayID;
        int m_seed;
    }

    public static class HistoryPair {
        private HistoryPair( String p_msg, boolean p_sourceLocal )
        {
            msg = p_msg;
            sourceLocal = p_sourceLocal;
        }
        String msg;
        boolean sourceLocal;
    }

    public static GameSummary getSummary( Context context, long rowid, 
                                          long maxMillis )
    {
        GameSummary result = null;
        GameUtils.GameLock lock = 
            new GameUtils.GameLock( rowid, false ).lock( maxMillis );
        if ( null != lock ) {
            result = getSummary( context, lock );
            lock.unlock();
        }
        return result;
    }

    public static GameSummary getSummary( Context context, long rowid )
    {
        return getSummary( context, rowid, 0L );
    }

    public static GameSummary getSummary( Context context, 
                                          GameUtils.GameLock lock )
    {
        initDB( context );
        GameSummary summary = null;

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { ROW_ID,
                                 DBHelper.NUM_MOVES, DBHelper.NUM_PLAYERS,
                                 DBHelper.MISSINGPLYRS,
                                 DBHelper.GAME_OVER, DBHelper.PLAYERS,
                                 DBHelper.TURN, DBHelper.GIFLAGS,
                                 DBHelper.CONTYPE, DBHelper.SERVERROLE,
                                 DBHelper.ROOMNAME, DBHelper.RELAYID, 
                                 DBHelper.SMSPHONE, DBHelper.SEED, 
                                 DBHelper.DICTLANG, DBHelper.GAMEID,
                                 DBHelper.SCORES, DBHelper.HASMSGS,
                                 DBHelper.LASTPLAY_TIME, DBHelper.REMOTEDEVS,
            };
            String selection = String.format( ROW_ID_FMT, lock.getRowid() );

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                summary = new GameSummary( context );
                summary.nMoves = cursor.getInt(cursor.
                                               getColumnIndex(DBHelper.NUM_MOVES));
                summary.nPlayers = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.NUM_PLAYERS));
                summary.missingPlayers = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.MISSINGPLYRS));
                summary.
                    setPlayerSummary( cursor.
                                      getString( cursor.
                                                 getColumnIndex( DBHelper.
                                                                 PLAYERS ) ) );
                summary.turn = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.TURN));
                summary.
                    setGiFlags( cursor.getInt(cursor.
                                              getColumnIndex(DBHelper.GIFLAGS))
                                );
                summary.gameID = 
                    cursor.getInt(cursor.getColumnIndex(DBHelper.GAMEID) ); 

                String players = cursor.
                    getString(cursor.getColumnIndex( DBHelper.PLAYERS ));
                summary.readPlayers( players );

                summary.dictLang = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.DICTLANG));
                summary.modtime = 
                    cursor.getLong(cursor.
                                   getColumnIndex(DBHelper.LASTPLAY_TIME));
                int tmp = cursor.getInt(cursor.
                                        getColumnIndex(DBHelper.GAME_OVER));
                summary.gameOver = tmp != 0;

                String scoresStr = 
                    cursor.getString( cursor.getColumnIndex(DBHelper.SCORES));
                int[] scores = new int[summary.nPlayers];
                if ( null != scoresStr && scoresStr.length() > 0 ) {
                    StringTokenizer st = new StringTokenizer( scoresStr );
                    for ( int ii = 0; ii < scores.length; ++ii ) {
                        Assert.assertTrue( st.hasMoreTokens() );
                        String token = st.nextToken();
                        scores[ii] = Integer.parseInt( token );
                    }
                } else {
                    for ( int ii = 0; ii < scores.length; ++ii ) {
                        scores[ii] = 0;
                    }
                }
                summary.scores = scores;

                int col = cursor.getColumnIndex( DBHelper.CONTYPE );
                if ( col >= 0 ) {
                    tmp = cursor.getInt( col );
                    summary.conType = CommsAddrRec.CommsConnType.values()[tmp];
                    col = cursor.getColumnIndex( DBHelper.SEED );
                    if ( col >= 0 ) {
                        summary.seed = cursor.getInt( col );
                    }
                    switch ( summary.conType ) {
                    case COMMS_CONN_RELAY:
                        col = cursor.getColumnIndex( DBHelper.ROOMNAME );
                        if ( col >= 0 ) {
                            summary.roomName = cursor.getString( col );
                        }
                        col = cursor.getColumnIndex( DBHelper.RELAYID );
                        if ( col >= 0 ) {
                            summary.relayID = cursor.getString( col );
                        }
                        break;
                    case COMMS_CONN_BT:
                    case COMMS_CONN_SMS:
                        col = cursor.getColumnIndex( DBHelper.REMOTEDEVS );
                        if ( col >= 0 ) {
                            summary.setRemoteDevs( context, 
                                                   cursor.getString( col ) );
                        }
                        break;
                    }
                }

                col = cursor.getColumnIndex( DBHelper.SERVERROLE );
                tmp = cursor.getInt( col );
                summary.serverRole = CurGameInfo.DeviceRole.values()[tmp];

                col = cursor.getColumnIndex( DBHelper.HASMSGS );
                if ( col >= 0 ) {
                    summary.pendingMsgLevel = cursor.getInt( col );
                }
            }
            cursor.close();
            db.close();
        }

        if ( null == summary ) {
            summary = GameUtils.summarize( context, lock );
            saveSummary( context, lock, summary );
        }
        return summary;
    } // getSummary

    public static void saveSummary( Context context, GameUtils.GameLock lock,
                                    GameSummary summary )
    {
        saveSummary( context, lock, summary, null );
    }

    public static void saveSummary( Context context, GameUtils.GameLock lock,
                                    GameSummary summary, String inviteID )
    {
        Assert.assertTrue( lock.canWrite() );
        long rowid = lock.getRowid();
        String selection = String.format( ROW_ID_FMT, rowid );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            if ( null == summary ) {
                db.delete( DBHelper.TABLE_NAME_SUM, selection, null );
            } else {
                ContentValues values = new ContentValues();
                values.put( DBHelper.NUM_MOVES, summary.nMoves );
                values.put( DBHelper.NUM_PLAYERS, summary.nPlayers );
                values.put( DBHelper.MISSINGPLYRS, summary.missingPlayers );
                values.put( DBHelper.TURN, summary.turn );
                values.put( DBHelper.GIFLAGS, summary.giflags() );
                values.put( DBHelper.PLAYERS, 
                            summary.summarizePlayers() );
                values.put( DBHelper.DICTLANG, summary.dictLang );
                values.put( DBHelper.GAMEID, summary.gameID );
                values.put( DBHelper.GAME_OVER, summary.gameOver? 1 : 0 );
                values.put( DBHelper.DICTLIST, summary.dictNames(DICTS_SEP) );
                values.put( DBHelper.HASMSGS, summary.pendingMsgLevel );
                if ( null != inviteID ) {
                    values.put( DBHelper.INVITEID, inviteID );
                }

                if ( null != summary.scores ) {
                    StringBuffer sb = new StringBuffer();
                    for ( int score : summary.scores ) {
                        sb.append( String.format( "%d ", score ) );
                    }
                    values.put( DBHelper.SCORES, sb.toString() );
                }

                if ( null != summary.conType ) {
                    values.put( DBHelper.CONTYPE, summary.conType.ordinal() );
                    values.put( DBHelper.SEED, summary.seed );
                    switch( summary.conType ) {
                    case COMMS_CONN_RELAY:
                        values.put( DBHelper.ROOMNAME, summary.roomName );
                        values.put( DBHelper.RELAYID, summary.relayID );
                        break;
                    case COMMS_CONN_BT:
                    case COMMS_CONN_SMS:
                        values.put( DBHelper.REMOTEDEVS, 
                                    summary.summarizeDevs() );
                        break;
                    }
                }
                values.put( DBHelper.SERVERROLE, summary.serverRole.ordinal() );

                long result = db.update( DBHelper.TABLE_NAME_SUM,
                                         values, selection, null );
                Assert.assertTrue( result >= 0 );
            }
            notifyListeners( rowid );
            db.close();
        }
    } // saveSummary

    public static int countGamesUsingLang( Context context, int lang )
    {
        int result = 0;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String selection = String.format( "%s = %d", DBHelper.DICTLANG,
                                              lang );
            // null for columns will return whole rows: bad
            String[] columns = { DBHelper.DICTLANG };
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );

            result = cursor.getCount();
            cursor.close();
            db.close();
        }
        return result;
    }

    public static int countGamesUsingDict( Context context, String dict )
    {
        int result = 0;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String pattern = String.format( "%%%s%s%s%%", 
                                            DICTS_SEP, dict, DICTS_SEP );
            String selection = String.format( "%s LIKE '%s'", 
                                              DBHelper.DICTLIST, pattern );
            // null for columns will return whole rows: bad.  But
            // might as well make it an int for speed
            String[] columns = { DBHelper.DICTLANG }; 
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            result = cursor.getCount();
            cursor.close();
            db.close();
        }
        return result;
    }

    private static void setInt( long rowid, String column, int value )
    {
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = String.format( ROW_ID_FMT, rowid );
            ContentValues values = new ContentValues();
            values.put( column, value );

            int result = db.update( DBHelper.TABLE_NAME_SUM, 
                                    values, selection, null );
            Assert.assertTrue( result == 1 );
            db.close();
        }
    }

    public static void setMsgFlags( long rowid, int flags )
    {
        setInt( rowid, DBHelper.HASMSGS, flags );
        notifyListeners( rowid );
    }

    public static void setExpanded( long rowid, boolean expanded )
    {
        setInt( rowid, DBHelper.CONTRACTED, expanded?0:1 );
    }

    private static int getInt( Context context, long rowid, String column,
                               int dflt )
    {
        int result = dflt;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String selection = String.format( ROW_ID_FMT, rowid );
            String[] columns = { column };
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result =
                    cursor.getInt( cursor.getColumnIndex(column));
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static int getMsgFlags( Context context, long rowid )
    {
        return getInt( context, rowid, DBHelper.HASMSGS, 
                       GameSummary.MSG_FLAGS_NONE );
    }

    public static boolean getExpanded( Context context, long rowid )
    {
        return 0 == getInt( context, rowid, DBHelper.CONTRACTED, 0 );
    }

    public static String getRelayID( Context context, long rowid )
    {
        String result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.RELAYID };
            String selection = String.format( ROW_ID_FMT, rowid );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = 
                    cursor.getString( cursor.getColumnIndex(DBHelper.RELAYID) );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static long[] getRowIDsFor( Context context, String relayID )
    {
        long[] result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { ROW_ID };
            String selection = DBHelper.RELAYID + "='" + relayID + "'";
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                if ( null == result ) {
                    result = new long[cursor.getCount()];
                }
                result[ii] = cursor.getLong( cursor.getColumnIndex(ROW_ID) );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static long[] getRowIDsFor( Context context, int gameID )
    {
        long[] result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { ROW_ID };
            String selection = String.format( DBHelper.GAMEID + "=%d", gameID );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );

            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                if ( null == result ) {
                    result = new long[cursor.getCount()];
                }
                result[ii] = cursor.getLong( cursor.getColumnIndex(ROW_ID) );
            }
            cursor.close();
            db.close();
        }
        if ( null != result && 1 < result.length ) {
            DbgUtils.logf( "getRowIDsFor(%x)=>length %d array", gameID,
                           result.length );
        }
        return result;
    }

    public static void listBTGames( Context context, 
                                    HashMap<String, int[]> result )
    {
        HashSet<Integer> set;
        String[] columns = { DBHelper.GAMEID, DBHelper.REMOTEDEVS };
        String selection = DBHelper.GAMEID + "!=0";
        HashMap<String, HashSet<Integer> > map = 
            new HashMap<String, HashSet<Integer> >();
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            while ( cursor.moveToNext() ) {
                int col = cursor.getColumnIndex( DBHelper.GAMEID );
                int gameID = cursor.getInt( col );
                col = cursor.getColumnIndex( DBHelper.REMOTEDEVS );
                String devs = cursor.getString( col );
                DbgUtils.logf( "gameid %d has remote[s] %s", gameID, devs );

                if ( null != devs && 0 < devs.length() ) {
                    for ( String dev : TextUtils.split( devs, "\n" ) ) {
                        set = map.get( dev );
                        if ( null == set ) {
                            set = new HashSet<Integer>();
                            map.put( dev, set );
                        }
                        set.add( new Integer(gameID) );
                    }
                }
            }
            cursor.close();
            db.close();
        }

        Set<String> devs = map.keySet();
        Iterator<String> iter = devs.iterator();
        while ( iter.hasNext() ) {
            String dev = iter.next();
            set = map.get( dev );
            int[] gameIDs = new int[set.size()];
            Iterator<Integer> idIter = set.iterator();
            for ( int ii = 0; idIter.hasNext(); ++ii ) {
                gameIDs[ii] = idIter.next();
            }
            result.put( dev, gameIDs );
        }
    }

    public static long getRowIDForOpen( Context context, NetLaunchInfo nli )
    {
        long result = ROWID_NOTFOUND;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { ROW_ID };
            String selection = DBHelper.ROOMNAME + "='" + nli.room + "' AND "
                + DBHelper.INVITEID + "='" + nli.inviteID + "' AND "
                + DBHelper.DICTLANG + "=" + nli.lang + " AND "
                + DBHelper.NUM_PLAYERS + "=" + nli.nPlayers;
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getLong( cursor.getColumnIndex(ROW_ID) );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static boolean isNewInvite( Context context, Uri data )
    {
        NetLaunchInfo nli = new NetLaunchInfo( data );
        return null != nli && -1 == getRowIDForOpen( context, nli );
    }

    public static String[] getRelayIDs( Context context, boolean noMsgs ) 
    {
        String[] result = null;
        initDB( context );
        ArrayList<String> ids = new ArrayList<String>();

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.RELAYID };
            String selection = DBHelper.RELAYID + " NOT null";
            if ( noMsgs ) {
                selection += " AND NOT " + DBHelper.HASMSGS;
            }

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );

            while ( cursor.moveToNext() ) {
                ids.add( cursor.getString( cursor.
                                           getColumnIndex(DBHelper.RELAYID)) );
            }
            cursor.close();
            db.close();
        }

        if ( 0 < ids.size() ) {
            result = ids.toArray( new String[ids.size()] );
        }
        return result;
    }

    public static void addDeceased( Context context, String relayID, 
                                    int seed )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            ContentValues values = new ContentValues();
            values.put( DBHelper.RELAYID, relayID );
            values.put( DBHelper.SEED, seed );

            try {
                long result = db.replaceOrThrow( DBHelper.TABLE_NAME_OBITS,
                                                 "", values );
            } catch ( Exception ex ) {
                DbgUtils.loge( ex );
            }
            db.close();
        }
    }

    public static Obit[] listObits( Context context )
    {
        Obit[] result = null;
        ArrayList<Obit> al = new ArrayList<Obit>();

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.RELAYID, DBHelper.SEED };
            Cursor cursor = db.query( DBHelper.TABLE_NAME_OBITS, columns, 
                                      null, null, null, null, null );
            if ( 0 < cursor.getCount() ) {
                int idIndex = cursor.getColumnIndex( DBHelper.RELAYID );
                int seedIndex = cursor.getColumnIndex( DBHelper.SEED );
                while ( cursor.moveToNext() ) {
                    String relayID = cursor.getString( idIndex );
                    int seed = cursor.getInt( seedIndex );
                    al.add( new Obit( relayID, seed ) );
                }
            }
            cursor.close();
            db.close();
        }

        int siz = al.size();
        if ( siz > 0 ) {
            result = al.toArray( new Obit[siz] );
        }
        return result;
    }

    public static void clearObits( Context context, Obit[] obits )
    {
        String fmt = DBHelper.RELAYID + "= \"%s\" AND + " 
            + DBHelper.SEED + " = %d";

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            for ( Obit obit: obits ) {
                String selection = String.format( fmt, obit.m_relayID, 
                                                  obit.m_seed );
                db.delete( DBHelper.TABLE_NAME_OBITS, selection, null );
            }
            db.close();
        }
    }

    public static GameUtils.GameLock saveNewGame( Context context, byte[] bytes )
    {
        GameUtils.GameLock lock = null;

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            ContentValues values = new ContentValues();
            values.put( DBHelper.SNAPSHOT, bytes );

            long timestamp = new Date().getTime();
            values.put( DBHelper.CREATE_TIME, timestamp );
            values.put( DBHelper.LASTPLAY_TIME, timestamp );

            long rowid = db.insert( DBHelper.TABLE_NAME_SUM, null, values );

            setCached( rowid, null ); // force reread

            lock = new GameUtils.GameLock( rowid, true ).lock();

            notifyListeners( rowid );
        }

        return lock;
    }

    public static long saveGame( Context context, GameUtils.GameLock lock, 
                                 byte[] bytes, boolean setCreate )
    {
        Assert.assertTrue( lock.canWrite() );
        long rowid = lock.getRowid();
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = String.format( ROW_ID_FMT, rowid );
            ContentValues values = new ContentValues();
            values.put( DBHelper.SNAPSHOT, bytes );

            long timestamp = new Date().getTime();
            if ( setCreate ) {
                values.put( DBHelper.CREATE_TIME, timestamp );
            }
            values.put( DBHelper.LASTPLAY_TIME, timestamp );

            int result = db.update( DBHelper.TABLE_NAME_SUM, 
                                    values, selection, null );
            if ( 0 == result ) {
                Assert.fail();
                // values.put( DBHelper.FILE_NAME, path );
                // rowid = db.insert( DBHelper.TABLE_NAME_SUM, null, values );
                // DbgUtils.logf( "insert=>%d", rowid );
                // Assert.assertTrue( row >= 0 );
            }
            db.close();
        }
        setCached( rowid, null ); // force reread

        if ( -1 != rowid ) {
            notifyListeners( rowid );
        }
        return rowid;
    }

    public static byte[] loadGame( Context context, GameUtils.GameLock lock )
    {
        long rowid = lock.getRowid();
        Assert.assertTrue( -1 != rowid );
        byte[] result = getCached( rowid );
        if ( null == result ) {
            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getReadableDatabase();

                String[] columns = { DBHelper.SNAPSHOT };
                String selection = String.format( ROW_ID_FMT, rowid );
                Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                          selection, null, null, null, null );
                if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                    result = cursor.getBlob( cursor
                                             .getColumnIndex(DBHelper.SNAPSHOT));
                }
                cursor.close();
                db.close();
            }
            setCached( rowid, result );
        }
        return result;
    }

    public static void deleteGame( Context context, long rowid )
    {
        GameUtils.GameLock lock = new GameUtils.GameLock( rowid, true ).lock();
        deleteGame( context, lock );
        lock.unlock();
    }

    public static void deleteGame( Context context, GameUtils.GameLock lock )
    {
        Assert.assertTrue( lock.canWrite() );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = String.format( ROW_ID_FMT, lock.getRowid() );
            db.delete( DBHelper.TABLE_NAME_SUM, selection, null );
            db.close();
        }
        notifyListeners( lock.getRowid() );
    }

    public static long[] gamesList( Context context )
    {
        long[] result = null;

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { ROW_ID };
            String orderBy = DBHelper.CREATE_TIME + " DESC";
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      null, null, null, null, orderBy );
            int count = cursor.getCount();
            result = new long[count];
            int index = cursor.getColumnIndex( ROW_ID );
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                result[ii] = cursor.getLong( index );
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    // Get either the file name or game name, preferring the latter.
    public static String getName( Context context, long rowid )
    {
        String result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { DBHelper.GAME_NAME };
            String selection = String.format( ROW_ID_FMT, rowid );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getString( cursor
                                           .getColumnIndex(DBHelper.GAME_NAME));
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    public static void setName( Context context, long rowid, String name )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = String.format( ROW_ID_FMT, rowid );
            ContentValues values = new ContentValues();
            values.put( DBHelper.GAME_NAME, name );

            int result = db.update( DBHelper.TABLE_NAME_SUM, 
                                    values, selection, null );
            db.close();
            if ( 0 == result ) {
                DbgUtils.logf( "setName(%d,%s) failed", rowid, name );
            }
        }
    }

    public static HistoryPair[] getChatHistory( Context context, long rowid )
    {
        HistoryPair[] result = null;
        final String localPrefix = context.getString( R.string.chat_local_id );
        String history = getChatHistoryStr( context, rowid );
        if ( null != history ) {
            String[] msgs = history.split( "\n" );
            result = new HistoryPair[msgs.length];
            for ( int ii = 0; ii < result.length; ++ii ) {
                String msg = msgs[ii];
                boolean isLocal = msg.startsWith( localPrefix );
                result[ii] = new HistoryPair( msg, isLocal );
            }
        }
        return result;
    }

    private static String getChatHistoryStr( Context context, long rowid )
    {
        String result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { DBHelper.CHAT_HISTORY };
            String selection = String.format( ROW_ID_FMT, rowid );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = 
                    cursor.getString( cursor
                                      .getColumnIndex(DBHelper.CHAT_HISTORY));
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void appendChatHistory( Context context, long rowid,
                                          String msg, boolean local )
    {
        Assert.assertNotNull( msg );
        int id = local ? R.string.chat_local_id : R.string.chat_other_id;
        msg = context.getString( id ) + msg;

        String cur = getChatHistoryStr( context, rowid );
        if ( null != cur ) {
            msg = cur + "\n" + msg;
        }

        saveChatHistory( context, rowid, msg );
    } // appendChatHistory

    public static void clearChatHistory( Context context, long rowid )
    {
        saveChatHistory( context, rowid, null );
    }

    public static void setDBChangeListener( DBChangeListener listener )
    {
        synchronized( s_listeners ) {
            Assert.assertNotNull( listener );
            s_listeners.add( listener );
        }
    }

    public static void clearDBChangeListener( DBChangeListener listener )
    {
        synchronized( s_listeners ) {
            Assert.assertTrue( s_listeners.contains( listener ) );
            s_listeners.remove( listener );
        }
    }

    public static void loadDB( Context context )
    {
        copyGameDB( context, false );
    }

    public static void saveDB( Context context )
    {
        copyGameDB( context, true );
    }

    public static boolean copyFileStream( FileOutputStream fos,
                                          FileInputStream fis )
    {
        boolean success = false;
        FileChannel channelSrc = null;
        FileChannel channelDest = null;
        try {
            channelSrc = fis.getChannel();
            channelDest = fos.getChannel();
            channelSrc.transferTo( 0, channelSrc.size(), channelDest );
            success = true;
        } catch( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        } finally {
            try {
                channelSrc.close();
                channelDest.close();
            } catch( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }
        }
        return success;
    }

    private static void copyGameDB( Context context, boolean toSDCard )
    {
        String name = DBHelper.getDBName();
        File gamesDB = context.getDatabasePath( name );
        File sdcardDB = new File( Environment.getExternalStorageDirectory(),
                                  name );
        try {
            File srcDB = toSDCard? gamesDB : sdcardDB;
            if ( srcDB.exists() ) {
                FileInputStream src = new FileInputStream( srcDB );
                FileOutputStream dest = 
                    new FileOutputStream( toSDCard? sdcardDB : gamesDB );
                copyFileStream( dest, src );
            }
        } catch( java.io.FileNotFoundException fnfe ) {
            DbgUtils.loge( fnfe );
        }
    }

    // Chat is independent of the GameLock mechanism because it's not
    // touching the SNAPSHOT column.
    private static void saveChatHistory( Context context, long rowid,
                                         String history )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = String.format( ROW_ID_FMT, rowid );
            ContentValues values = new ContentValues();
            if ( null != history ) {
                values.put( DBHelper.CHAT_HISTORY, history );
            } else {
                values.putNull( DBHelper.CHAT_HISTORY );
            }

            long timestamp = new Date().getTime();
            values.put( DBHelper.LASTPLAY_TIME, timestamp );

            int result = db.update( DBHelper.TABLE_NAME_SUM, 
                                    values, selection, null );
            db.close();
        }
    }

    private static void initDB( Context context )
    {
        if ( null == s_dbHelper ) {
            s_dbHelper = new DBHelper( context );
        }
    }

    private static void notifyListeners( long rowid )
    {
        synchronized( s_listeners ) {
            Iterator<DBChangeListener> iter = s_listeners.iterator();
            while ( iter.hasNext() ) {
                iter.next().gameSaved( rowid );
            }
        }
    }

    // Trivial one-item cache.  Typically bytes are read three times
    // in a row, so this saves two DB accesses per game opened.  Could
    // use a HashMap, but then lots of half-K byte[] chunks would fail
    // to gc.  This is good enough.
    private static byte[] getCached( long rowid )
    {
        byte[] result = s_cachedRowID == rowid ? s_cachedBytes : null;
        return result;
    }

    private static void setCached( long rowid, byte[] bytes )
    {
        s_cachedRowID = rowid;
        s_cachedBytes = bytes;
    }

}

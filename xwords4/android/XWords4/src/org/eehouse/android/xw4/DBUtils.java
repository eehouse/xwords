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
import org.eehouse.android.xw4.DictUtils.DictLoc;


public class DBUtils {
    public static final int ROWID_NOTFOUND = -1;

    private static final String DICTS_SEP = ",";

    private static final String ROW_ID = "rowid";
    private static final String ROW_ID_FMT = "rowid=%d";
    private static final String NAME_FMT = "%s='%s'";
    private static final String NAMELOC_FMT = "%s='%s' AND %s=%d";

    private static long s_cachedRowID = -1;
    private static byte[] s_cachedBytes = null;

    public static interface DBChangeListener {
        public void gameSaved( long rowid, boolean countChanged );
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

    public static class DictBrowseState {
        public int m_minShown;
        public int m_maxShown;
        public int m_pos;
        public int m_top;
        public String m_prefix;
        public int[] m_counts;
    }

    public static GameSummary getSummary( Context context, long rowid, 
                                          long maxMillis )
    {
        GameSummary result = null;
        GameLock lock = new GameLock( rowid, false ).lock( maxMillis );
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
                                          GameLock lock )
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
                                 /*DBHelper.SMSPHONE,*/ DBHelper.SEED, 
                                 DBHelper.DICTLANG, DBHelper.GAMEID,
                                 DBHelper.SCORES, DBHelper.HASMSGS,
                                 DBHelper.LASTPLAY_TIME, DBHelper.REMOTEDEVS,
                                 DBHelper.LASTMOVE
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
                summary.lastMoveTime = 
                    cursor.getInt(cursor.getColumnIndex(DBHelper.LASTMOVE));

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

    public static void saveSummary( Context context, GameLock lock,
                                    GameSummary summary )
    {
        saveSummary( context, lock, summary, null );
    }

    public static void saveSummary( Context context, GameLock lock,
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
                values.put( DBHelper.LASTMOVE, summary.lastMoveTime );
                
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
            db.close();
            notifyListeners( rowid, false );
            invalGroupsCache();
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
        ContentValues values = new ContentValues();
        values.put( column, value );
        updateRow( null, DBHelper.TABLE_NAME_SUM, rowid, values );
    }

    public static void setMsgFlags( long rowid, int flags )
    {
        setInt( rowid, DBHelper.HASMSGS, flags );
        notifyListeners( rowid, false );
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

    public static boolean gameOver( Context context, long rowid ) 
    {
        return 0 != getInt( context, rowid, DBHelper.GAME_OVER, 0 );
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
            result = new long[cursor.getCount()];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
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
            result = new long[cursor.getCount()];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
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

    public static boolean haveGame( Context context, long rowid ) 
    {
        boolean result = false;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { ROW_ID };
            String selection = String.format( ROW_ID + "=%d", rowid );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            Assert.assertTrue( 1 >= cursor.getCount() );
            result = 1 == cursor.getCount();
            cursor.close();
            db.close();
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

    // Return creation time of newest game matching this nli, or null
    // if none found.
    public static Date getMostRecentCreate( Context context, 
                                            NetLaunchInfo nli )
    {
        Date result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.CREATE_TIME };
            String selection = 
                String.format( "%s='%s' AND %s='%s' AND %s=%d AND %s=%d",
                               DBHelper.ROOMNAME, nli.room, 
                               DBHelper.INVITEID, nli.inviteID, 
                               DBHelper.DICTLANG, nli.lang, 
                               DBHelper.NUM_PLAYERS, nli.nPlayersT );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, 
                                      DBHelper.CREATE_TIME + " DESC" ); // order by
            if ( cursor.moveToNext() ) {
                int indx = cursor.getColumnIndex( DBHelper.CREATE_TIME );
                result = new Date( cursor.getLong( indx ) );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static Date getMostRecentCreate( Context context, Uri data )
    {
        Date result = null;
        NetLaunchInfo nli = new NetLaunchInfo( context, data );
        if ( null != nli && nli.isValid() ) {
            result = getMostRecentCreate( context, nli );
        }
        return result;
    }

    public static String[] getRelayIDs( Context context, long[][] rowIDs ) 
    {
        String[] result = null;
        initDB( context );
        ArrayList<String> ids = new ArrayList<String>();

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { ROW_ID, DBHelper.RELAYID };
            String selection = DBHelper.RELAYID + " NOT null";

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            int count = cursor.getCount();
            if ( 0 < count ) {
                result = new String[count];
                if ( null != rowIDs ) {
                    rowIDs[0] = new long[count];
                }

                int idIndex = cursor.getColumnIndex(DBHelper.RELAYID);
                int rowIndex = cursor.getColumnIndex(ROW_ID);
                for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                    result[ii] = cursor.getString( idIndex );
                    if ( null != rowIDs ) {
                        rowIDs[0][ii] = cursor.getLong( rowIndex );
                    }
                }
            }
            cursor.close();
            db.close();
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

    public static GameLock saveNewGame( Context context, byte[] bytes )
    {
        GameLock lock = null;

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            ContentValues values = new ContentValues();
            values.put( DBHelper.SNAPSHOT, bytes );

            long timestamp = new Date().getTime();
            values.put( DBHelper.CREATE_TIME, timestamp );
            values.put( DBHelper.LASTPLAY_TIME, timestamp );
            values.put( DBHelper.GROUPID, 
                        XWPrefs.getDefaultNewGameGroup( context ) );
            values.put( DBHelper.VISID, maxVISID( db ) );

            long rowid = db.insert( DBHelper.TABLE_NAME_SUM, null, values );

            setCached( rowid, null ); // force reread

            lock = new GameLock( rowid, true ).lock();
            notifyListeners( rowid, true );
        }

        return lock;
    }

    public static long saveGame( Context context, GameLock lock, 
                                 byte[] bytes, boolean setCreate )
    {
        Assert.assertTrue( lock.canWrite() );
        long rowid = lock.getRowid();

        ContentValues values = new ContentValues();
        values.put( DBHelper.SNAPSHOT, bytes );

        long timestamp = new Date().getTime();
        if ( setCreate ) {
            values.put( DBHelper.CREATE_TIME, timestamp );
        }
        values.put( DBHelper.LASTPLAY_TIME, timestamp );

        updateRow( context, DBHelper.TABLE_NAME_SUM, rowid, values );

        setCached( rowid, null ); // force reread
        if ( -1 != rowid ) {      // Means new game?
            notifyListeners( rowid, false );
        }
        invalGroupsCache();
        return rowid;
    }

    public static byte[] loadGame( Context context, GameLock lock )
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
                } else {
                    DbgUtils.logf( "DBUtils.loadGame: none for rowid=%d",
                                   rowid );
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
        GameLock lock = new GameLock( rowid, true ).lock( 300 );
        if ( null != lock ) {
            deleteGame( context, lock );
            lock.unlock();
        } else {
            DbgUtils.logf( "deleteGame: unable to lock rowid %d", rowid );
        }
    }

    public static void deleteGame( Context context, GameLock lock )
    {
        Assert.assertTrue( lock.canWrite() );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = String.format( ROW_ID_FMT, lock.getRowid() );
            db.delete( DBHelper.TABLE_NAME_SUM, selection, null );
            db.close();
        }
        notifyListeners( lock.getRowid(), true );
    }

    public static int getVisID( Context context, long rowid )
    {
        int result = -1;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { DBHelper.VISID };
            String selection = String.format( ROW_ID_FMT, rowid );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getInt( cursor
                                        .getColumnIndex(DBHelper.VISID));
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
        ContentValues values = new ContentValues();
        values.put( DBHelper.GAME_NAME, name );
        updateRow( context, DBHelper.TABLE_NAME_SUM, rowid, values );
    }

    public static HistoryPair[] getChatHistory( Context context, long rowid )
    {
        HistoryPair[] result = null;
        if ( GitVersion.CHAT_SUPPORTED ) {
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
        }
        return result;
    }

    // Groups stuff
    public static class GameGroupInfo {
        public String m_name;
        public boolean m_expanded;
        public long m_lastMoveTime;
        public boolean m_hasTurn;
        public boolean m_turnLocal;

        public GameGroupInfo( String name, boolean expanded ) {
            m_name = name; m_expanded = expanded;
            m_lastMoveTime = 0;
        }
    }

    private static HashMap<Long,GameGroupInfo> s_groupsCache = null;

    private static void invalGroupsCache() 
    {
        s_groupsCache = null;
    }

    // Return map of string (group name) to info about all games in
    // that group.
    public static HashMap<Long,GameGroupInfo> getGroups( Context context )
    {
        if ( null == s_groupsCache ) {
            HashMap<Long,GameGroupInfo> result = 
                new HashMap<Long,GameGroupInfo>();
            initDB( context );
            String[] columns = { ROW_ID, DBHelper.GROUPNAME, 
                                 DBHelper.EXPANDED };
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getReadableDatabase();
                Cursor cursor = db.query( DBHelper.TABLE_NAME_GROUPS, columns, 
                                          null, // selection
                                          null, // args
                                          null, // groupBy
                                          null, // having
                                          null //orderby 
                                          );
                int idIndex = cursor.getColumnIndex( ROW_ID );
                int nameIndex = cursor.getColumnIndex( DBHelper.GROUPNAME );
                int expandedIndex = cursor.getColumnIndex( DBHelper.EXPANDED );

                while ( cursor.moveToNext() ) {
                    String name = cursor.getString( nameIndex );
                    long id = cursor.getLong( idIndex );
                    Assert.assertNotNull( name );
                    boolean expanded = 0 != cursor.getInt( expandedIndex );
                    result.put( id, new GameGroupInfo( name, expanded ) );
                }
                cursor.close();

                Iterator<Long> iter = result.keySet().iterator();
                while ( iter.hasNext() ) {
                    Long id = iter.next();
                    GameGroupInfo ggi = result.get( id );
                    readTurnInfo( db, id, ggi );
                }

                db.close();
            }
            s_groupsCache = result;
        }
        return s_groupsCache;
    } // getGroups

    private static void readTurnInfo( SQLiteDatabase db, long id, 
                                      GameGroupInfo ggi )
    {
        String[] columns = { DBHelper.LASTMOVE, DBHelper.GIFLAGS, 
                             DBHelper.TURN };
        String orderBy = DBHelper.LASTMOVE;
        String selection = String.format( "%s=%d", DBHelper.GROUPID, id );
        Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                  selection,
                                  null, // args
                                  null, // groupBy,
                                  null, // having
                                  orderBy
                                  );
        
        // We want the earliest LASTPLAY_TIME (i.e. the first we see
        // since they're in order) that's a local turn, if any,
        // otherwise a non-local turn.
        long lastPlayTimeLocal = 0;
        long lastPlayTimeRemote = 0;
        int indexLPT = cursor.getColumnIndex( DBHelper.LASTMOVE );
        int indexFlags = cursor.getColumnIndex( DBHelper.GIFLAGS );
        int turnFlags = cursor.getColumnIndex( DBHelper.TURN );
        while ( cursor.moveToNext() && 0 == lastPlayTimeLocal ) {
            int flags = cursor.getInt( indexFlags );
            int turn = cursor.getInt( turnFlags );
            Boolean isLocal = GameSummary.localTurnNext( flags, turn );
            if ( null != isLocal ) {
                long lpt = cursor.getLong( indexLPT );
                if ( isLocal ) {
                    lastPlayTimeLocal = lpt;
                } else if ( 0 == lastPlayTimeRemote ) {
                    lastPlayTimeRemote = lpt;
                }
            }
        }
        cursor.close();

        ggi.m_hasTurn = 0 != lastPlayTimeLocal || 0 != lastPlayTimeRemote;
        if ( ggi.m_hasTurn ) {
            ggi.m_turnLocal = 0 != lastPlayTimeLocal;
            if ( ggi.m_turnLocal ) {
                ggi.m_lastMoveTime = lastPlayTimeLocal;
            } else {
                ggi.m_lastMoveTime = lastPlayTimeRemote;
            }
            // DateFormat df = DateFormat.getDateTimeInstance( DateFormat.SHORT, 
            //                                                 DateFormat.SHORT );
            // DbgUtils.logf( "using last play time %s for", 
            //                df.format( new Date( 1000 * ggi.m_lastMoveTime ) ) );
        }
    }

    public static long[] getGroupGames( Context context, long groupID )
    {
        long[] result = null;
        initDB( context );
        String[] columns = { ROW_ID };
        String selection = String.format( "%s=%d", DBHelper.GROUPID, groupID );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String orderBy = DBHelper.CREATE_TIME + " DESC";
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, // selection
                                      null, // args
                                      null, // groupBy
                                      null, // having
                                      orderBy
                                      );
            int index = cursor.getColumnIndex( ROW_ID );
            result = new long[ cursor.getCount() ];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                long rowid = cursor.getInt( index );
                result[ii] = rowid;
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    public static long getGroupForGame( Context context, long rowid )
    {
        long result = ROWID_NOTFOUND;
        initDB( context );
        String[] columns = { DBHelper.GROUPID };
        String selection = String.format( ROW_ID_FMT, rowid );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, // selection
                                      null, // args
                                      null, // groupBy
                                      null, // having
                                      null //orderby 
                                      );
            if ( cursor.moveToNext() ) {
                int index = cursor.getColumnIndex( DBHelper.GROUPID );
                result = cursor.getLong( index );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static long addGroup( Context context, String name )
    {
        long rowid = ROWID_NOTFOUND;
        if ( null != name && 0 < name.length() ) {
            HashMap<Long,GameGroupInfo> gameInfo = getGroups( context );
            if ( null == gameInfo.get( name ) ) {
                ContentValues values = new ContentValues();
                values.put( DBHelper.GROUPNAME, name );
                values.put( DBHelper.EXPANDED, 0 );

                initDB( context );
                synchronized( s_dbHelper ) {
                    SQLiteDatabase db = s_dbHelper.getWritableDatabase();
                    rowid = db.insert( DBHelper.TABLE_NAME_GROUPS, null, 
                                       values );
                    db.close();
                }
                invalGroupsCache();
            }
        }
        return rowid;
    }
    
    public static void deleteGroup( Context context, long groupid )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            // Nuke games having this group id
            String selection = 
                String.format( "%s=%d", DBHelper.GROUPID, groupid );
            db.delete( DBHelper.TABLE_NAME_SUM, selection, null );

            // And nuke the group record itself
            selection = String.format( ROW_ID_FMT, groupid );
            db.delete( DBHelper.TABLE_NAME_GROUPS, selection, null );

            db.close();
        }
        invalGroupsCache();
    }

    public static void setGroupName( Context context, long groupid, 
                                     String name )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.GROUPNAME, name );
        updateRow( context, DBHelper.TABLE_NAME_GROUPS, groupid, values );
        invalGroupsCache();
    }

    public static void setGroupExpanded( Context context, long groupid, 
                                         boolean expanded )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.EXPANDED, expanded? 1 : 0 );
        updateRow( context, DBHelper.TABLE_NAME_GROUPS, groupid, values );
        invalGroupsCache();
    }

    // Change group id of a game
    public static void moveGame( Context context, long gameid, long groupid )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.GROUPID, groupid );
        updateRow( context, DBHelper.TABLE_NAME_SUM, gameid, values );
    }

    private static String getChatHistoryStr( Context context, long rowid )
    {
        String result = null;
        if ( GitVersion.CHAT_SUPPORTED ) {
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
                                          .getColumnIndex(DBHelper
                                                          .CHAT_HISTORY));
                }
                cursor.close();
                db.close();
            }
        }
        return result;
    }

    public static void appendChatHistory( Context context, long rowid,
                                          String msg, boolean local )
    {
        if ( GitVersion.CHAT_SUPPORTED ) {
            Assert.assertNotNull( msg );
            int id = local ? R.string.chat_local_id : R.string.chat_other_id;
            msg = context.getString( id ) + msg;

            String cur = getChatHistoryStr( context, rowid );
            if ( null != cur ) {
                msg = cur + "\n" + msg;
            }

            saveChatHistory( context, rowid, msg );
        }
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

    /////////////////////////////////////////////////////////////////
    // DictsDB stuff
    /////////////////////////////////////////////////////////////////
    public static DictBrowseState dictsGetOffset( Context context, String name,
                                                  DictLoc loc )
    {
        Assert.assertTrue( DictLoc.UNKNOWN != loc );
        DictBrowseState result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.ITERPOS, DBHelper.ITERTOP,
                                 DBHelper.ITERMIN, DBHelper.ITERMAX,
                                 DBHelper.WORDCOUNTS, DBHelper.ITERPREFIX };
            String selection = 
                String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                               name, DBHelper.LOC, loc.ordinal() );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_DICTBROWSE, columns, 
                                      selection, null, null, null, null );
            if ( 1 >= cursor.getCount() && cursor.moveToFirst() ) {
                result = new DictBrowseState();
                result.m_pos = cursor.getInt( cursor
                                              .getColumnIndex(DBHelper.ITERPOS));
                result.m_top = cursor.getInt( cursor
                                              .getColumnIndex(DBHelper.ITERTOP));
                result.m_minShown = 
                    cursor.getInt( cursor
                                   .getColumnIndex(DBHelper.ITERMIN));
                result.m_maxShown = 
                    cursor.getInt( cursor
                                   .getColumnIndex(DBHelper.ITERMAX));
                result.m_prefix = 
                    cursor.getString( cursor
                                      .getColumnIndex(DBHelper.ITERPREFIX));
                String counts = 
                    cursor.getString( cursor.getColumnIndex(DBHelper.WORDCOUNTS));
                if ( null != counts ) {
                    String[] nums = TextUtils.split( counts, ":" );
                    int[] ints = new int[nums.length];
                    for ( int ii = 0; ii < nums.length; ++ii ) {
                        ints[ii] = Integer.parseInt( nums[ii] );
                    }
                    result.m_counts = ints;
                }
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void dictsSetOffset( Context context, String name, 
                                       DictLoc loc, DictBrowseState state )
    {
        Assert.assertTrue( DictLoc.UNKNOWN != loc );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = 
                String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                               name, DBHelper.LOC, loc.ordinal() );
            ContentValues values = new ContentValues();
            values.put( DBHelper.ITERPOS, state.m_pos );
            values.put( DBHelper.ITERTOP, state.m_top );
            values.put( DBHelper.ITERMIN, state.m_minShown );
            values.put( DBHelper.ITERMAX, state.m_maxShown );
            values.put( DBHelper.ITERPREFIX, state.m_prefix );
            if ( null != state.m_counts ) {
                String[] nums = new String[state.m_counts.length];
                for ( int ii = 0; ii < nums.length; ++ii ) {
                    nums[ii] = String.format( "%d", state.m_counts[ii] );
                }
                values.put( DBHelper.WORDCOUNTS, TextUtils.join( ":", nums ) );
            }
            int result = db.update( DBHelper.TABLE_NAME_DICTBROWSE,
                                    values, selection, null );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, name );
                values.put( DBHelper.LOC, loc.ordinal() );
                db.insert( DBHelper.TABLE_NAME_DICTBROWSE, null, values );
            }
            db.close();
        }
    }

    // Called from jni
    public static String dictsGetMD5Sum( Context context, String name )
    {
        DictInfo info = dictsGetInfo( context, name );
        String result = null == info? null : info.md5Sum;
        return result;
    }

    // Called from jni
    public static void dictsSetMD5Sum( Context context, String name, String sum )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = String.format( NAME_FMT, DBHelper.DICTNAME, name );
            ContentValues values = new ContentValues();
            values.put( DBHelper.MD5SUM, sum );
            int result = db.update( DBHelper.TABLE_NAME_DICTINFO,
                                    values, selection, null );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, name );
                db.insert( DBHelper.TABLE_NAME_DICTINFO, null, values );
            }
            db.close();
        }
    }

    public static DictInfo dictsGetInfo( Context context, String name )
    {
        DictInfo result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.LANGCODE,
                                 DBHelper.WORDCOUNT,
                                 DBHelper.MD5SUM,
                                 DBHelper.LOC };
            String selection = String.format( NAME_FMT, DBHelper.DICTNAME, name );
            Cursor cursor = db.query( DBHelper.TABLE_NAME_DICTINFO, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = new DictInfo();
                result.name = name;
                result.langCode = 
                    cursor.getInt( cursor.getColumnIndex(DBHelper.LANGCODE));
                result.wordCount = 
                    cursor.getInt( cursor.getColumnIndex(DBHelper.WORDCOUNT));
                result.md5Sum =
                    cursor.getString( cursor.getColumnIndex(DBHelper.MD5SUM));
             }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void dictsSetInfo( Context context, DictUtils.DictAndLoc dal,
                                     DictInfo info )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = 
                String.format( NAME_FMT, DBHelper.DICTNAME, dal.name );
            ContentValues values = new ContentValues();

            values.put( DBHelper.LANGCODE, info.langCode );
            values.put( DBHelper.WORDCOUNT, info.wordCount );
            values.put( DBHelper.MD5SUM, info.md5Sum );
            values.put( DBHelper.LOC, dal.loc.ordinal() );

            int result = db.update( DBHelper.TABLE_NAME_DICTINFO,
                                    values, selection, null );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, dal.name );
                db.insert( DBHelper.TABLE_NAME_DICTINFO, null, values );
            }
            db.close();
        }
    }

    public static void dictsMoveInfo( Context context, String name,
                                      DictLoc fromLoc, DictLoc toLoc )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = 
                String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                               name, DBHelper.LOC, fromLoc.ordinal() );
            ContentValues values = new ContentValues();
            values.put( DBHelper.LOC, toLoc.ordinal() );
            db.update( DBHelper.TABLE_NAME_DICTINFO, values, selection, null );
            db.update( DBHelper.TABLE_NAME_DICTBROWSE, values, selection, null);
            db.close();
        }
    }

    public static void dictsRemoveInfo( Context context, 
                                        DictUtils.DictAndLoc dal )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = 
                String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                               dal.name, DBHelper.LOC, dal.loc.ordinal() );
            db.delete( DBHelper.TABLE_NAME_DICTINFO, selection, null );
            db.delete( DBHelper.TABLE_NAME_DICTBROWSE, selection, null );
            db.close();
        }
    }

    public static boolean gameDBExists( Context context )
    {
        String name = DBHelper.getDBName();
        File sdcardDB = new File( Environment.getExternalStorageDirectory(),
                                  name );
        return sdcardDB.exists();
    }

    public static String[] getColumns( SQLiteDatabase db, String name )
    {
        String query = String.format( "SELECT * FROM %s LIMIT 1", name );
        Cursor cursor = db.rawQuery( query, null );
        String[] colNames = cursor.getColumnNames();
        cursor.close();
        return colNames;
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
        ContentValues values = new ContentValues();
        if ( null != history ) {
            values.put( DBHelper.CHAT_HISTORY, history );
        } else {
            values.putNull( DBHelper.CHAT_HISTORY );
        }
        values.put( DBHelper.LASTPLAY_TIME, new Date().getTime() );
        updateRow( context, DBHelper.TABLE_NAME_SUM, rowid, values );
    }

    private static void initDB( Context context )
    {
        if ( null == s_dbHelper ) {
            Assert.assertNotNull( context );
            s_dbHelper = new DBHelper( context );
            // force any upgrade
            s_dbHelper.getWritableDatabase().close();
        }
    }

    private static void updateRow( Context context, String table,
                                   long rowid, ContentValues values )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = String.format( ROW_ID_FMT, rowid );

            int result = db.update( table, values, selection, null );
            db.close();
            if ( 0 == result ) {
                DbgUtils.logf( "updateRow failed" );
            }
        }
    }

    private static int maxVISID( SQLiteDatabase db )
    {
        int result = 1;
        String query = String.format( "SELECT max(%s) FROM %s", DBHelper.VISID,
                                      DBHelper.TABLE_NAME_SUM );
        Cursor cursor = null;
        try {
            cursor = db.rawQuery( query, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = 1 + cursor.getInt( 0 );
            }
        } finally {
            if ( null != cursor ) {
                cursor.close();
            }
        }
        return result;
    }
    
    private static void notifyListeners( long rowid, boolean countChanged )
    {
        synchronized( s_listeners ) {
            Iterator<DBChangeListener> iter = s_listeners.iterator();
            while ( iter.hasNext() ) {
                iter.next().gameSaved( rowid, countChanged );
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

/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.Cursor;
import java.util.StringTokenizer;
import android.content.ContentValues;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.Iterator;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;


public class DBUtils {

    public static interface DBChangeListener {
        public void pathSaved( String path );
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

    public static GameSummary getSummary( Context context, String file, 
                                          boolean wait )
    {
        GameSummary result = null;
        GameUtils.GameLock lock = new GameUtils.GameLock( file, false );
        if ( wait ) {
            result = getSummary( context, lock.lock() );
            lock.unlock();
        } else if ( lock.tryLock() ) {
            result = getSummary( context, lock );
            lock.unlock();
        }
        return result;
    }

    public static GameSummary getSummary( Context context, 
                                          GameUtils.GameLock lock )
    {
        initDB( context );
        GameSummary summary = null;

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.NUM_MOVES, DBHelper.NUM_PLAYERS,
                                 DBHelper.GAME_OVER, DBHelper.PLAYERS,
                                 DBHelper.TURN, DBHelper.GIFLAGS,
                                 DBHelper.CONTYPE, DBHelper.SERVERROLE,
                                 DBHelper.ROOMNAME, DBHelper.RELAYID, 
                                 DBHelper.SMSPHONE, DBHelper.SEED, 
                                 DBHelper.DICTLANG, DBHelper.DICTNAME,
                                 DBHelper.SCORES, DBHelper.HASMSGS,
                                 DBHelper.LASTPLAY_TIME
            };
            String selection = DBHelper.FILE_NAME + "=\"" + lock.getPath() + "\"";

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                summary = new GameSummary();
                summary.nMoves = cursor.getInt(cursor.
                                               getColumnIndex(DBHelper.NUM_MOVES));
                summary.nPlayers = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.NUM_PLAYERS));
                summary.turn = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.TURN));
                summary.giFlags = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.GIFLAGS));
                summary.players = 
                    parsePlayers( cursor.getString(cursor.
                                                   getColumnIndex(DBHelper.
                                                                  PLAYERS)),
                                  summary.nPlayers );
                summary.dictLang = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.DICTLANG));
                summary.dictName = 
                    cursor.getString(cursor.
                                     getColumnIndex(DBHelper.DICTNAME));
                summary.modtime = 
                    cursor.getLong(cursor.
                                   getColumnIndex(DBHelper.LASTPLAY_TIME));
                int tmp = cursor.getInt(cursor.
                                        getColumnIndex(DBHelper.GAME_OVER));
                summary.gameOver = tmp == 0 ? false : true;

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
                    col = cursor.getColumnIndex( DBHelper.ROOMNAME );
                    if ( col >= 0 ) {
                        summary.roomName = cursor.getString( col );
                    }
                    col = cursor.getColumnIndex( DBHelper.RELAYID );
                    if ( col >= 0 ) {
                        summary.relayID = cursor.getString( col );
                    }
                    col = cursor.getColumnIndex( DBHelper.SEED );
                    if ( col >= 0 ) {
                        summary.seed = cursor.getInt( col );
                    }
                    col = cursor.getColumnIndex( DBHelper.SMSPHONE );
                    if ( col >= 0 ) {
                        summary.smsPhone = cursor.getString( col );
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
        Assert.assertTrue( lock.canWrite() );
        String path = lock.getPath();
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            if ( null == summary ) {
                String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
                db.delete( DBHelper.TABLE_NAME_SUM, selection, null );
            } else {
                ContentValues values = new ContentValues();
                values.put( DBHelper.NUM_MOVES, summary.nMoves );
                values.put( DBHelper.NUM_PLAYERS, summary.nPlayers );
                values.put( DBHelper.TURN, summary.turn );
                values.put( DBHelper.GIFLAGS, summary.giflags() );
                values.put( DBHelper.PLAYERS, 
                            summary.summarizePlayers(context) );
                values.put( DBHelper.DICTLANG, summary.dictLang );
                values.put( DBHelper.DICTNAME, summary.dictName );
                values.put( DBHelper.GAME_OVER, summary.gameOver );

                if ( null != summary.scores ) {
                    StringBuffer sb = new StringBuffer();
                    for ( int score : summary.scores ) {
                        sb.append( String.format( "%d ", score ) );
                    }
                    values.put( DBHelper.SCORES, sb.toString() );
                }

                if ( null != summary.conType ) {
                    values.put( DBHelper.CONTYPE, summary.conType.ordinal() );
                    values.put( DBHelper.ROOMNAME, summary.roomName );
                    values.put( DBHelper.RELAYID, summary.relayID );
                    values.put( DBHelper.SEED, summary.seed );
                    values.put( DBHelper.SMSPHONE, summary.smsPhone );
                }
                values.put( DBHelper.SERVERROLE, summary.serverRole.ordinal() );

                Utils.logf( "saveSummary: nMoves=%d", summary.nMoves );

                String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
                long result = db.update( DBHelper.TABLE_NAME_SUM,
                                         values, selection, null );
                Assert.assertTrue( result >= 0 );
            }
            notifyListeners( path );
            db.close();
        }
    } // saveSummary

    public static int countGamesUsing( Context context, String dict )
    {
        int result = 0;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String selection = DBHelper.DICTNAME + " LIKE \'" 
                + dict + "\'";
            // null for columns will return whole rows: bad
            String[] columns = { DBHelper.DICTNAME };
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );

            result = cursor.getCount();
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void setMsgFlags( String path, int flags )
    {
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
            ContentValues values = new ContentValues();
            values.put( DBHelper.HASMSGS, flags );

            int result = db.update( DBHelper.TABLE_NAME_SUM, 
                                    values, selection, null );
            Assert.assertTrue( result == 1 );
            db.close();
        }
        notifyListeners( path );
    }

    public static int getMsgFlags( String path )
    {
        int flags = GameSummary.MSG_FLAGS_NONE;
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
            String[] columns = { DBHelper.HASMSGS };
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                flags = cursor.getInt( cursor
                                         .getColumnIndex(DBHelper.HASMSGS));
            }
            cursor.close();
            db.close();
        }
        return flags;
    }

    public static String getPathFor( Context context, String relayID )
    {
        String result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.FILE_NAME };
            String selection = DBHelper.RELAYID + "='" + relayID + "'";
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getString( cursor
                                           .getColumnIndex(DBHelper.FILE_NAME));

            }
            cursor.close();
            db.close();
        }
        return result;
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

            if ( 0 < cursor.getCount() ) {
                cursor.moveToFirst();
                for ( ; ; ) {
                    ids.add( cursor.
                             getString( cursor.
                                        getColumnIndex(DBHelper.RELAYID)) );
                    if ( cursor.isLast() ) {
                        break;
                    }
                    cursor.moveToNext();
                }
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
                Utils.logf( "ex: %s", ex.toString() );
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
                cursor.moveToFirst();
                for ( ; ; ) {
                    int index = cursor.getColumnIndex( DBHelper.RELAYID );
                    String relayID = cursor.getString( index );
                    index = cursor.getColumnIndex( DBHelper.SEED );
                    int seed = cursor.getInt( index );
                    al.add( new Obit( relayID, seed ) );
                    if ( cursor.isLast() ) {
                        break;
                    }
                    cursor.moveToNext();
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

    public static void saveGame( Context context, GameUtils.GameLock lock, 
                                 byte[] bytes, boolean setCreate )
    {
        Assert.assertTrue( lock.canWrite() );
        String path = lock.getPath();
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
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
                values.put( DBHelper.FILE_NAME, path );
                long row = db.insert( DBHelper.TABLE_NAME_SUM, null, values );
                Assert.assertTrue( row >= 0 );
            }
            db.close();
        }
        notifyListeners( path );
    }

    public static byte[] loadGame( Context context, GameUtils.GameLock lock )
    {
        String path = lock.getPath();
        Assert.assertNotNull( path );
        byte[] result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { DBHelper.SNAPSHOT };
            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getBlob( cursor
                                         .getColumnIndex(DBHelper.SNAPSHOT));
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void deleteGame( Context context, GameUtils.GameLock lock )
    {
        Assert.assertTrue( lock.canWrite() );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = DBHelper.FILE_NAME + "=\"" + lock.getPath() + "\"";
            db.delete( DBHelper.TABLE_NAME_SUM, selection, null );
            db.close();
        }
        notifyListeners( lock.getPath() );
    }

    public static String[] gamesList( Context context )
    {
        ArrayList<String> al = new ArrayList<String>();

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { DBHelper.FILE_NAME };
            String orderBy = DBHelper.FILE_NAME + " DESC";
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      null, null, null, null, orderBy );
            if ( 0 < cursor.getCount() ) {
                cursor.moveToFirst();
                for ( ; ; ) {
                    int index = cursor.getColumnIndex( DBHelper.FILE_NAME );
                    String name = cursor.getString( index );
                    al.add( cursor.getString( index ) );
                    if ( cursor.isLast() ) {
                        break;
                    }
                    cursor.moveToNext();
                }
            }
            cursor.close();
            db.close();
        }

        return al.toArray( new String[al.size()] );
    }

    public static HistoryPair[] getChatHistory( Context context, String path )
    {
        HistoryPair[] result = null;
        final String localPrefix = context.getString( R.string.chat_local_id );
        String history = getChatHistoryStr( context, path );
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

    private static String getChatHistoryStr( Context context, String path )
    {
        String result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { DBHelper.CHAT_HISTORY };
            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
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

    public static void appendChatHistory( Context context, String path,
                                          String msg, boolean local )
    {
        Assert.assertNotNull( msg );
        int id = local ? R.string.chat_local_id : R.string.chat_other_id;
        msg = context.getString( id ) + msg;

        String cur = getChatHistoryStr( context, path );
        if ( null != cur ) {
            msg = cur + "\n" + msg;
        }

        saveChatHistory( context, path, msg );
    } // appendChatHistory

    public static void clearChatHistory( Context context, String path )
    {
        saveChatHistory( context, path, null );
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

    // Chat is independent of the GameLock mechanism because it's not
    // touching the SNAPSHOT column.
    private static void saveChatHistory( Context context, String path,
                                         String history )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
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

    private static String[] parsePlayers( final String players, int nPlayers ){
        String[] result = null;
        if ( null != players ) {
            result = new String[nPlayers];
            String sep = "vs. ";
            if ( players.contains("\n") ) {
                sep = "\n";
            }

            int ii, nxt;
            for ( ii = 0, nxt = 0; ; ++ii ) {
                int prev = nxt;
                nxt = players.indexOf( sep, nxt );
                String name = -1 == nxt ?
                    players.substring( prev ) : players.substring( prev, nxt );
                result[ii] = name;
                if ( -1 == nxt ) {
                    break;
                }
                nxt += sep.length();
            }
        }
        return result;
    }

    private static void initDB( Context context )
    {
        if ( null == s_dbHelper ) {
            s_dbHelper = new DBHelper( context );
        }
    }

    private static void notifyListeners( String path )
    {
        synchronized( s_listeners ) {
            Iterator<DBChangeListener> iter = s_listeners.iterator();
            while ( iter.hasNext() ) {
                iter.next().pathSaved( path );
            }
        }
    }

}

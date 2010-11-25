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
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;


public class DBUtils {

    private static SQLiteOpenHelper s_dbHelper = null;

    public static class Obit {
        public Obit( String relayID, int seed ) {
            m_relayID = relayID; m_seed = seed;
        }
        String m_relayID;
        int m_seed;
    }

    public static GameSummary getSummary( Context context, String file )
    {
        initDB( context );
        GameSummary summary = null;

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.NUM_MOVES, DBHelper.NUM_PLAYERS,
                                 DBHelper.GAME_OVER, DBHelper.PLAYERS,
                                 DBHelper.CONTYPE, DBHelper.SERVERROLE,
                                 DBHelper.ROOMNAME, DBHelper.RELAYID, 
                                 DBHelper.SMSPHONE, DBHelper.SEED, 
                                 DBHelper.DICTLANG, DBHelper.DICTNAME,
                                 DBHelper.SCORES, DBHelper.HASMSGS
            };
            String selection = DBHelper.FILE_NAME + "=\"" + file + "\"";

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                summary = new GameSummary();
                summary.nMoves = cursor.getInt(cursor.
                                               getColumnIndex(DBHelper.NUM_MOVES));
                summary.nPlayers = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.NUM_PLAYERS));
                summary.players = 
                    cursor.getString(cursor.
                                  getColumnIndex(DBHelper.PLAYERS));
                summary.dictLang = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.DICTLANG));
                summary.dictName = 
                    cursor.getString(cursor.
                                     getColumnIndex(DBHelper.DICTNAME));
                int tmp = cursor.getInt(cursor.
                                        getColumnIndex(DBHelper.GAME_OVER));
                summary.gameOver = tmp == 0 ? false : true;

                String scoresStr = 
                    cursor.getString( cursor.getColumnIndex(DBHelper.SCORES));
                if ( null != scoresStr ) {
                    StringTokenizer st = new StringTokenizer( scoresStr );
                    int[] scores = new int[st.countTokens()];
                    for ( int ii = 0; ii < scores.length; ++ii ) {
                        Assert.assertTrue( st.hasMoreTokens() );
                        String token = st.nextToken();
                        scores[ii] = Integer.parseInt( token );
                    }
                    summary.scores = scores;
                }

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
                    summary.msgsPending = 0 != cursor.getInt( col );
                }
            }
            cursor.close();
            db.close();
        }

        if ( null == summary ) {
            summary = GameUtils.summarize( context, file );
            saveSummary( context, file, summary );
        }
        return summary;
    }

    public static void saveSummary( Context context, String path, 
                                    GameSummary summary )
    {
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
                values.put( DBHelper.PLAYERS, 
                            summary.summarizePlayers(context) );
                values.put( DBHelper.DICTLANG, summary.dictLang );
                values.put( DBHelper.DICTNAME, summary.dictName );
                values.put( DBHelper.GAME_OVER, summary.gameOver );
                values.put( DBHelper.HASMSGS, 0 );

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

    public static void setHasMsgs( String relayID )
    {
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            
            String selection = DBHelper.RELAYID + "=\'" + relayID + "\'";
            ContentValues values = new ContentValues();
            values.put( DBHelper.HASMSGS, 1 );
            
            int result = db.update( DBHelper.TABLE_NAME_SUM, 
                                    values, selection, null );
            Assert.assertTrue( result == 1 );
            db.close();
        }
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

    public static String[] getRelayIDNoMsgs( Context context )
    {
        String[] result = null;
        initDB( context );
        ArrayList<String> ids = new ArrayList<String>();

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            String[] columns = { DBHelper.RELAYID };
            String selection = DBHelper.RELAYID + " NOT null AND " 
                + "NOT " + DBHelper.HASMSGS;

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

    public static void saveGame( Context context, String path, byte[] bytes )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
            ContentValues values = new ContentValues();
            values.put( DBHelper.SNAPSHOT, bytes );

            int result = db.update( DBHelper.TABLE_NAME_SUM, 
                                    values, selection, null );
            if ( 0 == result ) {
                values.put( DBHelper.FILE_NAME, path );
                long row = db.insert( DBHelper.TABLE_NAME_SUM, null, values );
                Assert.assertTrue( row >= 0 );
            }
            db.close();
        }
    }

    public static byte[] loadGame( Context context, String path )
    {
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

    public static void deleteGame( Context context, String path )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
            db.delete( DBHelper.TABLE_NAME_SUM, selection, null );
            db.close();
        }
    }

    public static String[] gamesList( Context context )
    {
        ArrayList<String> al = new ArrayList<String>();

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            String[] columns = { DBHelper.FILE_NAME };
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      null, null, null, null, null );
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

    private static void initDB( Context context )
    {
        if ( null == s_dbHelper ) {
            s_dbHelper = new DBHelper( context );
        }
    }

}

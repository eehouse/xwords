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
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;


public class DBUtils {

    private static SQLiteOpenHelper m_dbHelper = null;

    public static GameSummary getSummary( Context context, String file )
    {
        initDB( context );

        GameSummary summary = new GameSummary();
        SQLiteDatabase db = m_dbHelper.getReadableDatabase();

        String[] columns = { DBHelper.NUM_MOVES, DBHelper.GAME_OVER,
                             DBHelper.CONTYPE, DBHelper.ROOMNAME,
                             DBHelper.SMSPHONE, DBHelper.SCORES
        };
        String selection = DBHelper.FILE_NAME + "=\"" + file + "\"";

        Cursor cursor = db.query( DBHelper.TABLE_NAME, columns, selection, 
                                  null, null, null, null );
        if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
            summary = new GameSummary();
            summary.nMoves = cursor.getInt(cursor.
                                           getColumnIndex(DBHelper.NUM_MOVES));
            int tmp = cursor.getInt(cursor.
                                    getColumnIndex(DBHelper.GAME_OVER));
            summary.gameOver = tmp == 0 ? false : true;

            String scoresStr = cursor.getString( cursor.
                                                 getColumnIndex(DBHelper.SCORES));
            StringTokenizer st = new StringTokenizer( scoresStr );
            int[] scores = new int[st.countTokens()];
            for ( int ii = 0; ii < scores.length; ++ii ) {
                Assert.assertTrue( st.hasMoreTokens() );
                String token = st.nextToken();
                scores[ii] = Integer.parseInt( token );
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
                col = cursor.getColumnIndex( DBHelper.SMSPHONE );
                if ( col >= 0 ) {
                    summary.smsPhone = cursor.getString( col );
                }
            }
        }
        cursor.close();
        db.close();
        return summary;
    }

    public static void saveSummary( String path, GameSummary summary )
    {
        SQLiteDatabase db = m_dbHelper.getWritableDatabase();

        if ( null == summary ) {
            String selection = DBHelper.FILE_NAME + "=\"" + path + "\"";
            db.delete( DBHelper.TABLE_NAME, selection, null );
        } else {
            ContentValues values = new ContentValues();
            values.put( DBHelper.FILE_NAME, path );
            values.put( DBHelper.NUM_MOVES, summary.nMoves );
            values.put( DBHelper.GAME_OVER, summary.gameOver );

            StringBuffer sb = new StringBuffer();
            for ( int score : summary.scores ) {
                sb.append( String.format( "%d ", score ) );
            }
            values.put( DBHelper.SCORES, sb.toString() );

            if ( null != summary.conType ) {
                values.put( DBHelper.CONTYPE, summary.conType.ordinal() );
                Utils.logf( "wrote CONTYPE" );
                values.put( DBHelper.ROOMNAME, summary.roomName );
                values.put( DBHelper.SMSPHONE, summary.smsPhone );
            }

            Utils.logf( "saveSummary: nMoves=%d", summary.nMoves );

            try {
                long result = db.replaceOrThrow( DBHelper.TABLE_NAME, "", values );
            } catch ( Exception ex ) {
                Utils.logf( "ex: %s", ex.toString() );
            }
        }
        db.close();
    }

    private static void initDB( Context context )
    {
        if ( null == m_dbHelper ) {
            m_dbHelper = new DBHelper( context );
        }
    }

}

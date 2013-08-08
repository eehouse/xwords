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

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.text.TextUtils;
import java.util.ArrayList;
import java.util.Arrays;

public class DBHelper extends SQLiteOpenHelper {

    public static final String TABLE_NAME_SUM = "summaries";
    public static final String TABLE_NAME_OBITS = "obits";
    public static final String TABLE_NAME_DICTBROWSE = "dictbrowse";
    public static final String TABLE_NAME_DICTINFO = "dictinfo";
    public static final String TABLE_NAME_GROUPS = "groups";
    private static final String DB_NAME = "xwdb";
    private static final int DB_VERSION = 17;

    public static final String GAME_NAME = "GAME_NAME";
    public static final String VISID = "VISID";
    public static final String NUM_MOVES = "NUM_MOVES";
    public static final String TURN = "TURN";
    public static final String GIFLAGS = "GIFLAGS";

    public static final String PLAYERS = "PLAYERS";
    public static final String NUM_PLAYERS = "NUM_PLAYERS";
    public static final String MISSINGPLYRS = "MISSINGPLYRS";
    public static final String GAME_OVER = "GAME_OVER";
    public static final String IN_USE = "IN_USE";
    public static final String SCORES = "SCORES";
    public static final String CHAT_HISTORY = "CHAT_HISTORY";
    public static final String GAMEID = "GAMEID";
    public static final String REMOTEDEVS = "REMOTEDEVS";
    public static final String DICTLANG = "DICTLANG";
    public static final String DICTLIST = "DICTLIST";
    public static final String HASMSGS = "HASMSGS";
    public static final String CONTRACTED = "CONTRACTED";
    public static final String SNAPSHOT = "SNAPSHOT";
    public static final String CONTYPE = "CONTYPE";
    public static final String SERVERROLE = "SERVERROLE";
    public static final String ROOMNAME = "ROOMNAME";
    public static final String INVITEID = "INVITEID";
    public static final String RELAYID = "RELAYID";
    public static final String SEED = "SEED";
    public static final String SMSPHONE = "SMSPHONE"; // unused -- so far
    public static final String LASTMOVE = "LASTMOVE";
    public static final String GROUPID = "GROUPID";

    public static final String DICTNAME = "DICTNAME";
    public static final String MD5SUM = "MD5SUM";
    public static final String WORDCOUNT = "WORDCOUNT";
    public static final String WORDCOUNTS = "WORDCOUNTS";
    public static final String LANGCODE = "LANGCODE";
    public static final String LOC = "LOC";     
    public static final String ITERMIN = "ITERMIN";
    public static final String ITERMAX = "ITERMAX";
    public static final String ITERPOS = "ITERPOS";
    public static final String ITERTOP = "ITERTOP";
    public static final String ITERPREFIX = "ITERPREFIX";
    public static final String CREATE_TIME = "CREATE_TIME";
    public static final String LASTPLAY_TIME = "LASTPLAY_TIME";

    public static final String GROUPNAME = "GROUPNAME";
    public static final String EXPANDED = "EXPANDED";

    private Context m_context;

    private static final String[][] s_summaryColsAndTypes = {
        { "rowid",      "INTEGER PRIMARY KEY AUTOINCREMENT" }
        ,{ VISID,        "INTEGER" }
        ,{ GAME_NAME,    "TEXT" }
        ,{ NUM_MOVES,   "INTEGER" }
        ,{ TURN,        "INTEGER" }
        ,{ GIFLAGS,     "INTEGER" }
        ,{ NUM_PLAYERS, "INTEGER" }
        ,{ MISSINGPLYRS,"INTEGER" }
        ,{ PLAYERS,      "TEXT" }
        ,{ GAME_OVER,    "INTEGER" }
        ,{ SERVERROLE,   "INTEGER" }
        ,{ CONTYPE,      "INTEGER" }
        ,{ ROOMNAME,     "TEXT" }
        ,{ INVITEID,     "TEXT" }
        ,{ RELAYID,      "TEXT" }
        ,{ SEED,         "INTEGER" }
        ,{ DICTLANG,     "INTEGER" }
        ,{ DICTLIST,     "TEXT" }
        ,{ SMSPHONE,     "TEXT" }  // unused
        ,{ SCORES,       "TEXT" }
        ,{ CHAT_HISTORY, "TEXT" }
        ,{ GAMEID,       "INTEGER" }
        ,{ REMOTEDEVS,   "TEXT" }
        ,{ LASTMOVE,     "INTEGER DEFAULT 0" }
        ,{ GROUPID,      "INTEGER" }
        // HASMSGS: sqlite doesn't have bool; use 0 and 1
        ,{ HASMSGS,      "INTEGER DEFAULT 0" }
        ,{ CONTRACTED,   "INTEGER DEFAULT 0" }
        ,{ CREATE_TIME,  "INTEGER" }
        ,{ LASTPLAY_TIME,"INTEGER" }
        ,{ SNAPSHOT,     "BLOB" }
    };

    private static final String[][] s_obitsColsAndTypes = {
        { RELAYID, "TEXT" }
        ,{ SEED,   "INTEGER" }
    };

    private static final String[][] s_dictInfoColsAndTypes = {
        { DICTNAME, "TEXT" }
        ,{ LOC,      "UNSIGNED INTEGER(1)" }
        ,{ MD5SUM,   "TEXT(32)" }
        ,{ WORDCOUNT,"INTEGER" }
        ,{ LANGCODE, "INTEGER" }
    };

    private static final String[][] s_dictBrowseColsAndTypes = {
        { DICTNAME,    "TEXT" }
        ,{ LOC,        "UNSIGNED INTEGER(1)" }
        ,{ WORDCOUNTS, "TEXT" }
        ,{ ITERMIN,    "INTEGRE(4)" }
        ,{ ITERMAX,    "INTEGER(4)" }
        ,{ ITERPOS,    "INTEGER" }
        ,{ ITERTOP,    "INTEGER" }
        ,{ ITERPREFIX, "TEXT" }
    };

    private static final String[][] s_groupsSchema = {
        { GROUPNAME,  "TEXT" }
        ,{ EXPANDED,  "INTEGER(1)" }
    };

    public DBHelper( Context context )
    {
        super( context, DB_NAME, null, DB_VERSION );
        m_context = context;
    }

    public static String getDBName()
    {
        return DB_NAME;
    }

    @Override
    public void onCreate( SQLiteDatabase db ) 
    {
        createTable( db, TABLE_NAME_SUM, s_summaryColsAndTypes );
        createTable( db, TABLE_NAME_OBITS, s_obitsColsAndTypes );
        createTable( db, TABLE_NAME_DICTINFO, s_dictInfoColsAndTypes );
        createTable( db, TABLE_NAME_DICTBROWSE, s_dictBrowseColsAndTypes );
        forceRowidHigh( db, TABLE_NAME_SUM );
        createGroupsTable( db );
    }

    @Override
    @SuppressWarnings("fallthrough")
    public void onUpgrade( SQLiteDatabase db, int oldVersion, int newVersion ) 
    {
        DbgUtils.logf( "onUpgrade: old: %d; new: %d", oldVersion, newVersion );

        switch( oldVersion ) {
        case 5:
            createTable( db, TABLE_NAME_OBITS, s_obitsColsAndTypes );
        case 6:
            addSumColumn( db, TURN );
            addSumColumn( db, GIFLAGS );
            addSumColumn( db, CHAT_HISTORY );
        case 7:
            addSumColumn( db, MISSINGPLYRS );
        case 8:
            addSumColumn( db, GAME_NAME );
            addSumColumn( db, CONTRACTED );
        case 9:
            addSumColumn( db, DICTLIST );
        case 10:
            addSumColumn( db, INVITEID );
        case 11:
            addSumColumn( db, REMOTEDEVS );
        case 12:
            createTable( db, TABLE_NAME_DICTINFO, s_dictInfoColsAndTypes );
            createTable( db, TABLE_NAME_DICTBROWSE, s_dictBrowseColsAndTypes );
        case 13:
            addSumColumn( db, LASTMOVE );
        case 14:
            addSumColumn( db, GROUPID );
            createGroupsTable( db );
        case 15:
            moveToCurGames( db );
        case 16:
            addSumColumn( db, VISID );
            setColumnsEqual( db, TABLE_NAME_SUM, VISID, "rowid" );
            makeAutoincrement( db, TABLE_NAME_SUM, s_summaryColsAndTypes );
            // nothing yet
            break;
        default:
            db.execSQL( "DROP TABLE " + TABLE_NAME_SUM + ";" );
            if ( oldVersion >= 6 ) {
                db.execSQL( "DROP TABLE " + TABLE_NAME_OBITS + ";" );
            }
            onCreate( db );
        }
    }

    private void addSumColumn( SQLiteDatabase db, String colName )
    {
        String colType = null;
        for ( int ii = 0; ii < s_summaryColsAndTypes.length; ++ii ) {
            if ( s_summaryColsAndTypes[ii][0].equals( colName ) ) {
                colType = s_summaryColsAndTypes[ii][1];
                break;
            }
        }

        String cmd = String.format( "ALTER TABLE %s ADD COLUMN %s %s;",
                                    TABLE_NAME_SUM, colName, colType );
        db.execSQL( cmd );
    }

    private void createTable( SQLiteDatabase db, String name, String[][] data ) 
    {
        StringBuilder query = 
            new StringBuilder( String.format("CREATE TABLE %s (", name ) );

        for ( int ii = 0; ii < data.length; ++ii ) {
            String col = String.format( " %s %s,", data[ii][0], data[ii][1] );
            query.append( col );
        }
        query.setLength(query.length() - 1); // nuke the last comma
        query.append( ");" );

        db.execSQL( query.toString() );
    }

    private void createGroupsTable( SQLiteDatabase db )
    {
        createTable( db, TABLE_NAME_GROUPS, s_groupsSchema );

        // Create an empty group name
        ContentValues values = new ContentValues();
        values.put( GROUPNAME, m_context.getString(R.string.group_cur_games) );
        values.put( EXPANDED, 1 );
        long curGroup = db.insert( TABLE_NAME_GROUPS, null, values );
        values = new ContentValues();
        values.put( GROUPNAME, m_context.getString(R.string.group_new_games) );
        values.put( EXPANDED, 0 );
        long newGroup = db.insert( TABLE_NAME_GROUPS, null, values );

        // place all existing games in the initial unnamed group
        values = new ContentValues();
        values.put( GROUPID, curGroup );
        db.update( DBHelper.TABLE_NAME_SUM, values, null, null );

        XWPrefs.setDefaultNewGameGroup( m_context, newGroup );
    }

    // Move all existing games to the row previously named "cur games'
    private void moveToCurGames( SQLiteDatabase db )
    {
        String name = m_context.getString( R.string.group_cur_games );
        String[] columns = { "rowid" };
        String selection = String.format( "%s = '%s'", GROUPNAME, name );
        Cursor cursor = db.query( DBHelper.TABLE_NAME_GROUPS, columns, 
                                  selection, null, null, null, null );
        if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
            long rowid = cursor.getLong( cursor.getColumnIndex("rowid") );

            ContentValues values = new ContentValues();
            values.put( GROUPID, rowid );
            db.update( DBHelper.TABLE_NAME_SUM, values, null, null );
        }
        cursor.close();
    }

    private void makeAutoincrement( SQLiteDatabase db, String name, 
                                    String[][] data )
    {
        db.beginTransaction();
        try {
            String query;
            String[] columnNames = DBUtils.getColumns( db, name );
            if ( null != columnNames ) { // no data means no need to copy
                query = String.format( "ALTER table %s RENAME TO 'temp_%s'",
                                       name, name );
                db.execSQL( query );
            }
            createTable( db, name, data );
            forceRowidHigh( db, name );
            
            if ( null != columnNames ) {
                ArrayList<String> oldCols = 
                    new ArrayList<String>( Arrays.asList( columnNames ) );

                // Make a list of columns in the new DB, using it to
                // remove from the old list any that aren't in the
                // new.  Old tables may have column names we no longer
                // use, but we can't try to copy them because the new
                // doesn't have 'em. Note that calling getColumns() on
                // the newly-created table doesn't work, perhaps
                // because we're in a transaction and nothing's been
                // committed.
                ArrayList<String> newCols = new ArrayList<String>();
                for ( int ii = 0; ii < data.length; ++ii ) {
                    newCols.add( data[ii][0] );
                }
                oldCols.retainAll( newCols );

                String cols = TextUtils.join( ",", oldCols );
                query = 
                    String.format( "INSERT INTO %s (%s) SELECT %s from temp_%s",
                                   name, cols, cols, name );
                db.execSQL( query );
            }
            db.execSQL( String.format( "DROP table temp_%s", name ) );
            
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    private void setColumnsEqual( SQLiteDatabase db, String table, 
                                  String dest, String src )
    {
        String query = String.format( "UPDATE %s set %s = %s", table, 
                                      dest, src );
        db.execSQL( query );
    }

    private void forceRowidHigh( SQLiteDatabase db, String name )
    {
        long now = Utils.getCurSeconds();
        // knock 20 years off; whose clock can be that far back?
        now -= 622080000;
        String query = String.format( "INSERT INTO %s (rowid) VALUES (%d)",
                                      name, now );
        db.execSQL( query );
        query = String.format( "DELETE FROM %s", name );
        db.execSQL( query );
    }
}

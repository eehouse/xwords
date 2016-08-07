/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2016 by Eric House (xwords@eehouse.org).  All
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

import org.eehouse.android.xw4.loc.LocUtils;

import java.util.ArrayList;
import java.util.Arrays;

public class DBHelper extends SQLiteOpenHelper {

    public static final String TABLE_NAME_SUM = "summaries";
    public static final String TABLE_NAME_OBITS = "obits";
    public static final String TABLE_NAME_DICTBROWSE = "dictbrowse";
    public static final String TABLE_NAME_DICTINFO = "dictinfo";
    public static final String TABLE_NAME_GROUPS = "groups";
    public static final String TABLE_NAME_STUDYLIST = "study";
    public static final String TABLE_NAME_LOC = "loc";
    public static final String TABLE_NAME_PAIRS = "pairs";
    public static final String TABLE_NAME_INVITES = "invites";
    public static final String TABLE_NAME_CHAT = "chat";
    public static final String TABLE_NAME_LOGS = "logs";
    private static final String DB_NAME = "xwdb";
    private static final int DB_VERSION = 27;

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
    public static final String EXTRAS = "EXTRAS";
    public static final String DICTLANG = "DICTLANG";
    public static final String DICTLIST = "DICTLIST";
    public static final String HASMSGS = "HASMSGS";
    public static final String CONTRACTED = "CONTRACTED";
    public static final String SNAPSHOT = "SNAPSHOT";
    public static final String THUMBNAIL = "THUMBNAIL";
    public static final String CONTYPE = "CONTYPE";
    public static final String SERVERROLE = "SERVERROLE";
    public static final String ROOMNAME = "ROOMNAME";
    // written but never read; can go away
    // public static final String INVITEID = "INVITEID";
    public static final String RELAYID = "RELAYID";
    public static final String SEED = "SEED";
    public static final String SMSPHONE = "SMSPHONE"; // unused -- so far
    public static final String LASTMOVE = "LASTMOVE";
    public static final String NEXTNAG = "NEXTNAG";
    public static final String GROUPID = "GROUPID";
    public static final String NPACKETSPENDING = "NPACKETSPENDING";

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

    public static final String WORD = "WORD";
    public static final String LANGUAGE = "LANGUAGE";

    public static final String KEY = "KEY";
    public static final String VALUE = "VALUE";
    public static final String LOCALE = "LOCALE";
    public static final String BLESSED = "BLESSED";
    public static final String XLATION = "XLATION";

    public static final String ROW = "ROW";
    public static final String MEANS = "MEANS";
    public static final String TARGET = "TARGET";
    public static final String TIMESTAMP = "TIMESTAMP";

    public static final String SENDER = "SENDER";
    public static final String MESSAGE = "MESSAGE";
    public static final String TAG = "TAG";

    private Context m_context;

    private static final String[][] s_summaryColsAndTypes = {
        { "rowid",      "INTEGER PRIMARY KEY AUTOINCREMENT" }
        ,{ VISID,        "INTEGER" } // user-visible ID
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
        ,{ RELAYID,      "TEXT" }
        ,{ SEED,         "INTEGER" }
        ,{ DICTLANG,     "INTEGER" }
        ,{ DICTLIST,     "TEXT" }
        ,{ SMSPHONE,     "TEXT" }  // unused
        ,{ SCORES,       "TEXT" }
        ,{ CHAT_HISTORY, "TEXT" }
        ,{ GAMEID,       "INTEGER" }
        ,{ REMOTEDEVS,   "TEXT" }
        ,{ EXTRAS,       "TEXT" } // json data, most likely
        ,{ LASTMOVE,     "INTEGER DEFAULT 0" }
        ,{ NEXTNAG,      "INTEGER DEFAULT 0" }
        ,{ GROUPID,      "INTEGER" }
        // HASMSGS: sqlite doesn't have bool; use 0 and 1
        ,{ HASMSGS,      "INTEGER DEFAULT 0" }
        ,{ CONTRACTED,   "INTEGER DEFAULT 0" }
        ,{ CREATE_TIME,  "INTEGER" }
        ,{ LASTPLAY_TIME,"INTEGER" }
        ,{ NPACKETSPENDING,"INTEGER" }
        ,{ SNAPSHOT,     "BLOB" }
        ,{ THUMBNAIL,    "BLOB" }
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

    private static final String[][] s_studySchema = {
        { WORD,  "TEXT" }
        ,{ LANGUAGE,  "INTEGER(1)" }
        ,{ "UNIQUE", "(" + WORD + ", " + LANGUAGE + ")" }
    };

    private static final String[][] s_locSchema = {
        { KEY,  "TEXT" }
        ,{ LOCALE,  "TEXT(5)" }
        ,{ BLESSED,  "INTEGER(1)" }
        ,{ XLATION,  "TEXT" }
        ,{ "UNIQUE", "(" + KEY + ", " + LOCALE + "," + BLESSED + ")" }
    };

    private static final String[][] s_pairsSchema = {
        { KEY,  "TEXT" }
        ,{ VALUE,  "TEXT" }
        ,{ "UNIQUE", "(" + KEY + ")" }
    };

    private static final String[][] s_invitesSchema = {
        { ROW, "INTEGER" }
        ,{ TARGET, "TEXT" }
        ,{ MEANS, "INTEGER" }
        ,{ TIMESTAMP, "DATETIME DEFAULT CURRENT_TIMESTAMP" }
    };

    private static final String[][] s_chatsSchema = {
        { ROW, "INTEGER" }
        ,{ SENDER, "INTEGER" }
        ,{ MESSAGE, "TEXT" }
    };

    private static final String[][] s_logsSchema = {
        { TIMESTAMP, "DATETIME DEFAULT CURRENT_TIMESTAMP" },
        { MESSAGE, "TEXT" },
        { TAG, "TEXT" },
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
        createGroupsTable( db, false );
        createStudyTable( db );
        createLocTable( db );
        createPairsTable( db );
        createInvitesTable( db );
        createChatsTable( db );
        createLogsTable( db );
    }

    @Override
    @SuppressWarnings("fallthrough")
    public void onUpgrade( SQLiteDatabase db, int oldVersion, int newVersion )
    {
        DbgUtils.logf( false, "onUpgrade: old: %d; new: %d", oldVersion, newVersion );

        boolean madeSumTable = false;
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
        case 11:
            addSumColumn( db, REMOTEDEVS );
        case 12:
            createTable( db, TABLE_NAME_DICTINFO, s_dictInfoColsAndTypes );
            createTable( db, TABLE_NAME_DICTBROWSE, s_dictBrowseColsAndTypes );
        case 13:
            addSumColumn( db, LASTMOVE );
        case 14:
            addSumColumn( db, GROUPID );
            createGroupsTable( db, true );
        case 15:
            moveToCurGames( db );
        case 16:
            addSumColumn( db, VISID );
            setColumnsEqual( db, TABLE_NAME_SUM, VISID, "rowid" );
            makeAutoincrement( db, TABLE_NAME_SUM, s_summaryColsAndTypes );
            madeSumTable = true;
        case 17:
            if ( !madeSumTable ) {
                // THUMBNAIL also added by makeAutoincrement above
                addSumColumn( db, THUMBNAIL );
            }
        case 18:
            createStudyTable( db );
        case 19:
            if ( !madeSumTable ) {
                // NPACKETSPENDING also added by makeAutoincrement above
                addSumColumn( db, NPACKETSPENDING );
            }
        case 20:
            createLocTable( db );
        case 21:
            createPairsTable( db );
        case 22:
            if ( !madeSumTable ) {
                addSumColumn( db, NEXTNAG );
            }
        case 23:
            if ( !madeSumTable ) {
                addSumColumn( db, EXTRAS );
            }
        case 24:
            createInvitesTable( db );
        case 25:
            createChatsTable( db );
        case 26:
            createLogsTable( db );

            break;
        default:
            db.execSQL( "DROP TABLE " + TABLE_NAME_SUM + ";" );

            TableAndVersion[] tav = new TableAndVersion[] {
                new TableAndVersion( TABLE_NAME_OBITS, 5 ),
                new TableAndVersion( TABLE_NAME_DICTINFO, 12 ),
                new TableAndVersion( TABLE_NAME_DICTBROWSE, 12 ),
                new TableAndVersion( TABLE_NAME_GROUPS, 14 ),
                new TableAndVersion( TABLE_NAME_STUDYLIST, 18 ),
                new TableAndVersion( TABLE_NAME_LOC, 20 ),
                new TableAndVersion( TABLE_NAME_PAIRS, 21 ),
            };
            for ( TableAndVersion entry : tav ) {
                if ( oldVersion >= 1 + entry.addedVersion ) {
                    db.execSQL( "DROP TABLE " + entry.name + ";" );
                }
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

    private void createGroupsTable( SQLiteDatabase db, boolean isUpgrade )
    {
        // Do we have any existing games we'll be grouping?
        if ( isUpgrade ) {
            isUpgrade = 0 < countGames( db );
        }

        createTable( db, TABLE_NAME_GROUPS, s_groupsSchema );

        // Create an empty group name
        ContentValues values = new ContentValues();
        if ( isUpgrade ) {
            values.put( GROUPNAME, LocUtils.getString( m_context, false,
                                                       R.string.group_cur_games) );
            values.put( EXPANDED, 1 );
            long curGroup = db.insert( TABLE_NAME_GROUPS, null, values );

            // place all existing games in the initial unnamed group
            values = new ContentValues();
            values.put( GROUPID, curGroup );
            db.update( DBHelper.TABLE_NAME_SUM, values, null, null );
        }

        values = new ContentValues();
        values.put( GROUPNAME, LocUtils.getString( m_context, false,
                                                   R.string.group_new_games) );
        values.put( EXPANDED, 1 );
        long newGroup = db.insert( TABLE_NAME_GROUPS, null, values );
        XWPrefs.setDefaultNewGameGroup( m_context, newGroup );
    }

    private void createStudyTable( SQLiteDatabase db )
    {
        createTable( db, TABLE_NAME_STUDYLIST, s_studySchema );
    }

    private void createLocTable( SQLiteDatabase db )
    {
        createTable( db, TABLE_NAME_LOC, s_locSchema );
    }

    private void createPairsTable( SQLiteDatabase db )
    {
        createTable( db, TABLE_NAME_PAIRS, s_pairsSchema );
    }

    private void createInvitesTable( SQLiteDatabase db )
    {
        createTable( db, TABLE_NAME_INVITES, s_invitesSchema );
    }

    private void createChatsTable( SQLiteDatabase db )
    {
        createTable( db, TABLE_NAME_CHAT, s_chatsSchema );
    }

    private void createLogsTable( SQLiteDatabase db )
    {
        createTable( db, TABLE_NAME_LOGS, s_logsSchema );
    }

    // Move all existing games to the row previously named "cur games'
    private void moveToCurGames( SQLiteDatabase db )
    {
        String name = LocUtils.getString( m_context, false,
                                          R.string.group_cur_games );
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
            String[] columnNames = getColumns( db, name );
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

    private int countGames( SQLiteDatabase db )
    {
        final String query = "SELECT COUNT(*) FROM " + TABLE_NAME_SUM;

        Cursor cursor = db.rawQuery( query, null );
        cursor.moveToFirst();
        int result = cursor.getInt(0);
        cursor.close();
        return result;
    }

    private static String[] getColumns( SQLiteDatabase db, String name )
    {
        String query = String.format( "SELECT * FROM %s LIMIT 1", name );
        Cursor cursor = db.rawQuery( query, null );
        String[] colNames = cursor.getColumnNames();
        cursor.close();
        return colNames;
    }

    private class TableAndVersion {
        public String name;
        public int addedVersion;
        public TableAndVersion( String nn, int vers ) {
            name = nn; addedVersion = vers;
        }
    }

}

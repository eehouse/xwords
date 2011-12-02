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
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteDatabase;

public class DBHelper extends SQLiteOpenHelper {

    public static final String TABLE_NAME_SUM = "summaries";
    public static final String TABLE_NAME_OBITS = "obits";
    private static final String DB_NAME = "xwdb";
    private static final int DB_VERSION = 10;

    public static final String GAME_NAME = "GAME_NAME";
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
    // GAMEID: this isn't used yet but we'll want it to look up games
    // for which messages arrive.  Add now while changing the DB
    // format
    public static final String GAMEID = "GAMEID";
    public static final String DICTLANG = "DICTLANG";
    public static final String DICTLIST = "DICTLIST";
    public static final String HASMSGS = "HASMSGS";
    public static final String CONTRACTED = "CONTRACTED";
    public static final String SNAPSHOT = "SNAPSHOT";
    public static final String CONTYPE = "CONTYPE";
    public static final String SERVERROLE = "SERVERROLE";
    public static final String ROOMNAME = "ROOMNAME";
    public static final String RELAYID = "RELAYID";
    public static final String SEED = "SEED";
    public static final String SMSPHONE = "SMSPHONE";
    // not used yet
    public static final String CREATE_TIME = "CREATE_TIME";
    // not used yet
    public static final String LASTPLAY_TIME = "LASTPLAY_TIME";


    public DBHelper( Context context )
    {
        super( context, DB_NAME, null, DB_VERSION );
    }

    private void onCreateSum( SQLiteDatabase db ) 
    {
        db.execSQL( "CREATE TABLE " + TABLE_NAME_SUM + " ("
                    + GAME_NAME   + " TEXT,"
                    + NUM_MOVES   + " INTEGER,"
                    + TURN        + " INTEGER,"
                    + GIFLAGS     + " INTEGER,"

                    + NUM_PLAYERS + " INTEGER,"
                    + MISSINGPLYRS + " INTEGER,"
                    + PLAYERS     + " TEXT,"
                    + GAME_OVER   + " INTEGER,"

                    + SERVERROLE + " INTEGER,"
                    + CONTYPE    + " INTEGER,"
                    + ROOMNAME   + " TEXT,"
                    + RELAYID    + " TEXT,"
                    + SEED       + " INTEGER,"
                    + DICTLANG   + " INTEGER,"
                    + DICTLIST   + " TEXT,"

                    + SMSPHONE   + " TEXT,"
                    + SCORES     + " TEXT,"
                    + CHAT_HISTORY   + " TEXT,"
                    + GAMEID     + " INTEGER,"
                    // HASMSGS: sqlite doesn't have bool; use 0 and 1
                    + HASMSGS    + " INTEGER DEFAULT 0,"
                    + CONTRACTED + " INTEGER DEFAULT 0,"

                    + CREATE_TIME + " INTEGER,"
                    + LASTPLAY_TIME + " INTEGER,"

                    + SNAPSHOT   + " BLOB"
                    + ");" );
    }

    private void onCreateObits( SQLiteDatabase db ) 
    {
        db.execSQL( "CREATE TABLE " + TABLE_NAME_OBITS + " ("
                    + RELAYID    + " TEXT,"
                    + SEED       + " INTEGER"
                    + ");" );
    }

    @Override
    public void onCreate( SQLiteDatabase db ) 
    {
        onCreateSum( db );
        onCreateObits( db );
    }

    @Override
    public void onUpgrade( SQLiteDatabase db, int oldVersion, int newVersion ) 
    {
        DbgUtils.logf( "onUpgrade: old: %d; new: %d", oldVersion, newVersion );

        switch( oldVersion ) {
        case 5:
            onCreateObits(db);
        case 6:
            db.execSQL( "ALTER TABLE " + TABLE_NAME_SUM  +
                        " ADD COLUMN " + TURN + " INTEGER;" );
            db.execSQL( "ALTER TABLE " + TABLE_NAME_SUM  +
                        " ADD COLUMN " + GIFLAGS + " INTEGER;" );
            db.execSQL( "ALTER TABLE " + TABLE_NAME_SUM  +
                        " ADD COLUMN " + CHAT_HISTORY + " TEXT;" );
        case 7:
            db.execSQL( "ALTER TABLE " + TABLE_NAME_SUM  +
                        " ADD COLUMN " + MISSINGPLYRS + " INTEGER;" );
        case 8:
            db.execSQL( "ALTER TABLE " + TABLE_NAME_SUM  +
                        " ADD COLUMN " + GAME_NAME + " TEXT;" );
            db.execSQL( "ALTER TABLE " + TABLE_NAME_SUM  +
                        " ADD COLUMN " + CONTRACTED + " INTEGER;" );
        case 9:
            db.execSQL( "ALTER TABLE " + TABLE_NAME_SUM  +
                        " ADD COLUMN " + DICTLIST + " TEXT;" );
        case 10:
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
}

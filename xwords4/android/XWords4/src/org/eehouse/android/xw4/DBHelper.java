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
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteDatabase;

public class DBHelper extends SQLiteOpenHelper {

    public static final String TABLE_NAME = "summaries";
    private static final String DB_NAME = "xwdb";
    private static final int DB_VERSION = 3;

    public static final String FILE_NAME = "FILE_NAME";
    public static final String NUM_MOVES = "NUM_MOVES";
    public static final String GAME_OVER = "GAME_OVER";
    public static final String SCORES = "SCORES";
    // GAMEID: this isn't used yet but we'll want it to look up games
    // for which messages arrive.  Add now while changing the DB
    // format
    public static final String GAMEID = "GAMEID";
    public static final String SNAPSHOT = "SNAPSHOT";
    public static final String CONTYPE = "CONTYPE";
    public static final String ROOMNAME = "ROOMNAME";
    public static final String SMSPHONE = "SMSPHONE";
    // not used yet
    public static final String CREATE_TIME = "CREATE_TIME";
    // not used yet
    public static final String LASTPLAY_TIME = "LASTPLAY_TIME";


    public DBHelper( Context context )
    {
        super( context, DB_NAME, null, DB_VERSION );
    }

    @Override
    public void onCreate( SQLiteDatabase db ) 
    {
        db.execSQL( "CREATE TABLE " + TABLE_NAME + " ("
                    + FILE_NAME + " TEXT PRIMARY KEY,"
                    + NUM_MOVES + " INTEGER,"
                    + GAME_OVER + " INTEGER,"

                    + CONTYPE    + " INTEGER,"
                    + ROOMNAME   + " TEXT,"
                    + SMSPHONE   + " TEXT,"
                    + SCORES     + " TEXT,"
                    + GAMEID     + " INTEGER,"

                    + CREATE_TIME + " INTEGER,"
                    + LASTPLAY_TIME + " INTEGER,"

                    + SNAPSHOT   + " BLOB"
                    + ");" );
    }

    @Override
    public void onUpgrade( SQLiteDatabase db, int oldVersion, int newVersion ) 
    {
        Utils.logf( "onUpgrade: old: %d; new: %d", oldVersion, newVersion );
        db.execSQL( "DROP TABLE " + TABLE_NAME + ";" );
        onCreate( db );
    }
}

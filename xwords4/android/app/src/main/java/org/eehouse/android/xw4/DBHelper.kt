/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All
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
package org.eehouse.android.xw4

import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteException
import android.database.sqlite.SQLiteOpenHelper
import android.text.TextUtils
import org.eehouse.android.xw4.BuildConfig.DB_NAME
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils
import java.util.Arrays


class DBHelper(private val mContext: Context) :
    SQLiteOpenHelper(mContext, DB_NAME, null, DB_VERSION) {
    enum class TABLE_NAMES(
        private val mName: String,
        val mSchema: Array<Array<String>>?,
        private val mAddedVersion: Int
    ) {
        SUM(
            "summaries", arrayOf(
                arrayOf("rowid", "INTEGER PRIMARY KEY AUTOINCREMENT"),
                arrayOf(VISID, "INTEGER"),
                arrayOf(
                    GAME_NAME, "TEXT"
                ),
                arrayOf(NUM_MOVES, "INTEGER"),
                arrayOf(TURN, "INTEGER"),
                arrayOf(
                    TURN_LOCAL, "INTEGER"
                ),
                arrayOf(GIFLAGS, "INTEGER"),
                arrayOf(NUM_PLAYERS, "INTEGER"),
                arrayOf(
                    MISSINGPLYRS, "INTEGER"
                ),
                arrayOf(PLAYERS, "TEXT"),
                arrayOf(GAME_OVER, "INTEGER"),
                arrayOf(
                    QUASHED, "INTEGER"
                ),
                arrayOf(CAN_REMATCH, "INTEGER(1) default 0"),
                arrayOf(
                    SERVERROLE, "INTEGER"
                ),
                arrayOf(CONTYPE, "INTEGER"),
                arrayOf(ROOMNAME, "TEXT"),
                arrayOf(
                    RELAYID, "TEXT"
                ),
                arrayOf(SEED, "INTEGER"),
                arrayOf(DICTLANG, "INTEGER"),
                arrayOf(
                    ISOCODE, "TEXT(8)"
                ),
                arrayOf(LANGNAME, "TEXT"),
                arrayOf(DICTLIST, "TEXT"),
                arrayOf(
                    SMSPHONE, "TEXT"
                ),
                arrayOf(SCORES, "TEXT"),
                arrayOf(CHAT_HISTORY, "TEXT"),
                arrayOf(
                    GAMEID, "INTEGER"
                ),
                arrayOf(REMOTEDEVS, "TEXT"),
                arrayOf(EXTRAS, "TEXT"),
                arrayOf(
                    LASTMOVE, "INTEGER DEFAULT 0"
                ),
                arrayOf(NEXTDUPTIMER, "INTEGER DEFAULT 0"),
                arrayOf(
                    NEXTNAG, "INTEGER DEFAULT 0"
                ),
                arrayOf(GROUPID, "INTEGER"),
                arrayOf(HASMSGS, "INTEGER DEFAULT 0"),
                arrayOf(
                    CONTRACTED, "INTEGER DEFAULT 0"
                ),
                arrayOf(CREATE_TIME, "INTEGER"),
                arrayOf(
                    LASTPLAY_TIME, "INTEGER"
                ),
                arrayOf(NPACKETSPENDING, "INTEGER"),
                arrayOf(SNAPSHOT, "BLOB"),
                arrayOf(
                    THUMBNAIL, "BLOB"
                )
            ), 0
        ),
        _OBITS("obits", null, 5),
        DICTBROWSE(
            "dictbrowse", arrayOf(
                arrayOf(DICTNAME, "TEXT"), arrayOf(LOCATION, "UNSIGNED INTEGER(1)"), arrayOf(
                    WORDCOUNTS, "TEXT"
                ), arrayOf(ITERMIN, "INTEGER(4)"), arrayOf(ITERMAX, "INTEGER(4)"), arrayOf(
                    ITERPOS, "INTEGER"
                ), arrayOf(ITERTOP, "INTEGER"), arrayOf(ITERPREFIX, "TEXT")
            ), 12
        ),
        DICTINFO(
            "dictinfo", arrayOf(
                arrayOf(LOCATION, "UNSIGNED INTEGER(1)"), arrayOf(DICTNAME, "TEXT"), arrayOf(
                    MD5SUM, "TEXT(32)"
                ), arrayOf(FULLSUM, "TEXT(32)"), arrayOf(WORDCOUNT, "INTEGER"), arrayOf(
                    LANGCODE, "INTEGER"
                ), arrayOf(LANGNAME, "TEXT"), arrayOf(ISOCODE, "TEXT(8)"), arrayOf(
                    ON_SERVER, "INTEGER DEFAULT 0"
                )
            ), 12
        ),
        GROUPS("groups", arrayOf(arrayOf(GROUPNAME, "TEXT"), arrayOf(EXPANDED, "INTEGER(1)")), 14),
        STUDYLIST(
            "study", arrayOf(
                arrayOf(WORD, "TEXT"), arrayOf(LANGUAGE, "INTEGER(1)"), arrayOf(
                    ISOCODE, "TEXT(8)"
                ), arrayOf("UNIQUE", "(" + WORD + ", " + ISOCODE + ")")
            ), 18
        ),
        LOC(
            "loc", arrayOf(
                arrayOf(KEY, "TEXT"),
                arrayOf(LOCALE, "TEXT(5)"),
                arrayOf(BLESSED, "INTEGER(1)"),
                arrayOf(
                    XLATION, "TEXT"
                ),
                arrayOf("UNIQUE", "(" + KEY + ", " + LOCALE + "," + BLESSED + ")")
            ), 20
        ),
        PAIRS(
            "pairs",
            arrayOf(
                arrayOf(KEY, "TEXT"),
                arrayOf(VALUE, "TEXT"),
                arrayOf("UNIQUE", "(" + KEY + ")")
            ),
            21
        ),
        INVITES(
            "invites", arrayOf(
                arrayOf(ROW, "INTEGER"),
                arrayOf(TARGET, "TEXT"),
                arrayOf(MEANS, "INTEGER"),
                arrayOf(
                    TIMESTAMP, "DATETIME DEFAULT CURRENT_TIMESTAMP"
                )
            ), 24
        ),
        CHAT(
            "chat", arrayOf(
                arrayOf(ROW, "INTEGER"),
                arrayOf(SENDER, "INTEGER"),
                arrayOf(MESSAGE, "TEXT"),
                arrayOf(
                    CHATTIME, "INTEGER DEFAULT 0"
                )
            ), 25
        ),
        LOGS(
            "logs", arrayOf(
                arrayOf(TIMESTAMP, "DATETIME DEFAULT CURRENT_TIMESTAMP"), arrayOf(
                    MESSAGE, "TEXT"
                ), arrayOf(TAGG, "TEXT")
            ), 26
        );

        override fun toString(): String {
            return mName
        }

        internal fun addedVersion(): Int {
            return mAddedVersion
        }
    }

    override fun onCreate(db: SQLiteDatabase) {
        createTable(db, TABLE_NAMES.SUM)
        createTable(db, TABLE_NAMES.DICTINFO)
        createTable(db, TABLE_NAMES.DICTBROWSE)
        forceRowidHigh(db, TABLE_NAMES.SUM)
        createGroupsTable(db, false)
        createStudyTable(db)
        createLocTable(db)
        createPairsTable(db)
        createInvitesTable(db)
        createChatsTable(db)
        createLogsTable(db)
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        Log.i(TAG, "onUpgrade(%s): old: %d; new: %d", db, oldVersion, newVersion)
        Assert.assertTrueNR(newVersion == DB_VERSION)
        upgradeImpl(db, oldVersion, false, false, false, false)
    }

    private tailrec fun upgradeImpl(
        db: SQLiteDatabase, oldVersion: Int,
        madeSumTable: Boolean, madeChatTable: Boolean,
        madeDITable: Boolean, madeStudyTable: Boolean
    ) {
        var madeSumTable = madeSumTable
        var madeChatTable = madeChatTable
        var madeDITable = madeDITable
        var madeStudyTable = madeStudyTable
        when (oldVersion) {
            6 -> {
                addSumColumn(db, TURN)
                addSumColumn(db, GIFLAGS)
                addSumColumn(db, CHAT_HISTORY)
            }

            7 -> addSumColumn(db, MISSINGPLYRS)
            8 -> {
                addSumColumn(db, GAME_NAME)
                addSumColumn(db, CONTRACTED)
            }

            9 -> addSumColumn(db, DICTLIST)
            10 -> {}
            11 -> addSumColumn(db, REMOTEDEVS)
            12 -> {
                createTable(db, TABLE_NAMES.DICTINFO)
                createTable(db, TABLE_NAMES.DICTBROWSE)
                madeDITable = true
            }

            13 -> addSumColumn(db, LASTMOVE)
            14 -> {
                addSumColumn(db, GROUPID)
                createGroupsTable(db, true)
            }

            15 -> moveToCurGames(db)
            16 -> {
                addSumColumn(db, VISID)
                setColumnsEqual(db, TABLE_NAMES.SUM, VISID, "rowid")
                makeAutoincrement(db, TABLE_NAMES.SUM)
                madeSumTable = true
            }

            17 -> if (!madeSumTable) {
					  // THUMBNAIL also added by makeAutoincrement above
					  addSumColumn(db, THUMBNAIL)
				  }

            18 -> {
                createStudyTable(db)
                madeStudyTable = true
            }

            19 -> if (!madeSumTable) {
					  // NPACKETSPENDING also added by makeAutoincrement above
					  addSumColumn(db, NPACKETSPENDING)
				  }

            20 -> createLocTable(db)
            21 -> createPairsTable(db)
            22 -> if (!madeSumTable) {
					  addSumColumn(db, NEXTNAG)
				  }

            23 -> if (!madeSumTable) {
					  addSumColumn(db, EXTRAS)
				  }

            24 -> createInvitesTable(db)
            25 -> {
                createChatsTable(db)
                madeChatTable = true
            }

            26 -> createLogsTable(db)
            27 -> if (!madeSumTable) {
					  addSumColumn(db, TURN_LOCAL)
				  }

            28 -> if (!madeChatTable) {
					  addColumn(db, TABLE_NAMES.CHAT, CHATTIME)
				  }

            29 -> if (!madeSumTable) {
					  addSumColumn(db, NEXTDUPTIMER)
				  }

            30 -> if (!madeDITable) {
					  addColumn(db, TABLE_NAMES.DICTINFO, FULLSUM)
				  }

            31 -> {
                if (!madeSumTable) {
                    addSumColumn(db, ISOCODE)
                }
                langCodeToISOCode(db, TABLE_NAMES.SUM, DICTLANG, ISOCODE)
                if (!madeStudyTable) {
                    addColumn(db, TABLE_NAMES.STUDYLIST, ISOCODE)
                }
                langCodeToISOCode(db, TABLE_NAMES.STUDYLIST, LANGUAGE, ISOCODE)
                if (!madeDITable) {
                    addColumn(db, TABLE_NAMES.DICTINFO, ISOCODE)
                    addColumn(db, TABLE_NAMES.DICTINFO, LANGNAME)
                }
                langCodeToISOCode(db, TABLE_NAMES.DICTINFO, LANGCODE, ISOCODE)
                try {
                    db.execSQL("DROP TABLE IF EXISTS " + TABLE_NAMES._OBITS + ";")
                } catch (ex: SQLiteException) {
                    Log.ex(TAG, ex)
                }
            }

            32 -> if (!madeDITable) {
					  addColumn(db, TABLE_NAMES.DICTINFO, ON_SERVER)
				  }

            33 -> if (!madeSumTable) {
					  addColumn(db, TABLE_NAMES.SUM, QUASHED)
				  }

            34 -> {
                if (!madeSumTable) {
                    addColumn(db, TABLE_NAMES.SUM, CAN_REMATCH)
                }
                return
            }

            else -> {
				for (table in TABLE_NAMES.entries) {
					if (oldVersion >= 1 + table.addedVersion()) {
						db.execSQL("DROP TABLE $table;")
					}
				}
				onCreate(db)
			}
        }
        upgradeImpl(
            db, oldVersion + 1, madeSumTable, madeChatTable,
            madeDITable, madeStudyTable
        )
    }

    private fun langCodeToISOCode(
        db: SQLiteDatabase, table: TABLE_NAMES,
        oldIntCol: String, newIsoStringCol: String
    ) {
        try {
            db.beginTransaction()
            val columns = arrayOf(oldIntCol)
            val groupBy = columns[0]

            // First gather all the lang codes
            val map: MutableMap<Int, ISOCode> = HashMap()
            val cursor = db.query(
                table.toString(),
                columns, null, null, groupBy, null, null
            )
            val colIndex = cursor.getColumnIndex(columns[0])
            while (cursor.moveToNext()) {
                val code = cursor.getInt(colIndex)
                val isoCode = XwJNI.lcToLocaleJ(code)
                map[code] = isoCode!!
                Log.d(TAG, "added %d => %s", code, isoCode)
            }

            // Then update the DB
            for (code in map.keys) {
                val sb = StringBuffer()
                    .append("Update ").append(table)
                    .append(" SET ").append(newIsoStringCol).append(" = '").append(map[code])
                    .append("'")
                    .append(" WHERE ").append(oldIntCol).append(" = ").append(code)
                    .append(";")
                val query = sb.toString()
                // Log.d( TAG, "langCodeToISOCode() query: %s", query );
                db.execSQL(query)
            }
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }

    private fun addSumColumn(db: SQLiteDatabase, colName: String) {
        addColumn(db, TABLE_NAMES.SUM, colName)
    }

    private fun addColumn(
        db: SQLiteDatabase, tableName: TABLE_NAMES,
        colName: String
    ) {
        val colsAndTypes = tableName.mSchema
        var colType: String? = null
        for (ii in colsAndTypes!!.indices) {
            if (colsAndTypes[ii][0] == colName) {
                colType = colsAndTypes[ii][1]
                break
            }
        }
        val cmd = String.format(
            "ALTER TABLE %s ADD COLUMN %s %s;",
            tableName, colName, colType
        )
        db.execSQL(cmd)
    }

    private fun createTable(db: SQLiteDatabase, name: TABLE_NAMES) {
        val data = name.mSchema
        val query = StringBuilder(String.format("CREATE TABLE %s (", name))
        for (ii in data!!.indices) {
            val col = String.format(" %s %s,", data[ii][0], data[ii][1])
            query.append(col)
        }
        query.setLength(query.length - 1) // nuke the last comma
        query.append(");")
        db.execSQL(query.toString())
    }

    private fun createGroupsTable(db: SQLiteDatabase, isUpgrade: Boolean) {
        // Do we have any existing games we'll be grouping?
        var isUpgrade = isUpgrade
        if (isUpgrade) {
            isUpgrade = 0 < countGames(db)
        }
        createTable(db, TABLE_NAMES.GROUPS)

        // Create an empty group name
        var values = ContentValues()
        if (isUpgrade) {
            values.put(
                GROUPNAME, LocUtils.getString(
                    mContext, false,
                    R.string.group_cur_games
                )
            )
            values.put(EXPANDED, 1)
            val curGroup = insert(db, TABLE_NAMES.GROUPS, values)

            // place all existing games in the initial unnamed group
            values = ContentValues()
            values.put(GROUPID, curGroup)
            db.update(TABLE_NAMES.SUM.toString(), values, null, null)
        }
        values = ContentValues()
        values.put(
            GROUPNAME, LocUtils.getString(
                mContext, false,
                R.string.group_new_games
            )
        )
        values.put(EXPANDED, 1)
        val newGroup = insert(db, TABLE_NAMES.GROUPS, values)
        XWPrefs.setDefaultNewGameGroup(mContext, newGroup)
    }

    private fun createStudyTable(db: SQLiteDatabase) {
        createTable(db, TABLE_NAMES.STUDYLIST)
    }

    private fun createLocTable(db: SQLiteDatabase) {
        createTable(db, TABLE_NAMES.LOC)
    }

    private fun createPairsTable(db: SQLiteDatabase) {
        createTable(db, TABLE_NAMES.PAIRS)
    }

    private fun createInvitesTable(db: SQLiteDatabase) {
        createTable(db, TABLE_NAMES.INVITES)
    }

    private fun createChatsTable(db: SQLiteDatabase) {
        createTable(db, TABLE_NAMES.CHAT)
    }

    private fun createLogsTable(db: SQLiteDatabase) {
        createTable(db, TABLE_NAMES.LOGS)
    }

    // Move all existing games to the row previously named "cur games'
    private fun moveToCurGames(db: SQLiteDatabase) {
        val name = LocUtils.getString(
            mContext, false,
            R.string.group_cur_games
        )
        val columns = arrayOf("rowid")
        val selection = String.format("%s = '%s'", GROUPNAME, name)
        val cursor = query(db, TABLE_NAMES.GROUPS, columns, selection)
        if (1 == cursor.count && cursor.moveToFirst()) {
            val rowid = cursor.getLong(cursor.getColumnIndex("rowid"))
            val values = ContentValues()
            values.put(GROUPID, rowid)
            update(db, TABLE_NAMES.SUM, values, null)
        }
        cursor.close()
    }

    private fun makeAutoincrement(db: SQLiteDatabase, name: TABLE_NAMES) {
        val data = name.mSchema
        db.beginTransaction()
        try {
            var query: String
            val columnNames = getColumns(db, name)

            query = String.format(
                "ALTER table %s RENAME TO 'temp_%s'",
                name, name
            )
            db.execSQL(query)

            createTable(db, name)
            forceRowidHigh(db, name)

            val oldCols = ArrayList(Arrays.asList(*columnNames))

            // Make a list of columns in the new DB, using it to
            // remove from the old list any that aren't in the
            // new.  Old tables may have column names we no longer
            // use, but we can't try to copy them because the new
            // doesn't have 'em. Note that calling getColumns() on
            // the newly-created table doesn't work, perhaps
            // because we're in a transaction and nothing's been
            // committed.
            val newCols = ArrayList<String?>()
            for (ii in data!!.indices) {
                newCols.add(data[ii][0])
            }
            oldCols.retainAll(newCols)
            val cols = TextUtils.join(",", oldCols)
            query = String.format(
                "INSERT INTO %s (%s) SELECT %s from temp_%s",
                name, cols, cols, name
            )
            db.execSQL(query)

            db.execSQL(String.format("DROP table temp_%s", name))
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }

    private fun setColumnsEqual(
        db: SQLiteDatabase, table: TABLE_NAMES,
        dest: String, src: String
    ) {
        val query = String.format(
            "UPDATE %s set %s = %s", table,
            dest, src
        )
        db.execSQL(query)
    }

    private fun forceRowidHigh(db: SQLiteDatabase, name: TABLE_NAMES) {
        var now = Utils.getCurSeconds()
        // knock 20 years off; whose clock can be that far back?
        now -= 622080000
        var query = String.format(
            "INSERT INTO %s (rowid) VALUES (%d)",
            name, now
        )
        db.execSQL(query)
        query = String.format("DELETE FROM %s", name)
        db.execSQL(query)
    }

    private fun countGames(db: SQLiteDatabase): Int {
        val query = "SELECT COUNT(*) FROM " + TABLE_NAMES.SUM
        val cursor = db.rawQuery(query, null)
        cursor.moveToFirst()
        val result = cursor.getInt(0)
        cursor.close()
        return result
    }

    companion object {
        private val TAG = DBHelper::class.java.getSimpleName()
        private const val DB_VERSION = 35
        const val GAME_NAME = "GAME_NAME"
        const val VISID = "VISID"
        const val NUM_MOVES = "NUM_MOVES"
        const val TURN = "TURN"
        const val TURN_LOCAL = "TURN_LOCAL"
        const val GIFLAGS = "GIFLAGS"
        const val PLAYERS = "PLAYERS"
        const val NUM_PLAYERS = "NUM_PLAYERS"
        const val MISSINGPLYRS = "MISSINGPLYRS"
        const val GAME_OVER = "GAME_OVER"
        const val QUASHED = "QUASHED"
        const val IN_USE = "IN_USE"
        const val SCORES = "SCORES"
        const val CHAT_HISTORY = "CHAT_HISTORY"
        const val GAMEID = "GAMEID"
        const val REMOTEDEVS = "REMOTEDEVS"
        const val EXTRAS = "EXTRAS"
        private const val DICTLANG = "DICTLANG"
        const val ISOCODE = "ISOCODE"
        const val ON_SERVER = "ON_SERVER"
        const val LANGNAME = "LANGNAME"
        const val DICTLIST = "DICTLIST"
        const val HASMSGS = "HASMSGS"
        const val CONTRACTED = "CONTRACTED"
        const val SNAPSHOT = "SNAPSHOT"
        const val THUMBNAIL = "THUMBNAIL"
        const val CONTYPE = "CONTYPE"
        const val SERVERROLE = "SERVERROLE"
        const val ROOMNAME = "ROOMNAME"

        // written but never read; can go away
        // public static final String INVITEID = "INVITEID";
        const val RELAYID = "RELAYID"
        const val SEED = "SEED"
        const val SMSPHONE = "SMSPHONE" // unused -- so far
        const val LASTMOVE = "LASTMOVE"
        const val NEXTDUPTIMER = "NEXTDUPTIMER"
        const val NEXTNAG = "NEXTNAG"
        const val GROUPID = "GROUPID"
        const val NPACKETSPENDING = "NPACKETSPENDING"
        const val CAN_REMATCH = "CAN_REMATCH"
        const val DICTNAME = "DICTNAME"
        const val MD5SUM = "MD5SUM"
        const val FULLSUM = "FULLSUM"
        const val WORDCOUNT = "WORDCOUNT"
        const val WORDCOUNTS = "WORDCOUNTS"
        private const val LANGCODE = "LANGCODE"
        const val LOCATION = "LOC"
        const val ITERMIN = "ITERMIN"
        const val ITERMAX = "ITERMAX"
        const val ITERPOS = "ITERPOS"
        const val ITERTOP = "ITERTOP"
        const val ITERPREFIX = "ITERPREFIX"
        const val CREATE_TIME = "CREATE_TIME"
        const val LASTPLAY_TIME = "LASTPLAY_TIME"
        const val CHATTIME = "CHATTIME"
        const val GROUPNAME = "GROUPNAME"
        const val EXPANDED = "EXPANDED"
        const val WORD = "WORD"
        private const val LANGUAGE = "LANGUAGE"
        const val KEY = "KEY"
        const val VALUE = "VALUE"
        const val LOCALE = "LOCALE"
        const val BLESSED = "BLESSED"
        const val XLATION = "XLATION"
        const val ROW = "ROW"
        const val MEANS = "MEANS"
        const val TARGET = "TARGET"
        const val TIMESTAMP = "TIMESTAMP"
        const val SENDER = "SENDER"
        const val MESSAGE = "MESSAGE"

		@JvmStatic
		fun getDBName(): String {
			return DB_NAME
		}

        // TAG is a thing in Android; don't wear it out
        const val TAGG = "TAG"
        private fun getColumns(db: SQLiteDatabase, name: TABLE_NAMES): Array<String> {
            val query = String.format("SELECT * FROM %s LIMIT 1", name)
            val cursor = db.rawQuery(query, null)
            val colNames = cursor.columnNames
            cursor.close()
            return colNames
        }

        @JvmOverloads
        fun query(
            db: SQLiteDatabase?, table: TABLE_NAMES, columns: Array<String>?,
            selection: String?, orderBy: String? = null
        ): Cursor {
            return db?.query(
                table.toString(), columns, selection,
                null, null, null, orderBy
            )!!
        }

        fun update(
            db: SQLiteDatabase?, table: TABLE_NAMES, values: ContentValues?,
            selection: String?
        ): Int {
            // returns number of rows impacted
            return db!!.update(table.toString(), values, selection, null)
        }

        fun insert(db: SQLiteDatabase?, table: TABLE_NAMES, values: ContentValues?): Long {
            return db!!.insert(table.toString(), null, values)
        }
    }
}

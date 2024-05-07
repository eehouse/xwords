/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
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
package org.eehouse.android.xw4

import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.database.sqlite.SQLiteConstraintException
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import android.graphics.Bitmap
import android.graphics.Bitmap.CompressFormat
import android.graphics.BitmapFactory
import android.os.Environment
import android.text.TextUtils
import org.eehouse.android.xw4.DBHelper.TABLE_NAMES
import org.eehouse.android.xw4.DictUtils.DictAndLoc
import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.DictUtils.ON_SERVER
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.jni.DictInfo
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.io.Serializable
import java.util.Date
import java.util.StringTokenizer


object DBUtils {
    private val TAG = DBUtils::class.java.getSimpleName()
    const val ROWID_NOTFOUND = -1
    const val ROWIDS_ALL = -2
    const val GROUPID_UNSPEC = -1
    const val KEY_NEWGAMECOUNT = "DBUtils.newGameCount"

    // how many log rows to keep? (0 means off)
    private const val LOGLIMIT = 0
    private const val DICTS_SEP = ","
    private const val ROW_ID = "rowid"
    private const val ROW_ID_FMT = "rowid=%d"
    private const val NAME_FMT = "%s='%s'"
    private var s_cachedRowID = ROWID_NOTFOUND.toLong()
    private var s_cachedBytes: ByteArray? = null
    private val s_listeners = HashSet<DBChangeListener>()
    private val s_slListeners: MutableSet<StudyListListener> = HashSet()
    private var s_dbHelper: SQLiteOpenHelper? = null
    private var s_db: SQLiteDatabase? = null
    @JvmStatic
    fun getSummary(
        context: Context,
        lock: GameLock
    ): GameSummary? {
        val startMS = System.currentTimeMillis()
        initDB(context)
        var summary: GameSummary? = null
        val columns = arrayOf(
            ROW_ID,
            DBHelper.NUM_MOVES, DBHelper.NUM_PLAYERS,
            DBHelper.MISSINGPLYRS,
            DBHelper.GAME_OVER, DBHelper.QUASHED, DBHelper.PLAYERS,
            DBHelper.TURN, DBHelper.TURN_LOCAL, DBHelper.GIFLAGS,
            DBHelper.CONTYPE, DBHelper.SERVERROLE,
            DBHelper.ROOMNAME, DBHelper.RELAYID,  /*DBHelper.SMSPHONE,*/
            DBHelper.SEED,
            DBHelper.ISOCODE, DBHelper.GAMEID,
            DBHelper.SCORES,
            DBHelper.LASTPLAY_TIME, DBHelper.REMOTEDEVS,
            DBHelper.LASTMOVE, DBHelper.NPACKETSPENDING,
            DBHelper.EXTRAS, DBHelper.NEXTDUPTIMER,
            DBHelper.CREATE_TIME, DBHelper.CAN_REMATCH
        )
        val selection = String.format(ROW_ID_FMT, lock.rowid)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (1 == cursor.count && cursor.moveToFirst()) {
                summary = GameSummary()
                summary!!.nMoves = cursor
                    .getInt(cursor.getColumnIndex(DBHelper.NUM_MOVES))
                summary!!.nPlayers = cursor.getInt(cursor.getColumnIndex(DBHelper.NUM_PLAYERS))
                summary!!.missingPlayers =
                    cursor.getInt(cursor.getColumnIndex(DBHelper.MISSINGPLYRS))
                summary!!.setPlayerSummary(cursor.getString(cursor.getColumnIndex(DBHelper.PLAYERS)))
                summary!!.turn = cursor.getInt(cursor.getColumnIndex(DBHelper.TURN))
                summary!!.turnIsLocal =
                    0 != cursor.getInt(cursor.getColumnIndex(DBHelper.TURN_LOCAL))
                summary!!.setGiFlags(
                    cursor.getInt(cursor.getColumnIndex(DBHelper.GIFLAGS))
                )
                summary!!.gameID = cursor.getInt(cursor.getColumnIndex(DBHelper.GAMEID))
                val players = cursor.getString(cursor.getColumnIndex(DBHelper.PLAYERS))
                summary!!.readPlayers(context, players)

                // isoCode will be null when game first created
                summary!!.isoCode =
                    ISOCode.newIf(cursor.getString(cursor.getColumnIndex(DBHelper.ISOCODE)))
                summary!!.modtime = cursor.getLong(cursor.getColumnIndex(DBHelper.LASTPLAY_TIME))
                var tmpInt = cursor.getInt(cursor.getColumnIndex(DBHelper.GAME_OVER))
                summary!!.gameOver = tmpInt != 0
                tmpInt = cursor.getInt(cursor.getColumnIndex(DBHelper.QUASHED))
                summary!!.quashed = tmpInt != 0
                summary!!.lastMoveTime = cursor.getInt(cursor.getColumnIndex(DBHelper.LASTMOVE))
                summary!!.dupTimerExpires =
                    cursor.getInt(cursor.getColumnIndex(DBHelper.NEXTDUPTIMER))
                summary!!.created = cursor
                    .getLong(cursor.getColumnIndex(DBHelper.CREATE_TIME))
                tmpInt = cursor.getInt(cursor.getColumnIndex(DBHelper.CAN_REMATCH))
                summary!!.canRematch = 0 != 1 and tmpInt
                val str = cursor
                    .getString(cursor.getColumnIndex(DBHelper.EXTRAS))
                summary!!.extras = str
                val scoresStr = cursor.getString(cursor.getColumnIndex(DBHelper.SCORES))
                val scores = IntArray(summary!!.nPlayers)
                if (null != scoresStr && scoresStr.length > 0) {
                    val st = StringTokenizer(scoresStr)
                    for (ii in scores.indices) {
                        Assert.assertTrue(st.hasMoreTokens())
                        val token = st.nextToken()
                        scores[ii] = token.toInt()
                    }
                } else {
                    for (ii in scores.indices) {
                        scores[ii] = 0
                    }
                }
                summary!!.scores = scores
                var col = cursor.getColumnIndex(DBHelper.CONTYPE)
                if (0 <= col) {
                    tmpInt = cursor.getInt(col)
                    summary!!.conTypes = CommsConnTypeSet(tmpInt)
                    col = cursor.getColumnIndex(DBHelper.SEED)
                    if (0 < col) {
                        summary!!.seed = cursor.getInt(col)
                    }
                    col = cursor.getColumnIndex(DBHelper.NPACKETSPENDING)
                    if (0 <= col) {
                        summary!!.nPacketsPending = cursor.getInt(col)
                    }
                    val iter: Iterator<CommsConnType> = summary!!.conTypes.iterator()
                    while (iter.hasNext()) {
                        val typ = iter.next()
                        when (typ) {
                            CommsConnType.COMMS_CONN_RELAY -> {
                                // Can't do this: there are still some relay games
                                // on my devices anyway
                                // Assert.failDbg();
                                col = cursor.getColumnIndex(DBHelper.ROOMNAME)
                                if (col >= 0) {
                                    summary!!.roomName = cursor.getString(col)
                                }
                                col = cursor.getColumnIndex(DBHelper.RELAYID)
                                if (col >= 0) {
                                    summary!!.relayID = cursor.getString(col)
                                }
                            }

                            CommsConnType.COMMS_CONN_BT, CommsConnType.COMMS_CONN_SMS -> {
                                col = cursor.getColumnIndex(DBHelper.REMOTEDEVS)
                                if (col >= 0) {
                                    summary!!.setRemoteDevs(
                                        context, typ,
                                        cursor.getString(col)
                                    )
                                }
                            }
                            else -> Log.d( TAG, "unexpected typ $typ")
                        }
                    }
                }
                col = cursor.getColumnIndex(DBHelper.SERVERROLE)
                tmpInt = cursor.getInt(col)
                summary!!.serverRole = DeviceRole.entries[tmpInt]
            }
            cursor.close()
        }
        if (null == summary && lock.canWrite()) {
            summary = GameUtils.summarize(context, lock)
        }
        val endMS = System.currentTimeMillis()

        // Might want to be cacheing this...
        val elapsed = endMS - startMS
        if (elapsed > 10) {
            Log.d(
                TAG, "getSummary(rowid=%d) => %s (took>10: %dms)",
                lock.rowid, summary, elapsed
            )
        }
        return summary
    } // getSummary

    @JvmStatic
    fun saveSummary(
        context: Context, lock: GameLock,
        summary: GameSummary?
    ) {
        Assert.assertTrue(lock.canWrite())
        val rowid = lock.rowid
        val selection = String.format(ROW_ID_FMT, rowid)
        var values: ContentValues? = null
        if (null != summary) {
            values = ContentValues()
				.putAnd(DBHelper.NUM_MOVES, summary.nMoves)
				.putAnd(DBHelper.NUM_PLAYERS, summary.nPlayers)
				.putAnd(DBHelper.MISSINGPLYRS, summary.missingPlayers)
				.putAnd(DBHelper.TURN, summary.turn)
				.putAnd(DBHelper.TURN_LOCAL, if (summary.turnIsLocal) 1 else 0)
				.putAnd(DBHelper.GIFLAGS, summary.giflags())
				.putAnd(DBHelper.PLAYERS,summary.summarizePlayers() )
            Assert.assertTrueNR(null != summary.isoCode)
				values.putAnd(DBHelper.ISOCODE, summary.isoCode.toString())
					.putAnd(DBHelper.GAMEID, summary.gameID)
					.putAnd(DBHelper.GAME_OVER, if (summary.gameOver) 1 else 0)
					.putAnd(DBHelper.QUASHED, if (summary.quashed) 1 else 0)
					.putAnd(DBHelper.LASTMOVE, summary.lastMoveTime)
					.putAnd(DBHelper.NEXTDUPTIMER, summary.dupTimerExpires)
					.putAnd(DBHelper.CAN_REMATCH, if (summary.canRematch) 1 else 0)

            // Don't overwrite extras! Sometimes this method is called from
            // JNIThread which has created the summary from common code that
            // doesn't know about Android additions. Leave those unset to
            // avoid overwriting.
            val extras = summary.extras
            if (null != extras) {
                values.put(DBHelper.EXTRAS, summary.extras)
            }
            val nextNag = if (summary.nextTurnIsLocal()) NagTurnReceiver.figureNextNag(
                context,
                1000 * summary.lastMoveTime.toLong()
            ) else 0
            values.putAnd(DBHelper.NEXTNAG, nextNag)
				.putAnd(DBHelper.DICTLIST, summary.dictNames(DICTS_SEP))
            if (null != summary.scores) {
                val sb = StringBuffer()
                for (score in summary.scores) {
                    sb.append(String.format("%d ", score))
                }
                values.put(DBHelper.SCORES, sb.toString())
            }
            if (null != summary.conTypes) {
                values.putAnd(DBHelper.CONTYPE, summary.conTypes.toInt())
					.putAnd(DBHelper.SEED, summary.seed)
					.putAnd(DBHelper.NPACKETSPENDING, summary.nPacketsPending)
                val iter: Iterator<CommsConnType> = summary.conTypes.iterator()
                while (iter.hasNext()) {
                    when (val typ = iter.next()) {
                        CommsConnType.COMMS_CONN_RELAY -> {
                            val relayID = summary.relayID
                            values.putAnd(DBHelper.ROOMNAME, summary.roomName)
								.putAnd(DBHelper.RELAYID, summary.relayID)
                        }

                        CommsConnType.COMMS_CONN_BT, CommsConnType.COMMS_CONN_SMS
							-> values.put(DBHelper.REMOTEDEVS,
										  summary.summarizeDevs() )
                        else -> Log.d( TAG, "unexpected type ${typ}")
                    }
                }
            }
            values.put(DBHelper.SERVERROLE, summary.serverRole.ordinal)
        }
        initDB(context)
        synchronized(s_dbHelper!!) {
            if (null == summary) {
                delete(TABLE_NAMES.SUM, selection)
            } else {
                val result = update(TABLE_NAMES.SUM, values, selection).toLong()
                Assert.assertTrue(result >= 0)
            }
            notifyListeners(context, rowid, GameChangeType.GAME_CHANGED)
            invalGroupsCache()
        }
        if (null != summary) { // nag time may have changed
            NagTurnReceiver.setNagTimer(context)
        }
    } // saveSummary

    @JvmStatic
    fun countGamesUsingISOCode(context: Context, isoCode: ISOCode?): Int {
        var result = 0
        val columns = arrayOf(DBHelper.ISOCODE)
        val selection = String.format(
            "%s = '%s'", columns[0],
            isoCode
        )
        // null for columns will return whole rows: bad
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            result = cursor.count
            cursor.close()
        }
        return result
    }

    fun countGamesUsingDict(context: Context, dict: String?): Int {
        var result = 0
        val pattern = String.format(
            "%%%s%s%s%%",
            DICTS_SEP, dict, DICTS_SEP
        )
        val selection = String.format(
            "%s LIKE '%s'",
            DBHelper.DICTLIST, pattern
        )
        // null for columns will return whole rows: bad.  But
        // might as well make it an int for speed
        val columns = arrayOf(DBHelper.ISOCODE)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            result = cursor.count
            cursor.close()
        }
        return result
    }

    private fun countOpenGamesUsing(
        context: Context,
        connTypWith: CommsConnType,
        connTypWithout: CommsConnType? = null
    ): Int {
        var result = 0
        val columns = arrayOf(DBHelper.CONTYPE)
        val selection = String.format("%s = 0", DBHelper.GAME_OVER)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            val indx = cursor.getColumnIndex(DBHelper.CONTYPE)
            while (cursor.moveToNext()) {
                val typs = CommsConnTypeSet(cursor.getInt(indx))
                if (typs.contains(connTypWith)) {
                    if (null == connTypWithout || !typs.contains(connTypWithout)) {
                        ++result
                    }
                }
            }
            cursor.close()
        }
        if (0 < result) {
            Log.d(
                TAG, "countOpenGamesUsing(with: %s, without: %s) => %d",
                connTypWith, connTypWithout, result
            )
        }
        return result
    }

    fun countOpenGamesUsingNBS(context: Context): Int {
        // Log.d( TAG, "countOpenGamesUsingNBS() => %d", result );
        return countOpenGamesUsing(context, CommsConnType.COMMS_CONN_SMS)
    }

    @JvmStatic
    fun getInvitesFor(context: Context, rowid: Long): SentInvitesInfo {
        val result = SentInvitesInfo(rowid)
        val columns = arrayOf(
            DBHelper.MEANS, DBHelper.TARGET,
            " (strftime('%s', " + DBHelper.TIMESTAMP
                    + ") * 1000) AS " + DBHelper.TIMESTAMP
        )
        val selection = String.format("%s = %d", DBHelper.ROW, rowid)
        val orderBy = DBHelper.TIMESTAMP + " DESC"
        synchronized(s_dbHelper!!) {
            val cursor = DBHelper.query(
                s_db!!, TABLE_NAMES.INVITES, columns,
                selection, orderBy
            )
            if (0 < cursor.count) {
                val indxMns = cursor.getColumnIndex(DBHelper.MEANS)
                val indxTS = cursor.getColumnIndex(DBHelper.TIMESTAMP)
                val indxTrgt = cursor.getColumnIndex(DBHelper.TARGET)
                val values = InviteMeans.entries.toTypedArray()
                while (cursor.moveToNext()) {
                    val ordinal = cursor.getInt(indxMns)
                    if (ordinal < values.size) {
                        val means = values[ordinal]
                        val ts = Date(cursor.getLong(indxTS))
                        val target = cursor.getString(indxTrgt)
                        result.addEntry(means, target, ts)
                    }
                }
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun recordInviteSent(
        context: Context, rowid: Long,
        means: InviteMeans, target: String?,
        dropDupes: Boolean
    ) {
        if (BuildConfig.NON_RELEASE) {
            when (means) {
                InviteMeans.EMAIL, InviteMeans.NFC, InviteMeans.CLIPBOARD, InviteMeans.WIFIDIRECT, InviteMeans.SMS_USER, InviteMeans.QRCODE, InviteMeans.MQTT, InviteMeans.SMS_DATA, InviteMeans.BLUETOOTH -> {}
                InviteMeans.RELAY -> Assert.failDbg()
                else -> Assert.failDbg()
            }
        }
        var dropTest: String? = null
        if (dropDupes) {
            dropTest = String.format(
                "%s = %d AND %s = %d",
                DBHelper.ROW, rowid,
                DBHelper.MEANS, means.ordinal
            )
            if (null != target) {
                dropTest += String.format(
                    " AND %s = '%s'",
                    DBHelper.TARGET, target
                )
            } else {
                // If I'm seeing this, need to check above if a "target is
                // null" test is needed to avoid nuking unintentinally.
                Assert.failDbg()
            }
        }
        val values = ContentValues()
			.putAnd(DBHelper.ROW, rowid)
			.putAnd(DBHelper.MEANS, means.ordinal)
        if (null != target) {
            values.put(DBHelper.TARGET, target)
        }
        initDB(context)
        synchronized(s_dbHelper!!) {
            if (null != dropTest) {
                delete(TABLE_NAMES.INVITES, dropTest)
            }
            insert(TABLE_NAMES.INVITES, values)
        }
    }

    private fun setSummaryInt(context: Context, rowid: Long, column: String, value: Int) {
        val values = ContentValues().putAnd(column, value)
        updateRow(context, TABLE_NAMES.SUM, rowid, values)
    }

    @JvmStatic
    fun setMsgFlags(context: Context, rowid: Long, flags: Int) {
        setSummaryInt(context, rowid, DBHelper.HASMSGS, flags)
        notifyListeners(context, rowid, GameChangeType.GAME_CHANGED)
    }

    @JvmStatic
    fun setExpanded(context: Context, rowid: Long, expanded: Boolean) {
        setSummaryInt(context, rowid, DBHelper.CONTRACTED, if (expanded) 0 else 1)
    }

    private fun getSummaryInt(
        context: Context, rowid: Long, column: String,
        dflt: Int
    ): Int {
        var result = dflt
        val selection = String.format(ROW_ID_FMT, rowid)
        val columns = arrayOf(column)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (1 == cursor.count && cursor.moveToFirst()) {
                result = cursor.getInt(cursor.getColumnIndex(column))
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun getMsgFlags(context: Context, rowid: Long): Int {
        return getSummaryInt(
            context, rowid, DBHelper.HASMSGS,
            GameSummary.MSG_FLAGS_NONE
        )
    }

    @JvmStatic
    fun getExpanded(context: Context, rowid: Long): Boolean {
        return 0 == getSummaryInt(context, rowid, DBHelper.CONTRACTED, 0)
    }

    @JvmStatic
    fun gameOver(context: Context, rowid: Long): Boolean {
        return 0 != getSummaryInt(context, rowid, DBHelper.GAME_OVER, 0)
    }

    @JvmStatic
    fun saveThumbnail(
        context: Context, lock: GameLock,
        thumb: Bitmap?
    ) {
        val rowid = lock.rowid
        val selection = String.format(ROW_ID_FMT, rowid)
        val values = ContentValues()
        if (null == thumb) {
            values.putNull(DBHelper.THUMBNAIL)
        } else {
            val bas = ByteArrayOutputStream()
            thumb.compress(CompressFormat.PNG, 0, bas)
            values.put(DBHelper.THUMBNAIL, bas.toByteArray())
        }
        initDB(context)
        synchronized(s_dbHelper!!) {
            val result = update(TABLE_NAMES.SUM, values, selection).toLong()
            Assert.assertTrue(result >= 0)
            notifyListeners(context, rowid, GameChangeType.GAME_CHANGED)
        }
    }

    @JvmStatic
    fun clearThumbnails(context: Context) {
        val values = ContentValues()
        values.putNull(DBHelper.THUMBNAIL)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val result = update(TABLE_NAMES.SUM, values, null).toLong()
            notifyListeners(context, ROWIDS_ALL.toLong(), GameChangeType.GAME_CHANGED)
        }
    }

    @JvmStatic
    fun getGamesWithSendsPending(context: Context): HashMap<Long, CommsConnTypeSet> {
        val result = HashMap<Long, CommsConnTypeSet>()
        val columns = arrayOf(ROW_ID, DBHelper.CONTYPE)
        val selection = String.format(
            "%s != %d AND %s > 0 AND %s != %d",
            DBHelper.SERVERROLE,
            DeviceRole.SERVER_STANDALONE.ordinal,
            DBHelper.NPACKETSPENDING,
            DBHelper.GROUPID, getArchiveGroup(context)
        )
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            val indx1 = cursor.getColumnIndex(ROW_ID)
            val indx2 = cursor.getColumnIndex(DBHelper.CONTYPE)
            var ii = 0
            while (cursor.moveToNext()) {
                val rowid = cursor.getLong(indx1)
                val typs = CommsConnTypeSet(cursor.getInt(indx2))
                // Better have an address if has pending sends
                if (0 < typs.size) {
                    result[rowid] = typs
                }
                ++ii
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun getGameCountUsing(context: Context, typ: CommsConnType): Int {
        var result = 0
        val columns = arrayOf(DBHelper.CONTYPE)
        val selection = String.format("%s = 0", DBHelper.GAME_OVER)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            val indx = cursor.getColumnIndex(DBHelper.CONTYPE)
            while (cursor.moveToNext()) {
                val typs = CommsConnTypeSet(cursor.getInt(indx))
                if (typs.contains(typ)) {
                    ++result
                }
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun getRowIDsFor(context: Context, gameID: Int): LongArray {
        var result: LongArray
        val columns = arrayOf(ROW_ID)
        val selection = String.format(DBHelper.GAMEID + "=%d", gameID)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            result = LongArray(cursor.count)
            var ii = 0
            while (cursor.moveToNext()) {
                result[ii] = cursor.getLong(cursor.getColumnIndex(ROW_ID))
                ++ii
            }
            cursor.close()
        }
        if (1 != result.size) {
            Log.d(
                TAG, "getRowIDsFor(gameID=%X)=>length %d array", gameID,
                result.size
            )
        }
        return result
    }

    @JvmStatic
    fun getRowIDsAndChannels(context: Context, gameID: Int): Map<Long, Int> {
        val result: MutableMap<Long, Int> = HashMap()
        val columns = arrayOf(ROW_ID, DBHelper.GIFLAGS)
        val selection = String.format(DBHelper.GAMEID + "=%d", gameID)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            while (cursor.moveToNext()) {
                val flags = cursor.getInt(cursor.getColumnIndex(DBHelper.GIFLAGS))
                val forceChannel = (flags shr GameSummary.FORCE_CHANNEL_OFFSET
                        and GameSummary.FORCE_CHANNEL_MASK)
                val rowid = cursor.getLong(cursor.getColumnIndex(ROW_ID))
                result[rowid] = forceChannel
                // Log.i( TAG, "getRowIDsAndChannels(): added %d => %d",
                //        rowid, forceChannel );
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun haveWithRowID(context: Context, rowid: Long): Boolean {
        var result = false
        val columns = arrayOf(ROW_ID)
        val selection = String.format(ROW_ID + "=%d", rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            Assert.assertTrue(1 >= cursor.count)
            result = 1 == cursor.count
            cursor.close()
        }
        return result
    }

    fun listBTGames(
        context: Context,
        result: HashMap<String?, IntArray?>
    ) {
        var set: HashSet<Int>?
        val columns = arrayOf(DBHelper.GAMEID, DBHelper.REMOTEDEVS)
        val selection = DBHelper.GAMEID + "!=0"
        val map = HashMap<String, HashSet<Int>>()
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            while (cursor.moveToNext()) {
                var col = cursor.getColumnIndex(DBHelper.GAMEID)
                val gameID = cursor.getInt(col)
                col = cursor.getColumnIndex(DBHelper.REMOTEDEVS)
                val devs = cursor.getString(col)
                Log.i(TAG, "gameid %d has remote[s] %s", gameID, devs)
                if (null != devs && 0 < devs.length) {
                    for (dev in TextUtils.split(devs, "\n")) {
                        set = map[dev]
                        if (null == set) {
                            set = HashSet()
                            map[dev] = set
                        }
                        set.add(gameID)
                    }
                }
            }
            cursor.close()
        }
        val devs: Set<String> = map.keys
        val iter = devs.iterator()
        while (iter.hasNext()) {
            val dev = iter.next()
            set = map[dev]!!
            val gameIDs = IntArray(set.size)
            val idIter: Iterator<Int> = set.iterator()
            var ii = 0
            while (idIter.hasNext()) {
                gameIDs[ii] = idIter.next()
                ++ii
            }
            result[dev] = gameIDs
        }
    }

    @JvmStatic
    fun saveNewGame(
        context: Context, bytes: ByteArray,
        groupID: Long, name: String?
    ): GameLock? {
        Assert.assertTrue(GROUPID_UNSPEC.toLong() != groupID)
        var lock: GameLock? = null
        val timestamp = Date().time // milliseconds since epoch
        val values = ContentValues()
			.putAnd(DBHelper.SNAPSHOT, bytes)
			.putAnd(DBHelper.CREATE_TIME, timestamp)
			.putAnd(DBHelper.LASTPLAY_TIME, timestamp)
			.putAnd(DBHelper.GROUPID, groupID)
        if (null != name) {
            values.put(DBHelper.GAME_NAME, name)
        }
        invalGroupsCache() // do first in case any listener has cached data
        initDB(context)
        synchronized(s_dbHelper!!) {
            values.put(DBHelper.VISID, maxVISID(s_db))
            val rowid = insert(TABLE_NAMES.SUM, values)
            setCached(rowid, null) // force reread
            lock = GameLock.tryLock(rowid)
            Assert.assertNotNull(lock)
            notifyListeners(context, rowid, GameChangeType.GAME_CREATED)
        }
        invalGroupsCache() // then again after
        return lock
    } // saveNewGame

    @JvmStatic
    fun saveGame(
        context: Context, lock: GameLock,
        bytes: ByteArray, setCreate: Boolean
    ): Long {
        Assert.assertTrue(lock.canWrite())
        val rowid = lock.rowid
        val values = ContentValues()
			.putAnd(DBHelper.SNAPSHOT, bytes)
        val timestamp = Date().time
        if (setCreate) {
            values.put(DBHelper.CREATE_TIME, timestamp)
        }
        values.put(DBHelper.LASTPLAY_TIME, timestamp)
        updateRow(context, TABLE_NAMES.SUM, rowid, values)
        setCached(rowid, null) // force reread
        if (ROWID_NOTFOUND.toLong() != rowid) {      // Means new game?
            notifyListeners(context, rowid, GameChangeType.GAME_CHANGED)
        }
        invalGroupsCache()
        return rowid
    }

    @JvmStatic
    fun loadGame(context: Context, lock: GameLock): ByteArray? {
        var result: ByteArray? = null
        val rowid = lock.rowid
        Assert.assertTrue(ROWID_NOTFOUND.toLong() != rowid)
        if (Quarantine.safeToOpen(rowid)) {
            result = getCached(rowid)
            if (null == result) {
                val columns = arrayOf(DBHelper.SNAPSHOT)
                val selection = String.format(ROW_ID_FMT, rowid)
                initDB(context)
                synchronized(s_dbHelper!!) {
                    val cursor = query(TABLE_NAMES.SUM, columns, selection)
                    if (1 == cursor.count && cursor.moveToFirst()) {
                        result = cursor.getBlob(
                            cursor
                                .getColumnIndex(DBHelper.SNAPSHOT)
                        )
                    } else {
                        Log.e(TAG, "loadGame: none for rowid=%d", rowid)
                    }
                    cursor.close()
                }
                setCached(rowid, result)
            }
        }
        return result
    }

    fun deleteGame(context: Context, rowid: Long) {
        GameLock.lock(rowid, 300).use { lock ->
            if (null != lock) {
                deleteGame(context, lock)
            } else {
                Log.e(TAG, "deleteGame: unable to lock rowid %d", rowid)
                Assert.failDbg()
            }
        }
    }

    @JvmStatic
    fun deleteGame(context: Context, lock: GameLock) {
        Assert.assertTrue(lock.canWrite())
        val rowid = lock.rowid
        val selSummaries = String.format(ROW_ID_FMT, rowid)
        val selInvites = String.format("%s=%d", DBHelper.ROW, rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            delete(TABLE_NAMES.SUM, selSummaries)

            // Delete invitations too
            delete(TABLE_NAMES.INVITES, selInvites)

            // Delete chats too -- same sel as for invites
            delete(TABLE_NAMES.CHAT, selInvites)
            deleteCurChatsSync(s_db, rowid)
        }
        notifyListeners(context, rowid, GameChangeType.GAME_DELETED)
        invalGroupsCache()
    }

    @JvmStatic
    fun getVisID(context: Context, rowid: Long): Int {
        var result = ROWID_NOTFOUND
        val columns = arrayOf(DBHelper.VISID)
        val selection = String.format(ROW_ID_FMT, rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (1 == cursor.count && cursor.moveToFirst()) {
                result = cursor.getInt(
                    cursor
                        .getColumnIndex(DBHelper.VISID)
                )
            }
            cursor.close()
        }
        return result
    }

    // Get either the file name or game name, preferring the latter.
    @JvmStatic
    fun getName(context: Context, rowid: Long): String? {
        var result: String? = null
        val columns = arrayOf(DBHelper.GAME_NAME)
        val selection = String.format(ROW_ID_FMT, rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (1 == cursor.count && cursor.moveToFirst()) {
                result = cursor.getString(
                    cursor
                        .getColumnIndex(DBHelper.GAME_NAME)
                )
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun setName(context: Context, rowid: Long, name: String) {
        val values = ContentValues()
			.putAnd(DBHelper.GAME_NAME, name)
        updateRow(context, TABLE_NAMES.SUM, rowid, values)
    }

    private fun convertChatString(
        context: Context, rowid: Long,
        playersLocal: BooleanArray
    ): java.util.ArrayList<HistoryPair> {
        var result = ArrayList<HistoryPair>()
        val oldHistory = getChatHistoryStr(context, rowid)
        if (null != oldHistory) {
            Log.d(TAG, "convertChatString(): got string: %s", oldHistory)
            val valuess = ArrayList<ContentValues>()
            //val pairs = ArrayList<HistoryPair?>()
            val localPrefix = LocUtils.getString(context, R.string.chat_local_id)
            val rmtPrefix = LocUtils.getString(context, R.string.chat_other_id)
            Log.d(TAG, "convertChatString(): prefixes: \"%s\" and \"%s\"", localPrefix, rmtPrefix)
            val msgs = oldHistory.split("\n".toRegex()).dropLastWhile { it.isEmpty() }
                .toTypedArray()
            Log.d(TAG, "convertChatString(): split into %d", msgs.size)
            var localPlayerIndx = -1
            var remotePlayerIndx = -1
            for (ii in playersLocal.indices.reversed()) {
                if (playersLocal[ii]) {
                    localPlayerIndx = ii
                } else {
                    remotePlayerIndx = ii
                }
            }
            for (msg in msgs) {
                Log.d(TAG, "convertChatString(): msg: %s", msg)
                var indx = -1
                var prefix: String? = null
                if (msg.startsWith(localPrefix)) {
                    Log.d(TAG, "convertChatString(): msg: %s starts with %s", msg, localPrefix)
                    prefix = localPrefix
                    indx = localPlayerIndx
                } else if (msg.startsWith(rmtPrefix)) {
                    Log.d(TAG, "convertChatString(): msg: %s starts with %s", msg, rmtPrefix)
                    prefix = rmtPrefix
                    indx = remotePlayerIndx
                } else {
                    Log.d(TAG, "convertChatString(): msg: %s starts with neither", msg)
                }
                if (-1 != indx) {
                    Log.d(TAG, "convertChatString(): removing substring %s; was: %s", prefix, msg)
                    val msg2 = msg.substring(prefix!!.length, msg.length)
                    Log.d(TAG, "convertChatString(): removED substring; now %s", msg2)
                    valuess.add(cvForChat(rowid, msg2, indx, 0))
                    val pair = HistoryPair(msg2, indx, 0)
                    result.add(pair)
                }
            }
            // result = pairs.toTypedArray<HistoryPair?>()
            appendChatHistory(context, valuess)
            // clearChatHistoryString( context, rowid );
        }
        return result
    }

    fun getChatHistory(
        context: Context, rowid: Long,
        playersLocal: BooleanArray
    ): ArrayList<HistoryPair> {
        var result = java.util.ArrayList<HistoryPair>()
        val columns = arrayOf(DBHelper.SENDER, DBHelper.MESSAGE, DBHelper.CHATTIME)
        val selection = String.format("%s=%d", DBHelper.ROW, rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.CHAT, columns, selection)
            if (0 < cursor.count) {
                val msgIndex = cursor.getColumnIndex(DBHelper.MESSAGE)
                val plyrIndex = cursor.getColumnIndex(DBHelper.SENDER)
                val tsIndex = cursor.getColumnIndex(DBHelper.CHATTIME)
                while (cursor.moveToNext()) {
                    val msg = cursor.getString(msgIndex)
                    val plyr = cursor.getInt(plyrIndex)
                    val ts = cursor.getInt(tsIndex)
                    val pair = HistoryPair(msg, plyr, ts)
                    result.add(pair)
                }
            }
            cursor.close()
        }
        if (result.isEmpty()) {
            result = convertChatString(context, rowid, playersLocal)
        }
        return result
    }

    private fun formatCurChatKey(rowid: Long, player: Int = -1): String {
        val playerMatch =
            if (0 <= player) String.format("%d", player) else "%"
        return String.format("<<chat/%d/%s>>", rowid, playerMatch)
    }

    fun getCurChat(
        context: Context, rowid: Long, player: Int,
        startAndEndOut: IntArray
    ): String? {
        var result: String? = null
        val key = formatCurChatKey(rowid, player)
        val all = getStringFor(context, key, "")
        val parts = TextUtils.split(all, ":")
        if (3 <= parts.size) {
            result = all!!.substring(2 + parts[0].length + parts[1].length)
            startAndEndOut[0] = Math.min(result.length, parts[0].toInt())
            startAndEndOut[1] = Math.min(result.length, parts[1].toInt())
        }
        Log.d(
            TAG, "getCurChat(): => %s [%d,%d]", result,
            startAndEndOut[0], startAndEndOut[1]
        )
        return result
    }

    fun setCurChat(
        context: Context, rowid: Long, player: Int,
        text: String?, start: Int, end: Int
    ) {
        var text = text
        val key = formatCurChatKey(rowid, player)
        text = String.format("%d:%d:%s", start, end, text)
        setStringFor(context, key, text)
    }

    private fun deleteCurChatsSync(db: SQLiteDatabase?, rowid: Long) {
        val like = formatCurChatKey(rowid)
        delStringsLikeSync(db, like)
    }

    @JvmStatic
    fun getNeedNagging(context: Context): Array<NeedsNagInfo?>? {
        var result: Array<NeedsNagInfo?>? = null
        val now = Date().time // in milliseconds
        val columns = arrayOf(
            ROW_ID, DBHelper.NEXTNAG, DBHelper.LASTMOVE,
            DBHelper.SERVERROLE
        )
        // where nextnag > 0 AND nextnag < now
        val selection = String.format(
            "%s > 0 AND %s < %s", DBHelper.NEXTNAG,
            DBHelper.NEXTNAG, now
        )
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            val count = cursor.count
            if (0 < count) {
                result = arrayOfNulls(count)
                val rowIndex = cursor.getColumnIndex(ROW_ID)
                val nagIndex = cursor.getColumnIndex(DBHelper.NEXTNAG)
                val lastMoveIndex = cursor.getColumnIndex(DBHelper.LASTMOVE)
                val roleIndex = cursor.getColumnIndex(DBHelper.SERVERROLE)
                var ii = 0
                while (ii < result!!.size && cursor.moveToNext()) {
                    val rowid = cursor.getLong(rowIndex)
                    val nextNag = cursor.getLong(nagIndex)
                    val lastMove = cursor.getLong(lastMoveIndex)
                    val role = DeviceRole.entries[cursor.getInt(roleIndex)]
                    result!![ii] = NeedsNagInfo(rowid, nextNag, lastMove, role)
                    ++ii
                }
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun getNextNag(context: Context): Long {
        var result: Long = 0
        val columns = arrayOf("MIN(" + DBHelper.NEXTNAG + ") as min")
        val selection = "NOT " + DBHelper.NEXTNAG + "= 0"
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (cursor.moveToNext()) {
                result = cursor.getLong(cursor.getColumnIndex("min"))
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun updateNeedNagging(context: Context, needNagging: Array<NeedsNagInfo>) {
        var updateQuery = ("update %s set %s = ? "
                + " WHERE %s = ? ")
        updateQuery = String.format(
            updateQuery, TABLE_NAMES.SUM,
            DBHelper.NEXTNAG, ROW_ID
        )
        initDB(context)
        synchronized(s_dbHelper!!) {
            val updateStmt = s_db!!.compileStatement(updateQuery)
            for (info in needNagging) {
                updateStmt.bindLong(1, info.m_nextNag)
                updateStmt.bindLong(2, info.m_rowid)
                updateStmt.execute()
            }
        }
    }

    private var s_groupsCache: MutableMap<Long, GameGroupInfo?>? = null
    private fun invalGroupsCache() {
        s_groupsCache = null
    }

    @JvmStatic
    fun getThumbnail(context: Context, rowid: Long): Bitmap? {
        var thumb: Bitmap? = null
        var data: ByteArray? = null
        val columns = arrayOf(DBHelper.THUMBNAIL)
        val selection = String.format(ROW_ID_FMT, rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (1 == cursor.count && cursor.moveToFirst()) {
                data = cursor.getBlob(cursor.getColumnIndex(DBHelper.THUMBNAIL))
            }
            cursor.close()
        }
        if (null != data) {
            thumb = BitmapFactory.decodeByteArray(data, 0, data!!.size)
        }
        return thumb
    }

    private fun getGameCounts(db: SQLiteDatabase?): HashMap<Long, Int> {
        val result = HashMap<Long, Int>()
        var query = "SELECT %s, count(%s) as cnt FROM %s GROUP BY %s"
        query = String.format(
            query, DBHelper.GROUPID, DBHelper.GROUPID,
            TABLE_NAMES.SUM, DBHelper.GROUPID
        )
        val cursor = db!!.rawQuery(query, null)
        val rowIndex = cursor.getColumnIndex(DBHelper.GROUPID)
        val cntIndex = cursor.getColumnIndex("cnt")
        while (cursor.moveToNext()) {
            val row = cursor.getLong(rowIndex)
            val count = cursor.getInt(cntIndex)
            result[row] = count
        }
        cursor.close()
        return result
    }

    // Map of groups rowid (= summaries.groupid) to group info record
    @JvmStatic
    fun getGroups(context: Context): Map<Long, GameGroupInfo?> {
        var result = s_groupsCache
        if (null == result) {
            result = HashMap()

            // Select all groups.  For each group get the number of games in
            // that group.  There should be a way to do that with one query
            // but I can't figure it out.
            val query = ("SELECT rowid, groupname as groups_groupname, "
                    + " groups.expanded as groups_expanded FROM groups")
            initDB(context)
            synchronized(s_dbHelper!!) {
                val map = getGameCounts(s_db)
                val cursor = s_db!!.rawQuery(query, null)
                val idIndex = cursor.getColumnIndex("rowid")
                val nameIndex = cursor.getColumnIndex("groups_groupname")
                val expandedIndex = cursor.getColumnIndex("groups_expanded")
                while (cursor.moveToNext()) {
                    val id = cursor.getLong(idIndex)
                    val name = cursor.getString(nameIndex)
                    Assert.assertNotNull(name)
                    val expanded = 0 != cursor.getInt(expandedIndex)
                    val count = if (map.containsKey(id)) map[id]!! else 0
                    result[id] = GameGroupInfo(name, count, expanded)
                }
                cursor.close()
                val iter: Iterator<Long> = result.keys.iterator()
                while (iter.hasNext()) {
                    val groupID = iter.next()
                    val ggi = result[groupID]
                    readTurnInfo(s_db, groupID, ggi)
                }
            }
            s_groupsCache = result
        }
        // Log.d( TAG, "getGroups() => %s", result );
        return result
    } // getGroups

    private fun readTurnInfo(
        db: SQLiteDatabase?, groupID: Long,
        ggi: GameGroupInfo?
    ) {
        val columns = arrayOf(
            DBHelper.LASTMOVE, DBHelper.GIFLAGS,
            DBHelper.TURN
        )
        val orderBy = DBHelper.LASTMOVE
        val selection = String.format("%s=%d", DBHelper.GROUPID, groupID)
        val cursor = DBHelper.query(
            db!!, TABLE_NAMES.SUM, columns,
            selection, orderBy
        )

        // We want the earliest LASTPLAY_TIME (i.e. the first we see
        // since they're in order) that's a local turn, if any,
        // otherwise a non-local turn.
        var lastPlayTimeLocal: Long = 0
        var lastPlayTimeRemote: Long = 0
        val indexLPT = cursor.getColumnIndex(DBHelper.LASTMOVE)
        val indexFlags = cursor.getColumnIndex(DBHelper.GIFLAGS)
        val turnFlags = cursor.getColumnIndex(DBHelper.TURN)
        while (cursor.moveToNext() && 0L == lastPlayTimeLocal) {
            val flags = cursor.getInt(indexFlags)
            val turn = cursor.getInt(turnFlags)
            val isLocal = GameSummary.localTurnNext(flags, turn)
            if (null != isLocal) {
                val lpt = cursor.getLong(indexLPT)
                if (isLocal) {
                    lastPlayTimeLocal = lpt
                } else if (0L == lastPlayTimeRemote) {
                    lastPlayTimeRemote = lpt
                }
            }
        }
        cursor.close()
        ggi!!.m_hasTurn = 0L != lastPlayTimeLocal || 0L != lastPlayTimeRemote
        if (ggi.m_hasTurn) {
            ggi.m_turnLocal = 0L != lastPlayTimeLocal
            if (ggi.m_turnLocal) {
                ggi.m_lastMoveTime = lastPlayTimeLocal
            } else {
                ggi.m_lastMoveTime = lastPlayTimeRemote
            }
            // DateFormat df = DateFormat.getDateTimeInstance( DateFormat.SHORT,
            //                                                 DateFormat.SHORT );
            // DbgUtils.logf( "using last play time %s for",
            //                df.format( new Date( 1000 * ggi.m_lastMoveTime ) ) );
        }
    }

    fun countGames(context: Context): Int {
        var result = 0
        val columns = arrayOf(ROW_ID)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, null)
            result = cursor.count
            cursor.close()
        }
        return result
    }

    // ORDER BY clause that governs display of games in main GamesList view
    private val s_getGroupGamesOrderBy = TextUtils.join(
        ",", arrayOf( // Ended games at bottom
            DBHelper.GAME_OVER,  // games with unread chat messages at top
            "(" + DBHelper.HASMSGS + " & " + GameSummary.MSG_FLAGS_CHAT + ") IS NOT 0 DESC",  // Games not yet connected at top
            DBHelper.TURN + " is -1 DESC",  // Games where it's a local player's turn at top
            DBHelper.TURN_LOCAL + " DESC",  // finally, sort by timestamp of last-made move
            DBHelper.LASTMOVE
        )
    )

    @JvmStatic
    fun getGroupGames(context: Context, groupID: Long): LongArray {
        var result = longArrayOf()
        initDB(context)
        val columns = arrayOf(ROW_ID, DBHelper.HASMSGS)
        val selection = String.format("%s=%d", DBHelper.GROUPID, groupID)
        synchronized(s_dbHelper!!) {
            val cursor = s_db!!.query(
                TABLE_NAMES.SUM.toString(), columns,
                selection,  // selection
                null,  // args
                null,  // groupBy
                null,  // having
                s_getGroupGamesOrderBy
            )
            val index = cursor.getColumnIndex(ROW_ID)
            result = LongArray(cursor.count)
            var ii = 0
            while (cursor.moveToNext()) {
                val rowid = cursor.getInt(index).toLong()
                result[ii] = rowid
                ++ii
            }
            cursor.close()
        }
        return result
    }

    // pass ROWID_NOTFOUND to get *any* group.  Because there may be
    // some hidden games stored with group = -1 thanks to
    // recently-fixed bugs, be sure to skip them.
    @JvmStatic
    fun getGroupForGame(context: Context, rowid: Long): Long {
        var result = GROUPID_UNSPEC.toLong()
        initDB(context)
        val columns = arrayOf(DBHelper.GROUPID)
        var selection = String.format(
            "%s != %d", DBHelper.GROUPID,
            GROUPID_UNSPEC
        )
        if (ROWID_NOTFOUND.toLong() != rowid) {
            selection += " AND " + String.format(ROW_ID_FMT, rowid)
        }
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (cursor.moveToNext()) {
                val index = cursor.getColumnIndex(DBHelper.GROUPID)
                result = cursor.getLong(index)
            }
            cursor.close()
        }
        return result
    }

    @JvmStatic
    fun getAnyGroup(context: Context): Long {
        var result = GROUPID_UNSPEC.toLong()
        val groups = getGroups(context)
        val iter = groups.keys.iterator()
        if (iter.hasNext()) {
            result = iter.next()
        }
        Assert.assertTrue(GROUPID_UNSPEC.toLong() != result)
        return result
    }

    @JvmStatic
    fun getGroup(context: Context, name: String): Long {
        var result: Long
        initDB(context)
        synchronized(s_dbHelper!!) { result = getGroupImpl(name) }
        return result
    }

    private fun getGroupImpl(name: String): Long {
        var result = GROUPID_UNSPEC.toLong()
        val columns = arrayOf(ROW_ID)
        val selection = DBHelper.GROUPNAME + " = ?"
        val selArgs = arrayOf(name)
        val cursor = s_db!!.query(
            TABLE_NAMES.GROUPS.toString(), columns,
            selection, selArgs,
            null,  // groupBy
            null,  // having
            null // orderby
        )
        if (cursor.moveToNext()) {
            result = cursor.getLong(cursor.getColumnIndex(ROW_ID))
        }
        cursor.close()
        Log.d(TAG, "getGroupImpl(%s) => %d", name, result)
        return result
    }

    private fun addGroupImpl(name: String): Long {
        val values = ContentValues()
			.putAnd(DBHelper.GROUPNAME, name)
			.putAnd(DBHelper.EXPANDED, 1)
        val rowid = insert(TABLE_NAMES.GROUPS, values)
        invalGroupsCache()
        return rowid
    }

    @JvmStatic
    fun addGroup(context: Context, name: String): Long {
        var rowid = GROUPID_UNSPEC.toLong()
        if (0 < name.length) {
            synchronized(s_dbHelper!!) {
				rowid = addGroupImpl(name)
			}
        }
        return rowid
    }

    @JvmStatic
    fun deleteGroup(context: Context, groupid: Long) {
        // Nuke games having this group id
        val selectionGames = String.format("%s=%d", DBHelper.GROUPID, groupid)

        // And nuke the group record itself
        val selectionGroups = String.format(ROW_ID_FMT, groupid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            delete(TABLE_NAMES.SUM, selectionGames)
            delete(TABLE_NAMES.GROUPS, selectionGroups)
        }
        invalGroupsCache()
    }

    @JvmStatic
    fun setGroupName(
        context: Context, groupid: Long,
        name: String
    ) {
        val values = ContentValues()
			.putAnd(DBHelper.GROUPNAME, name)
        updateRow(context, TABLE_NAMES.GROUPS, groupid, values)
        invalGroupsCache()
    }

    @JvmStatic
    fun setGroupExpanded(
        context: Context, groupid: Long,
        expanded: Boolean
    ) {
        val values = ContentValues()
			.putAnd(DBHelper.EXPANDED, if (expanded) 1 else 0)
        updateRow(context, TABLE_NAMES.GROUPS, groupid, values)
        invalGroupsCache()
    }

    @JvmStatic
    fun getArchiveGroup(context: Context): Long {
        val archiveName = LocUtils
            .getString(context, R.string.group_name_archive)
        var archiveGroup = getGroup(context, archiveName)
        if (GROUPID_UNSPEC.toLong() == archiveGroup) {
            archiveGroup = addGroup(context, archiveName)
        }
        return archiveGroup
    }

    // Change group id of a game
    @JvmStatic
    fun moveGame(context: Context, rowid: Long, groupID: Long) {
        Assert.assertTrue(GROUPID_UNSPEC.toLong() != groupID)
        val values = ContentValues()
			.putAnd(DBHelper.GROUPID, groupID)
        updateRow(context, TABLE_NAMES.SUM, rowid, values)
        invalGroupsCache()
        notifyListeners(context, rowid, GameChangeType.GAME_MOVED)
    }

    @JvmStatic
    fun getDupModeGames(context: Context): Map<Long, Int> {
        return getDupModeGames(context, ROWID_NOTFOUND.toLong())
    }

    // Return all games whose DUP_MODE_MASK bit is set. Return also (as map
    // value) the nextTimer value, which will be negative if the game's
    // paused. As a bit of a hack, set it to 0 if the local player has already
    // committed his turn so caller (DupeModeTimer) will know not to show a
    // notification.
    @JvmStatic
    fun getDupModeGames(context: Context, rowid: Long): Map<Long, Int> {
        // select giflags from summaries where 0x100 & giflags != 0;
        val result: MutableMap<Long, Int> = HashMap()
        val columns = arrayOf(ROW_ID, DBHelper.NEXTDUPTIMER, DBHelper.TURN_LOCAL)
        var selection = String.format(
            "%d & %s != 0",
            GameSummary.DUP_MODE_MASK,
            DBHelper.GIFLAGS
        )
        if (ROWID_NOTFOUND.toLong() != rowid) {
            selection += String.format(" AND %s = %d", ROW_ID, rowid)
        }
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            val count = cursor.count
            val indxRowid = cursor.getColumnIndex(ROW_ID)
            val indxTimer = cursor.getColumnIndex(DBHelper.NEXTDUPTIMER)
            val indxIsLocal = cursor.getColumnIndex(DBHelper.TURN_LOCAL)
            while (cursor.moveToNext()) {
                val isLocal = 0 != cursor.getInt(indxIsLocal)
                val timer = if (isLocal) cursor.getInt(indxTimer) else 0
                result[cursor.getLong(indxRowid)] = timer
            }
            cursor.close()
        }
        Log.d(TAG, "getDupModeGames(%d) => %s", rowid, result)
        return result
    }

    private fun getChatHistoryStr(context: Context, rowid: Long): String? {
        var result: String? = null
        val columns = arrayOf(DBHelper.CHAT_HISTORY)
        val selection = String.format(ROW_ID_FMT, rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.SUM, columns, selection)
            if (1 == cursor.count && cursor.moveToFirst()) {
                result = cursor.getString(
                    cursor
                        .getColumnIndex(DBHelper.CHAT_HISTORY)
                )
            }
            cursor.close()
        }
        return result
    }

    private fun appendChatHistory(
        context: Context,
        valuess: ArrayList<ContentValues>
    ) {
        initDB(context)
        synchronized(s_dbHelper!!) {
            for (values in valuess) {
                insert(TABLE_NAMES.CHAT, values)
            }
        }
    }

    private fun cvForChat(rowid: Long, msg: String, plyr: Int, tsSeconds: Long): ContentValues {
        val values = ContentValues()
			.putAnd(DBHelper.ROW, rowid)
			.putAnd(DBHelper.MESSAGE, msg)
			.putAnd(DBHelper.SENDER, plyr)
			.putAnd(DBHelper.CHATTIME, tsSeconds)
        return values
    }

    @JvmStatic
    fun appendChatHistory(
        context: Context, rowid: Long,
        msg: String, fromPlayer: Int,
        tsSeconds: Long
    ) {
        Assert.assertNotNull(msg)
        Assert.assertFalse(-1 == fromPlayer)
        val valuess = ArrayList<ContentValues>()
        valuess.add(cvForChat(rowid, msg, fromPlayer, tsSeconds))
        appendChatHistory(context, valuess)
        Log.i(
            TAG, "appendChatHistory: inserted \"%s\" from player %d",
            msg, fromPlayer
        )
    } // appendChatHistory

    fun clearChatHistory(context: Context, rowid: Long) {
        val selection = String.format("%s = %d", DBHelper.ROW, rowid)
        initDB(context)
        synchronized(s_dbHelper!!) {
            delete(TABLE_NAMES.CHAT, selection)

            // for now, remove any old-format history too. Later when it's
            // removed once converted (after that process is completely
            // debugged), this can be removed.
            val values = ContentValues()
            values.putNull(DBHelper.CHAT_HISTORY)
            updateRowImpl(TABLE_NAMES.SUM, rowid, values)
        }
    }

    @JvmStatic
    fun setDBChangeListener(listener: DBChangeListener) {
        synchronized(s_listeners) {
            Assert.assertNotNull(listener)
            s_listeners.add(listener)
        }
    }

    @JvmStatic
    fun clearDBChangeListener(listener: DBChangeListener) {
        synchronized(s_listeners) {
            Assert.assertTrue(s_listeners.contains(listener))
            s_listeners.remove(listener)
        }
    }

    internal fun addStudyListChangedListener(lnr: StudyListListener) {
        synchronized(s_slListeners) { s_slListeners.add(lnr) }
    }

    internal fun removeStudyListChangedListener(lnr: StudyListListener) {
        synchronized(s_slListeners) { s_slListeners.remove(lnr) }
    }

    @JvmStatic
    fun copyStream(fos: OutputStream, fis: InputStream): Boolean {
        var success = false
        val buf = ByteArray(1024 * 8)
        try {
            var totalBytes: Long = 0
            while (true) {
                val nRead = fis.read(buf)
                if (0 >= nRead) {
                    break
                }
                fos.write(buf, 0, nRead)
                totalBytes += nRead.toLong()
            }
            success = true
            Log.d(
                TAG, "copyFileStream(): copied %s to %s (%d bytes)",
                fis, fos, totalBytes
            )
        } catch (ioe: IOException) {
            Log.ex(TAG, ioe)
        }
        return success
    }

    // Called from jni
    @JvmStatic
    fun dictsGetMD5Sum(context: Context, name: String?): String? {
        val info = dictsGetInfo(context, name)
        return info?.md5Sum
    }

    // Called from jni
    @JvmStatic
    fun dictsSetMD5Sum(context: Context, name: String?, sum: String) {
        val selection = String.format(NAME_FMT, DBHelper.DICTNAME, name)
        val values = ContentValues()
			.putAnd(DBHelper.MD5SUM, sum)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val result = update(TABLE_NAMES.DICTINFO, values, selection)
            if (0 == result) {
                values.put(DBHelper.DICTNAME, name)
                val rowid = insert(TABLE_NAMES.DICTINFO, values)
                Assert.assertTrue(rowid > 0 || !BuildConfig.DEBUG)
            }
        }
    }

    @JvmStatic
    fun dictsGetInfo(context: Context, name: String?): DictInfo? {
        var result: DictInfo? = null
        val columns = arrayOf(
            DBHelper.ISOCODE,
            DBHelper.LANGNAME,
            DBHelper.WORDCOUNT,
            DBHelper.MD5SUM,
            DBHelper.FULLSUM,
            DBHelper.ON_SERVER
        )
        val selection = String.format(NAME_FMT, DBHelper.DICTNAME, name)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = query(TABLE_NAMES.DICTINFO, columns, selection)
            if (1 == cursor.count && cursor.moveToFirst()) {
                result = DictInfo()
                result!!.name = name
                result!!.isoCodeStr = cursor.getString(cursor.getColumnIndex(DBHelper.ISOCODE))
                result!!.wordCount = cursor.getInt(cursor.getColumnIndex(DBHelper.WORDCOUNT))
                result!!.md5Sum = cursor.getString(cursor.getColumnIndex(DBHelper.MD5SUM))
                result!!.fullSum = cursor.getString(cursor.getColumnIndex(DBHelper.FULLSUM))
                result!!.langName = cursor.getString(cursor.getColumnIndex(DBHelper.LANGNAME))
                val onServer = cursor.getInt(cursor.getColumnIndex(DBHelper.ON_SERVER))
                result!!.onServer = ON_SERVER.entries[onServer]

                // int loc = cursor.getInt(cursor.getColumnIndex(DBHelper.LOC));
                // Log.d( TAG, "dictsGetInfo(): read sum %s/loc %d for %s", result.md5Sum,
                //        loc, name );
            }
            cursor.close()
        }
        if (null != result) {
            if (null == result!!.fullSum) { // force generation
                result = null
            }
        }

        // Log.d( TAG, "dictsGetInfo(%s) => %s", name, result );
        return result
    }

    @JvmStatic
    fun dictsSetInfo(
        context: Context, dal: DictAndLoc,
        info: DictInfo
    ) {
        Assert.assertTrueNR(null != info.isoCode())
        val selection = String.format(NAME_FMT, DBHelper.DICTNAME, dal.name)
        val values = ContentValues()
			.putAnd(DBHelper.ISOCODE, info.isoCode().toString())
			.putAnd(DBHelper.LANGNAME, info.langName)
			.putAnd(DBHelper.WORDCOUNT, info.wordCount)
			.putAnd(DBHelper.MD5SUM, info.md5Sum)
			.putAnd(DBHelper.FULLSUM, info.fullSum)
			.putAnd(DBHelper.LOCATION, dal.loc.ordinal)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val result = update(TABLE_NAMES.DICTINFO, values, selection)
            if (0 == result) {
                values.put(DBHelper.DICTNAME, dal.name)
                val rowid = insert(TABLE_NAMES.DICTINFO, values)
                Assert.assertTrueNR(0 < rowid)
            }
        }
    }

    @JvmStatic
    fun dictsMoveInfo(
        context: Context, name: String?,
        fromLoc: DictLoc?, toLoc: DictLoc
    ) {
        val selection = String.format(
            DBHelper.DICTNAME + "='%s' AND " + DBHelper.LOCATION + "=%d",
            name, toLoc.ordinal
        )
        val values = ContentValues()
			.putAnd(DBHelper.LOCATION, toLoc.ordinal)
        initDB(context)
        synchronized(s_dbHelper!!) {
            update(TABLE_NAMES.DICTINFO, values, selection)
            update(TABLE_NAMES.DICTBROWSE, values, selection)
        }
    }

    @JvmStatic
    fun dictsRemoveInfo(context: Context, name: String) {
        val selection = String.format("%s=?", DBHelper.DICTNAME)
        val args = arrayOf(name)
        initDB(context)
        synchronized(s_dbHelper!!) {
            var removed = delete(TABLE_NAMES.DICTINFO, selection, args)
            // Log.d( TAG, "removed %d rows from %s", removed, DBHelper.TABLE_NAME_DICTINFO );
            removed = delete(TABLE_NAMES.DICTBROWSE, selection, args)
        }
    }

    @JvmStatic
    fun updateServed(
        context: Context, dal: DictAndLoc,
        served: Boolean
    ) {
        // For some reason, loc is sometimes wrong. So just flag the thing
        // wherever it is.
        val selection = String.format(DBHelper.DICTNAME + "='%s' ", dal.name)
        val onServer = if (served) ON_SERVER.YES else ON_SERVER.NO
        val values = ContentValues()
			.putAnd(DBHelper.ON_SERVER, onServer.ordinal)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val count = update(TABLE_NAMES.DICTINFO, values, selection)
            Log.d(TAG, "update(%s) => %d rows affected", selection, count)
            Assert.assertTrueNR(count > 0)
        }
    }

    @JvmStatic
    fun addToStudyList(
        context: Context, word: String,
        isoCode: ISOCode
    ) {
        val values = ContentValues()
			.putAnd(DBHelper.WORD, word)
			.putAnd(DBHelper.ISOCODE, isoCode.toString())
        initDB(context)
        synchronized(s_dbHelper!!) { insert(TABLE_NAMES.STUDYLIST, values) }
        notifyStudyListListeners(word, isoCode)
    }

    @JvmStatic
    fun studyListLangs(context: Context): ArrayList<ISOCode> {
        val results = ArrayList<ISOCode>()
        val columns = arrayOf(DBHelper.ISOCODE)
        val groupBy = columns[0]
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = s_db!!.query(
                TABLE_NAMES.STUDYLIST.toString(), columns,
                null, null, groupBy, null, null
            )
            val count = cursor.count
            if (0 < count) {
                var index = 0
                val colIndex = cursor.getColumnIndex(columns[0])
                while (cursor.moveToNext()) {
                    results.add(ISOCode(cursor.getString(colIndex)))
                }
            }
            cursor.close()
        }
		return results
    }

    @JvmStatic
    fun studyListWords(context: Context, isoCode: ISOCode?): ArrayList<String> {
        var result = ArrayList<String>()
        val selection = String.format("%s = '%s'", DBHelper.ISOCODE, isoCode)
        val columns = arrayOf(DBHelper.WORD)
        val orderBy = columns[0]
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor = DBHelper.query(
                s_db!!, TABLE_NAMES.STUDYLIST, columns,
                selection, orderBy
            )
            val count = cursor.count
            if (0 < count) {
                var index = 0
                val colIndex = cursor.getColumnIndex(DBHelper.WORD)
                while (cursor.moveToNext()) {
                    result.add(cursor.getString(colIndex))
                }
            }
            cursor.close()
        }
        return result
    }

    @JvmOverloads
    fun studyListClear(context: Context, isoCode: ISOCode, words: Array<String>? = null) {
        var selection = String.format("%s = '%s'", DBHelper.ISOCODE, isoCode)
        if (null != words) {
            selection += String.format(
                " AND %s in ('%s')", DBHelper.WORD,
                TextUtils.join("','", words)
            )
        }
        initDB(context)
        synchronized(s_dbHelper!!) { delete(TABLE_NAMES.STUDYLIST, selection) }
    }

    fun saveXlations(
        context: Context, locale: String?,
        data: Map<String?, String?>?, blessed: Boolean
    ) {
        if (null != data && 0 < data.size) {
            val blessedLong = (if (blessed) 1 else 0).toLong()
            val iter = data.keys.iterator()
            var insertQuery = ("insert into %s (%s, %s, %s, %s) "
                    + " VALUES (?, ?, ?, ?)")
            insertQuery = String.format(
                insertQuery, TABLE_NAMES.LOC,
                DBHelper.KEY, DBHelper.LOCALE,
                DBHelper.BLESSED, DBHelper.XLATION
            )
            var updateQuery = ("update %s set %s = ? "
                    + " WHERE %s = ? and %s = ? and %s = ?")
            updateQuery = String.format(
                updateQuery, TABLE_NAMES.LOC,
                DBHelper.XLATION, DBHelper.KEY,
                DBHelper.LOCALE, DBHelper.BLESSED
            )
            initDB(context)
            synchronized(s_dbHelper!!) {
                val insertStmt = s_db!!.compileStatement(insertQuery)
                val updateStmt = s_db!!.compileStatement(updateQuery)
                while (iter.hasNext()) {
                    val key = iter.next()
                    val xlation = data[key]
                    // DbgUtils.logf( "adding key %s, xlation %s, locale %s, blessed: %d",
                    //                key, xlation, locale, blessedLong );
                    insertStmt.bindString(1, key)
                    insertStmt.bindString(2, locale)
                    insertStmt.bindLong(3, blessedLong)
                    insertStmt.bindString(4, xlation)
                    try {
                        insertStmt.execute()
                    } catch (sce: SQLiteConstraintException) {
                        updateStmt.bindString(1, xlation)
                        updateStmt.bindString(2, key)
                        updateStmt.bindString(3, locale)
                        updateStmt.bindLong(4, blessedLong)
                        try {
                            updateStmt.execute()
                        } catch (ex: Exception) {
                            Log.ex(TAG, ex)
                            Assert.failDbg()
                        }
                    }
                }
            }
        }
    }

    // You can't have an array of paramterized types in java, so we'll let the
    // caller cast.
    fun getXlations(
        context: Context,
        locale: String?
    ): Array<Any> {
        val local =
            HashMap<String, String>()
        val blessed =
            HashMap<String, String>()
        val selection = String.format(
            "%s = '%s'", DBHelper.LOCALE,
            locale
        )
        val columns =
            arrayOf(DBHelper.KEY, DBHelper.XLATION, DBHelper.BLESSED)
        initDB(context)
        synchronized(s_dbHelper!!) {
            val cursor =
                query(TABLE_NAMES.LOC, columns, selection)
            val keyIndex = cursor.getColumnIndex(DBHelper.KEY)
            val valueIndex = cursor.getColumnIndex(DBHelper.XLATION)
            val blessedIndex = cursor.getColumnIndex(DBHelper.BLESSED)
            while (cursor.moveToNext()) {
                val key = cursor.getString(keyIndex)
                val value = cursor.getString(valueIndex)
                val map =
                    if (0 == cursor.getInt(blessedIndex)) local else blessed
                map[key] = value
            }
            cursor.close()
        }
        return arrayOf(local, blessed)
    }

    fun dropXLations(context: Context, locale: String?) {
        val selection = String.format(
            "%s = '%s'", DBHelper.LOCALE,
            locale
        )
        initDB(context)
        synchronized(s_dbHelper!!) { delete(TABLE_NAMES.LOC, selection) }
    }

    private fun setStringForSync(db: SQLiteDatabase?, key: String, value: String?) {
        val selection = String.format("%s = '%s'", DBHelper.KEY, key)
        val values = ContentValues()
			.putAnd(DBHelper.VALUE, value)
        val result = DBHelper.update(db, TABLE_NAMES.PAIRS, values, selection).toLong()
        if (0L == result) {
            values.put(DBHelper.KEY, key)
            DBHelper.insert(db, TABLE_NAMES.PAIRS, values)
        }
    }

    private fun delStringsLikeSync(db: SQLiteDatabase?, like: String) {
        val selection = String.format("%s LIKE '%s'", DBHelper.KEY, like)
        delete(db, TABLE_NAMES.PAIRS, selection, null)
    }

    private fun getStringForSyncSel(db: SQLiteDatabase?, selection: String): String? {
        var result: String? = null
        val columns = arrayOf(DBHelper.VALUE)
        // If there are multiple matches, we want to use the newest. At least
        // that's the right move where a devID's key has been changed with
        // each upgrade.
        val orderBy = ROW_ID + " DESC"
        val cursor = DBHelper.query(db, TABLE_NAMES.PAIRS, columns, selection, orderBy)
        // Log.d( TAG, "getStringForSyncSel(selection=%s)", selection );
        val tooMany = BuildConfig.DEBUG && 1 < cursor.count
        if (cursor.moveToNext()) {
            result = cursor.getString(cursor.getColumnIndex(DBHelper.VALUE))
        }
        cursor.close()
        return result
    }

    private fun getStringForSync(
        db: SQLiteDatabase?, key: String,
        keyEndsWith: String?, dflt: String?
    ): String? {
        var dflt = dflt
        var selection = String.format("%s = '%s'", DBHelper.KEY, key)
        val found = false
        var oneResult = getStringForSyncSel(db, selection)
        if (null == oneResult && null != keyEndsWith) {
            selection = String.format("%s LIKE '%%%s'", DBHelper.KEY, keyEndsWith)
            oneResult = getStringForSyncSel(db, selection)
            // Log.d( TAG, "getStringForSync() LIKE case: %s => %s", keyEndsWith, oneResult );
            if (null != oneResult) {
                setStringForSync(db, key, oneResult) // store so won't need LIKE in future
            }
        }
        if (null != oneResult) {
            dflt = oneResult
        }
        return dflt
    }

    private fun getModStringFor(context: Context, key: String, proc: Modifier): String? {
        var result: String? = null
        initDB(context)
        synchronized(s_dbHelper!!) {
            result = getStringForSync(s_db, key, null, null)
            result = proc.modifySync(result)
            setStringForSync(s_db, key, result)
        }
        return result
    }

    @JvmStatic
    fun setStringFor(context: Context, key: String, value: String?) {
        initDB(context)
        synchronized(s_dbHelper!!) { setStringForSync(s_db, key, value) }
    }

    @JvmStatic
    fun getStringFor(context: Context, key: String): String? {
        return getStringFor(context, key, null)
    }

    @JvmStatic
    fun getStringFor(context: Context, key: String, dflt: String?): String? {
        return getStringFor(context, key, null, dflt)
    }

    fun getStringFor(
        context: Context, key: String,
        keyEndsWith: String?, dflt: String?
    ): String? {
        var dflt = dflt
        initDB(context)
        synchronized(s_dbHelper!!) { dflt = getStringForSync(s_db, key, keyEndsWith, dflt) }
        return dflt
    }

    @JvmStatic
    fun setIntFor(context: Context, key: String, value: Int) {
        // Log.d( TAG, "DBUtils.setIntFor(key=%s, val=%d)", key, value );
        val asStr = String.format("%d", value)
        setStringFor(context, key, asStr)
    }

    @JvmStatic
    fun getIntFor(context: Context, key: String, dflt: Int): Int {
        var dflt = dflt
        val asStr = getStringFor(context, key, null)
        if (null != asStr) {
            dflt = asStr.toInt()
        }
        // Log.d( TAG, "DBUtils.getIntFor(key=%s)=>%d", key, dflt );
        return dflt
    }

    @JvmStatic
    fun setLongFor(context: Context, key: String, value: Long) {
        // Log.d( TAG, "DBUtils.setIntFor(key=%s, val=%d)", key, value );
        val asStr = String.format("%d", value)
        setStringFor(context, key, asStr)
    }

    fun getLongFor(context: Context, key: String, dflt: Long): Long {
        var dflt = dflt
        val asStr = getStringFor(context, key, null)
        if (null != asStr) {
            dflt = asStr.toLong()
        }
        // Log.d( TAG, "DBUtils.getIntFor(key=%s)=>%d", key, dflt );
        return dflt
    }

    @JvmStatic
    fun setBoolFor(context: Context, key: String, value: Boolean) {
        // Log.df( "DBUtils.setBoolFor(key=%s, val=%b)", key, value );
        val asStr = String.format("%b", value)
        setStringFor(context, key, asStr)
    }

    @JvmStatic
    fun getBoolFor(context: Context, key: String, dflt: Boolean): Boolean {
        var dflt = dflt
        val asStr = getStringFor(context, key, null)
        if (null != asStr) {
            dflt = asStr.toBoolean()
        }
        // Log.df( "DBUtils.getBoolFor(key=%s)=>%b", key, dflt );
        return dflt
    }

    @JvmStatic
    fun getIncrementIntFor(
        context: Context, key: String, dflt: Int,
        incr: Int
    ): Int {
        val proc: Modifier = object : Modifier {
            override fun modifySync(curVal: String?): String? {
                val `val` = curVal?.toInt() ?: 0
                return String.format("%d", `val` + incr)
            }
        }
        val newVal = getModStringFor(context, key, proc)
        // DbgUtils.logf( "getIncrementIntFor(%s) => %d", key, asInt );
        return newVal!!.toInt()
    }

    @JvmStatic
    fun setBytesFor(context: Context, key: String, bytes: ByteArray?) {
        // DbgUtils.logf( "setBytesFor: writing %d bytes", bytes.length );
        val asStr = Utils.base64Encode(bytes)
        setStringFor(context, key, asStr)
    }

    @JvmStatic
    fun getBytesFor(context: Context, key: String): ByteArray? {
        return getBytesFor(context, key, null)
    }

    fun getBytesFor(context: Context, key: String, keyEndsWith: String?): ByteArray? {
        var bytes: ByteArray? = null
        val asStr = getStringFor(context, key, keyEndsWith, null)
        if (null != asStr) {
            bytes = Utils.base64Decode(asStr)
        }
        return bytes
    }

    @JvmStatic
    fun getSerializableFor(context: Context, key: String): Serializable? {
        var value: Serializable? = null
        val str64 = getStringFor(context, key, "")
        if (str64 != null) {
            value = Utils.string64ToSerializable(str64) as Serializable?
        }
        return value
    }

    @JvmStatic
    fun setSerializableFor(
        context: Context, key: String,
        value: Serializable?
    ) {
        val str64 = if (null == value) "" else Utils.serializableToString64(value)
        setStringFor(context, key, str64)
    }

    fun appendLog(tag: String?, msg: String) {
        appendLog(XWApp.getContext(), msg)
    }

    private fun appendLog(context: Context, msg: String) {
        if (0 < LOGLIMIT) {
            val values = ContentValues()
				.putAnd(DBHelper.MESSAGE, msg)
            initDB(context)
            synchronized(s_dbHelper!!) {
                val rowid = insert(TABLE_NAMES.LOGS, values)
                if (0L == rowid % (LOGLIMIT / 10)) {
                    val where = String.format(
                        "not rowid in (select rowid from %s order by TIMESTAMP desc limit %d)",
                        TABLE_NAMES.LOGS, LOGLIMIT
                    )
                    val nGone = delete(TABLE_NAMES.LOGS, where)
                    Log.i(TAG, "appendLog(): deleted %d rows", nGone)
                }
            }
        }
    }

    // Copy my .apk to the Downloads directory, from which a user could more
    // easily share it with somebody else. Should be blocked for apks
    // installed from the Play store since viral distribution isn't allowed,
    // but might be helpful in other cases. Need to figure out how to expose
    // it, and how to recommend transmissions. E.g. gmail doesn't let me
    // attach an .apk even if I rename it.
    fun copyApkToDownloads(context: Context) {
        try {
            val myName = context.packageName
            val pm = context.packageManager
            val appInfo = pm.getApplicationInfo(myName, 0)
            val srcPath = File(appInfo.publicSourceDir)
            var destPath = Environment
                .getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            destPath = File(destPath, context.getString(R.string.app_name) + ".apk")
            val src = FileInputStream(srcPath)
            val dest = FileOutputStream(destPath)
            copyStream(dest, src)
        } catch (ex: Exception) {
            Log.e(TAG, "copyApkToDownloads(): got ex: %s", ex)
        }
    }

    private val variantDBName: String
        private get() = String.format(
            "%s_%s", DBHelper.getDBName(),
            BuildConfig.FLAVOR
        )

    // private static void clearChatHistoryString( Context context, long rowid )
    // {
    //     ContentValues values = new ContentValues();
    //     values.putNull( DBHelper.CHAT_HISTORY );
    //     updateRow( context, DBHelper.TABLE_NAMES.SUM, rowid, values );
    // }
    private fun showHiddenGames(context: Context, db: SQLiteDatabase?) {
        Log.d(TAG, "showHiddenGames()")
        var query = ("select " + ROW_ID + " from summaries WHERE NOT groupid"
                + " IN (SELECT " + ROW_ID + " FROM groups);")
        var ids: MutableList<String?>? = null
        val cursor = db!!.rawQuery(query, null)
        if (0 < cursor.count) {
            ids = ArrayList()
            val indx = cursor.getColumnIndex(ROW_ID)
            while (cursor.moveToNext()) {
                val rowid = cursor.getLong(indx)
                ids.add(String.format("%d", rowid))
            }
        }
        cursor.close()
        if (null != ids) {
            val name = LocUtils.getString(context, R.string.recovered_group)
            var groupid = getGroupImpl(name)
            if (GROUPID_UNSPEC.toLong() == groupid) {
                groupid = addGroupImpl(name)
            }
            query = String.format(
                "UPDATE summaries SET groupid = %d"
                        + " WHERE rowid IN (%s);", groupid,
                TextUtils.join(",", ids)
            )
            db.execSQL(query)
        }
    }

    private fun initDB(context: Context) {
        synchronized(DBUtils::class.java) {
            if (null == s_dbHelper) {
                Assert.assertNotNull(context)
                s_dbHelper = DBHelper(context)
                // force any upgrade
                s_dbHelper!!.getWritableDatabase().close()
                s_db = s_dbHelper!!.getWritableDatabase()

                // Workaround for bug somewhere. Run this once on startup
                // before anything else uses the db.
                showHiddenGames(context, s_db)
            }
        }
    }

    @JvmStatic
    fun hideGames(context: Context, rowid: Long) {
        if (BuildConfig.NON_RELEASE) {
            val nonID = 500 + Utils.nextRandomInt() % 1000
            val query = String.format(
                "UPDATE summaries set GROUPID = %d"
                        + " WHERE rowid = %d", nonID, rowid
            )
            initDB(context)
            synchronized(s_dbHelper!!) { s_db!!.execSQL(query) }
        }
    }

    private fun updateRowImpl(
        table: TABLE_NAMES,
        rowid: Long, values: ContentValues
    ): Int {
        val selection = String.format(ROW_ID_FMT, rowid)
        return DBHelper.update(s_db, table, values, selection)
    }

    private fun updateRow(
        context: Context, table: TABLE_NAMES,
        rowid: Long, values: ContentValues
    ) {
        initDB(context)
        synchronized(s_dbHelper!!) {
            val result = updateRowImpl(table, rowid, values)
            if (0 == result) {
                Log.w(TAG, "updateRow failed")
            }
        }
    }

    private fun maxVISID(db: SQLiteDatabase?): Int {
        var result = 1
        val query = String.format(
            "SELECT max(%s) FROM %s", DBHelper.VISID,
            TABLE_NAMES.SUM
        )
        var cursor: Cursor? = null
        try {
            cursor = db!!.rawQuery(query, null)
            if (1 == cursor.count && cursor.moveToFirst()) {
                result = 1 + cursor.getInt(0)
            }
        } finally {
            cursor?.close()
        }
        return result
    }

    private fun notifyStudyListListeners(word: String, isoCode: ISOCode) {
        synchronized(s_slListeners) {
            for (listener in s_slListeners) {
                listener.onWordAdded(word, isoCode)
            }
        }
    }

    private fun notifyListeners(
        context: Context, rowid: Long,
        change: GameChangeType
    ) {
        synchronized(s_listeners) {
            val iter: Iterator<DBChangeListener> = s_listeners.iterator()
            while (iter.hasNext()) {
                iter.next().gameSaved(context, rowid, change)
            }
        }
    }

    // Trivial one-item cache.  Typically bytes are read three times
    // in a row, so this saves two DB accesses per game opened.  Could
    // use a HashMap, but then lots of half-K byte[] chunks would fail
    // to gc.  This is good enough.
    private fun getCached(rowid: Long): ByteArray? {
        return if (s_cachedRowID == rowid) s_cachedBytes else null
    }

    private fun setCached(rowid: Long, bytes: ByteArray?) {
        s_cachedRowID = rowid
        s_cachedBytes = bytes
    }

    private fun query(table: TABLE_NAMES, columns: Array<String>, selection: String?): Cursor {
        return DBHelper.query(s_db, table, columns, selection)
    }

    private fun delete(
        db: SQLiteDatabase?,
        table: TABLE_NAMES,
        selection: String,
        args: Array<String>? = null
    ): Int {
        return db!!.delete(table.toString(), selection, args)
    }

    private fun delete(table: TABLE_NAMES, selection: String): Int {
        return delete(s_db, table, selection, null)
    }

    private fun delete(table: TABLE_NAMES, selection: String, args: Array<String>): Int {
        return delete(s_db, table, selection, args)
    }

    private fun update(table: TABLE_NAMES, values: ContentValues?, selection: String?): Int {
        return DBHelper.update(s_db, table, values, selection)
    }

    private fun insert(table: TABLE_NAMES, values: ContentValues): Long {
        return DBHelper.insert(s_db, table, values)
    }

    enum class GameChangeType {
        GAME_CHANGED,
        GAME_CREATED,
        GAME_DELETED,
        GAME_MOVED
    }

    interface DBChangeListener {
        fun gameSaved(
            context: Context, rowid: Long,
            change: GameChangeType?
        )
    }

    interface StudyListListener {
        fun onWordAdded(word: String, isoCode: ISOCode)
    }

    class HistoryPair(var msg: String, var playerIndx: Int, var ts: Int)
    class SentInvite(var mMeans: InviteMeans, var mTarget: String, var mTimestamp: Date) :
        Serializable {
        override fun equals(otherObj: Any?): Boolean {
            var result = false
            if (otherObj is SentInvite) {
                val other = otherObj
                result =
                    mMeans == other.mMeans && mTarget == other.mTarget && mTimestamp == other.mTimestamp
            }
            return result
        }
    }

    class SentInvitesInfo(var m_rowid: Long) :
        Serializable /* Serializable b/c passed as param to alerts */ {
        private val mSents: ArrayList<SentInvite>
        private var m_cachedCount = 0
        var remotesRobots = false
            private set

        override fun equals(other: Any?): Boolean {
            var result = null != other && other is SentInvitesInfo
            if (result) {
                val it = other as SentInvitesInfo?
                if (m_rowid == it!!.m_rowid && it.mSents.size == mSents.size && it.m_cachedCount == m_cachedCount) {
                    var ii = 0
                    while (result && ii < mSents.size) {
                        result = it.mSents[ii] == mSents[ii]
                        ++ii
                    }
                }
            }
            // Log.d( TAG, "equals() => %b", result );
            return result
        }

        init {
            mSents = ArrayList()
        }

        fun addEntry(means: InviteMeans, target: String, ts: Date) {
            mSents.add(SentInvite(means, target, ts))
            m_cachedCount = -1
        }

        fun getLastDev(means: InviteMeans): String? {
            var result: String? = null
            for (si in mSents) {
                if (means == si.mMeans) {
                    result = si.mTarget
                    break
                }
            }
            return result
        }

        val minPlayerCount: Int
            // There will be lots of duplicates, but we can't detect them all. BUT
            get() {
                if (-1 == m_cachedCount) {
                    val count = mSents.size
                    val hashes: MutableMap<InviteMeans, MutableSet<String>> = HashMap()
                    var fakeCount = 0 // make all null-targets count for one
                    for (ii in 0 until count) {
                        val si = mSents[ii]
                        val means = si.mMeans
                        var devs: MutableSet<String>
                        if (!hashes.containsKey(means)) {
                            devs = HashSet()
                            hashes[means] = devs
                        }
                        devs = hashes[means]!!
                        var target: String? = si.mTarget
                        if (null == target) {
                            target = String.format("%d", ++fakeCount)
                        }
                        devs.add(target)
                    }

                    // Now find the max
                    m_cachedCount = 0
                    for (means in InviteMeans.entries) {
                        if (hashes.containsKey(means)) {
                            val siz = hashes[means]!!.size
                            m_cachedCount += siz
                        }
                    }
                }
                return m_cachedCount
            }

        fun getAsText(context: Context): String {
            val result: String
            val count = mSents.size
            if (0 == count) {
                result = LocUtils.getString(context, R.string.no_invites)
            } else {
                val strs: MutableList<String?> = ArrayList()
                for (si in mSents) {
                    val means = si.mMeans
                    val target = si.mTarget
                    val timestamp = si.mTimestamp.toString()
                    var msg: String? = null
                    when (means) {
                        InviteMeans.SMS_DATA -> {
                            val fmt = R.string.invit_expl_sms_fmt
                            msg = LocUtils.getString(context, fmt, target, timestamp)
                        }

                        InviteMeans.SMS_USER -> {
                            val fmt = R.string.invit_expl_usrsms_fmt
                            msg = LocUtils.getString(context, fmt, timestamp)
                        }

                        InviteMeans.BLUETOOTH -> {
                            val devName = BTUtils.nameForAddr(target)
                            msg = LocUtils.getString(
                                context, R.string.invit_expl_bt_fmt,
                                devName, timestamp
                            )
                        }

                        InviteMeans.RELAY -> Assert.failDbg()
                        InviteMeans.MQTT -> {
                            val player = XwJNI.kplr_nameForMqttDev(target)
                            if (null != player) {
                                msg = LocUtils.getString(
                                    context,
                                    R.string.invit_expl_player_fmt,
                                    player, timestamp
                                )
                                break
                            }
                            msg = LocUtils.getString(
                                context, R.string.invit_expl_notarget_fmt,
                                means.toString(), timestamp
                            )
                        }

                        else -> msg = LocUtils.getString(
                            context, R.string.invit_expl_notarget_fmt,
                            means.toString(), timestamp
                        )
                    }
                    strs.add(msg)
                }
                result = TextUtils.join("\n\n", strs)
            }
            return result
        }

        fun getKPName(context: Context): String? {
            var mqttID: String? = null
            for (si in mSents) {
                val means = si.mMeans
                if (means == InviteMeans.MQTT) {
                    mqttID = si.mTarget
                    break
                }
            }
            var result: String? = null
            if (null != mqttID) {
                result = XwJNI.kplr_nameForMqttDev(mqttID)
            }
            Log.d(TAG, "getKPName() => %s", result)
            return result
        }

        fun setRemotesRobots() {
            remotesRobots = true
        }
    }

    class NeedsNagInfo(
        @JvmField var m_rowid: Long, @JvmField var m_nextNag: Long, lastMove: Long,
        role: DeviceRole
    ) {
        @JvmField
        var m_lastMoveMillis: Long
        val isSolo: Boolean

        init {
            m_lastMoveMillis = 1000 * lastMove
            isSolo = DeviceRole.SERVER_STANDALONE == role
        }
    }

    // Groups stuff
    class GameGroupInfo(@JvmField var m_name: String, @JvmField var m_count: Int, @JvmField var m_expanded: Boolean) {
        @JvmField
        var m_lastMoveTime: Long = 0
        @JvmField
        var m_hasTurn = false
        @JvmField
        var m_turnLocal = false
        override fun toString(): String {
            return String.format("GameGroupInfo: {name: %s}", m_name)
        }
    }

    private interface Modifier {
        fun modifySync(curVal: String?): String?
    }
}

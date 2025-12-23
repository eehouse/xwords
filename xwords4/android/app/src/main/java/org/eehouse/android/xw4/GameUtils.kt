/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.provider.Settings
import android.provider.Telephony
import android.text.Html
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

import java.io.File
import java.io.FileOutputStream
import java.util.Arrays
import kotlin.math.min

import org.eehouse.android.xw4.GameLock.GameLockedException
import org.eehouse.android.xw4.NFCUtils.nfcAvail
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.jni.DrawCtx
import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.jni.LastMoveInfo
import org.eehouse.android.xw4.jni.UtilCtxt
import org.eehouse.android.xw4.loc.LocUtils

object GameUtils {
    private val TAG: String = GameUtils::class.java.simpleName

    const val INTENT_KEY_ROWID: String = "rowid"
    const val INTENT_KEY_GAMEREF: String = "gr"

    private var s_minScreen: Int? = null

    // Used to determine whether to resend all messages on networking coming
    // back up.  The length of the array determines the number of times in the
    // interval we'll do a send.
    private val s_sendTimes = HashMap<CommsConnType?, LongArray>()
    private const val RESEND_INTERVAL_SECS = (60 * 5 // 5 minutes
            ).toLong()

    private val s_syncObj = Any()

    fun savedGame(context: Context, rowid: Long): ByteArray {
        var result: ByteArray? = null
        GameLock.tryLockRO(rowid).use { lock ->
            lock?.let {
                result = savedGame(context, it)
            }
        }
        if (null == result) {
            val msg = ("savedGame(): unable to get lock; holder dump: "
                    + GameLock.getHolderDump(rowid))
            Log.d(TAG, msg)
            if (BuildConfig.NON_RELEASE) {
                Utils.emailAuthor(context, msg)
            }
            throw NoSuchGameException(rowid)
        }

        return result
    }

    fun savedGame(context: Context, lock: GameLock): ByteArray? {
        Assert.fail()
        return null // DBUtils.loadGame(context, lock)
    } // savedGame

    fun haveWithGameID(context: Context, gameID: Int): Boolean {
        return haveWithGameID(context, gameID, -1)
    }

    fun haveWithGameID(context: Context, gameID: Int, channel: Int): Boolean {
        val map = DBUtils.getRowIDsAndChannels(context, gameID)
        var found = 0 < map.size
        if (found) {
            if ( XWPrefs.getPrefsBoolean(context, R.string.key_allowDupGameIDs,
                                         false) ) {
                found = (-1 == channel || map.values.contains(channel))
            }
            // && (-1 == channel || map.values.contains(channel))
        }
        // Log.d( TAG, "haveWithGameID(gameID=%X, channel=%d) => %b",
        //        gameID, channel, found );
        return found
    }

    fun deleteGame(
        context: Context, gr: GameRef,
        informNow: Boolean, skipTell: Boolean
    ) {
        if (!skipTell) {
            tellDied(context, gr, informNow)
        }
        Utils.cancelNotification(context, gr)
        GameMgr.deleteGame(gr)
    }

    fun getName(context: Context, rowid: Long): String? {
        var result = DBUtils.getName(context, rowid)
        if (null == result || 0 == result.length) {
            val visID = DBUtils.getVisID(context, rowid)
            result = LocUtils.getString(context, R.string.game_fmt, visID)
        }
        return result
    }

    fun makeDefaultName(context: Context): String {
        val count = DBUtils.getIncrementIntFor(
            context, DBUtils.KEY_NEWGAMECOUNT, 0, 1)
        return LocUtils.getString(context, R.string.game_fmt, count)
    }

    private fun giFromStream(context: Context, stream: ByteArray?): CurGameInfo? {
        var gi: CurGameInfo? = null
        Assert.failDbg()
        // if (null != stream) {
        //     gi = CurGameInfo(context)
        //     XwJNI.giFromStream(gi, stream)
        // }
        return gi
    }

    fun getThumbSize(context: Context, nCols: Int): Int {
        Log.d(TAG, "getThumbSize(nCols=$nCols)")
        if (null == s_minScreen) {
            Log.d(TAG, "s_minscreen null")
            if (context is Activity) {
                val display = context.windowManager.defaultDisplay
                val width = display.width
                val height = display.height
                s_minScreen = min(width, height)
            } else {
                Log.d(TAG, "getThumbSize(): wrong activity type")
            }
        }
        val result =
            s_minScreen?.let { minScreen ->
                val pct = XWPrefs.getThumbPct(context)
                Log.d(TAG, "getThumbSize(): pct: $pct")
                val dim = minScreen * pct / 100
                dim - (dim % nCols)
            } ?: 0
        Log.d(TAG, "getThumbSize() => $result")
        return result
    }

    fun makeRematch(
        context: Context, srcRowid: Long,
        groupID: Long, gameName: String?,
        newOrder: Array<Int>
    ): Long {
        Assert.failDbg()
        var rowid = DBUtils.ROWID_NOTFOUND
        // GameWrapper.make(context, srcRowid).use { gw ->
        //     if (null != gw) {
        //         val util: UtilCtxt = UtilCtxt(GameRef(0))
        //         val cp = CommonPrefs.get(context)
        //         XwJNI.game_makeRematch(gw.gamePtr()!!, util, cp,
        //                                gameName, newOrder)
        //             // .use { gamePtrNew ->
        //         // if (null != gamePtrNew) {
        //         // rowid = saveNewGame1(
        //         // context, gamePtrNew,
        //         // groupID, gameName
        //         // )
        //     // }
        //     // }
        //     }
        // }
        // Log.d(TAG, "makeRematch() => %d", rowid)
        return rowid
    }

    fun getGameWithChannel(context: Context, nli: NetLaunchInfo): Long
    {
        var found = DBUtils.ROWID_NOTFOUND
        val rowids = DBUtils.getRowIDsAndChannels(context, nli.gameID())
        for (rowid in rowids.keys) {
            if (0 == nli.forceChannel || nli.forceChannel == rowids[rowid]) {
                found = rowid
                break
            }
        }
        return found
    }

    fun saveGame(
        context: Context, bytes: ByteArray?,
        lock: GameLock?, setCreate: Boolean
    ): Long {
        return DBUtils.saveGame(context, lock!!, bytes!!, setCreate)
    }

    @JvmOverloads
    fun saveNewGame(
        context: Context, bytes: ByteArray,
        groupID: Long = DBUtils.GROUPID_UNSPEC
    ): GameLock? {
        return DBUtils.saveNewGame(context, bytes, groupID, null)
    }

    fun makeSaveNew(
        context: Context, gi: CurGameInfo,
        groupID: Long, gameName: String?
    ): Long {
        Assert.assertTrueNR(DeviceRole.ROLE_STANDALONE == gi.deviceRole)
        return makeSaveNew(context, gi, null, null, groupID, gameName, null)
    }

    private fun makeSaveNew(
        context: Context, gi: CurGameInfo,
        selfAddr: CommsAddrRec?, hostAddr: CommsAddrRec?,
        groupID: Long, gameName: String?,
        invitee: CommsAddrRec?
    ): Long {
        Assert.failDbg()
        // var groupID = groupID
        // if (DBUtils.GROUPID_UNSPEC == groupID) {
        //     groupID = 0 // XWPrefs.getDefaultNewGameGroup(context)
        //     Assert.failDbg()
        // }

        // val gamePtr = XwJNI.initNew(
        // gi, selfAddr, hostAddr,
        // null as UtilCtxt?, null as DrawCtx?,
        // CommonPrefs.get(context), null as TransportProcs?)

        // invitee?.let {
        //     val summary = GameSummary(gi)
        //     XwJNI.game_summarize(gamePtr, summary)
        //     val nli = NetLaunchInfo(context, summary, gi)
        //     Log.d(TAG, "passing %s to comms_invite()", nli)
        //     // XwJNI.comms_invite(gamePtr, nli, it, false)
        // }

        // var rowid = DBUtils.ROWID_NOTFOUND
        // val bytes = XwJNI.game_saveToStream(gamePtr, gi)
        // bytes?.let {
        //     DBUtils.saveNewGame(context, it, groupID, gameName).use { lock ->
        //         rowid = lock!!.rowid
        //     }
        // }
        return 0
    }

    fun makeNewMultiGame1(context: Context, nli: NetLaunchInfo): GameRef? {
        return makeNewMultiGame2(
            context, nli, null as MultiMsgSink?,
            null as UtilCtxt?
        )
    }

    fun makeNewMultiGame2(
        context: Context, nli: NetLaunchInfo,
        sink: MultiMsgSink?, util: UtilCtxt?
    ): GameRef? {
        // Log.d( TAG, "makeNewMultiGame(nli=%s)", nli.toString() );
        // Called to create a client in response to invitation from host. As
        // client, it can be created knowing host's address, and with its own
        // address based on the connection types the host is using.
        val hostAddr = nli.makeAddrRec(context)
        val selfAddr = CommsAddrRec.getSelfAddr(context, hostAddr.conTypes!!)
        val isHost = false

        return makeNewMultiGame6(
            context, sink, util, DBUtils.GROUPID_UNSPEC,
            selfAddr, hostAddr,
            arrayOf(nli.isoCode()),
            arrayOf(nli.dict), null, nli.nPlayersT,
            nli.nPlayersH, nli.forceChannel,
            nli.inviteID(), nli.gameID(),
            nli.gameName, isHost, nli.remotesAreRobots,
            null
        )
    }

    fun makeNewMultiGame3(
        context: Context, groupID: Long,
        gameName: String?, invitee: CommsAddrRec?
    ): GameRef? {
        return makeNewMultiGame4(
            context, groupID, null as String?,
            null as ISOCode?, null as String?,
            null as CommsConnTypeSet?, gameName,
            invitee
        )
    }

    fun makeNewMultiGame4(
        context: Context, groupID: Long,
        dict: String?, isoCode: ISOCode?,
        jsonData: String?,
        selfSet: CommsConnTypeSet?,
        gameName: String?, invitee: CommsAddrRec?
    ): GameRef? {
        val inviteID = makeRandomID()
        return makeNewMultiGame5(
            context, groupID, inviteID, dict, isoCode,
            jsonData, selfSet, gameName, invitee
        )
    }

    private fun makeNewMultiGame5(
        context: Context, groupID: Long,
        inviteID: String, dict: String?,
        isoCode: ISOCode?, jsonData: String?,
        selfSet: CommsConnTypeSet?,
        gameName: String?, invitee: CommsAddrRec?
    ): GameRef? {
        var selfSet = selfSet
        val langArray = arrayOf(isoCode)
        val dictArray = arrayOf(dict)
        if (null == selfSet) {
            selfSet = XWPrefs.getAddrTypes(context)
        }

        // Silently add this to any networked game if our device supports
        // it. comms is unhappy if we later pass in a message using an address
        // type the game doesn't have in its set.
        if (nfcAvail(context)[0]) {
            selfSet!!.add(CommsConnType.COMMS_CONN_NFC)
        }

        val selfAddr = CommsAddrRec(selfSet!!)
            .populate(context)
        val forceChannel = 0
        return makeNewMultiGame6(
            context, null as MultiMsgSink?, null as UtilCtxt?,
            groupID, selfAddr, null as CommsAddrRec?,
            langArray, dictArray, jsonData, 2, 1,
            forceChannel, inviteID, 0, gameName,
            true, false, invitee
        )
    }

    private fun makeNewMultiGame6(
        context: Context, sink: MultiMsgSink?,
        util: UtilCtxt?, groupID: Long,
        selfAddr: CommsAddrRec, hostAddr: CommsAddrRec?,
        isoCode: Array<ISOCode?>, dict: Array<String?>,
        jsonData: String?,
        nPlayersT: Int, nPlayersH: Int,
        forceChannel: Int, inviteID: String,
        gameID: Int, gameName: String?,
        isHost: Boolean, localsRobots: Boolean,
        invitee: CommsAddrRec?
    ): GameRef? {
        val rowid = DBUtils.ROWID_NOTFOUND

        Assert.assertNotNull(inviteID)
        val gi = CurGameInfo(context, inviteID)
        gi.setFrom(jsonData)
        gi.setLang(context, isoCode[0], dict[0])
        gi.forceChannel = forceChannel
        isoCode[0] = gi.isoCode()
        dict[0] = gi.dictName
        gi.setNPlayers(nPlayersT, nPlayersH, localsRobots)
        gi.juggle()
        if (0 != gameID) {
            gi.gameID = gameID
        }
        if (isHost) {
            gi.deviceRole = DeviceRole.ROLE_ISHOST
        }
        // Will need to add a setNPlayers() method to gi to make this
        // work
        Assert.assertTrue(gi.nPlayers == nPlayersT)
        return makeNewMultiGame8(
            context, sink, gi, selfAddr, hostAddr,
            util, groupID, gameName, invitee
        )
    }

    fun makeNewMultiGame7(
        context: Context, gi: CurGameInfo,
        selfSet: CommsConnTypeSet?, gameName: String?
    ): GameRef? {
        val selfAddr = CommsAddrRec(selfSet!!)
            .populate(context)
        return makeNewMultiGame8(
            context, null as MultiMsgSink?,
            gi, selfAddr, null as CommsAddrRec?,
            null as UtilCtxt?,
            DBUtils.GROUPID_UNSPEC, gameName, null
        )
    }

    private fun makeNewMultiGame8(
        context: Context, sink: MultiMsgSink?,
        gi: CurGameInfo, selfAddr: CommsAddrRec,
        hostAddr: CommsAddrRec?, util: UtilCtxt?,
        groupID: Long, gameName: String?,
        invitee: CommsAddrRec?
    ): GameRef? {
        Assert.failDbg()
        return null
        // var selfAddr: CommsAddrRec? = selfAddr
        // if (null == selfAddr) {
        //     selfAddr = CommsAddrRec.getSelfAddr(context, gi)
        // }
        // val rowid = makeSaveNew(
        //     context, gi, selfAddr, hostAddr, groupID,
        //     gameName, invitee
        // )
        // if (null != sink) {
        //     sink.rowID = rowid
        // }

        // if (DBUtils.ROWID_NOTFOUND != rowid) {
        //     // Use tryLock in case we're on UI thread. It's guaranteed to
        //     // succeed because we just created the rowid.
        //     GameLock.tryLock(rowid).use { lock ->
        //         Assert.assertNotNull(lock)
        //         applyChanges2(
        //             context, sink, gi, util, hostAddr,
        //             null as Map<CommsConnType, BooleanArray>?,
        //             lock!!, false /*forceNew*/
        //         )
        //     }
        // }

        // return rowid
    }

    fun inviteURLToClip(context: Context, gr: GameRef) {
        Utils.launch {
            gr.inviteUrl(context).also {
                Utils.stringToClip(context, it)
            }

            Utils.showToast(context, R.string.invite_copied)
        }
    }

    fun launchEmailInviteActivity(activity: Activity, gr: GameRef) {
        Utils.launch {
            makeInviteMessage(activity, gr, R.string.invite_htm_fmt)?.let { message ->
                val intent = Intent()
                intent.setAction(Intent.ACTION_SEND)
                val subject = LocUtils.getString(activity, R.string.invite_subject)
                intent.putExtra(Intent.EXTRA_SUBJECT, subject)
                intent.putExtra(Intent.EXTRA_TEXT, Html.fromHtml(message))

                var attach: File? = null
                val tmpdir =
                    if (BuildConfig.ATTACH_SUPPORTED) DictUtils.getDownloadDir(activity) else null
                if (null != tmpdir) { // no attachment
                    val gi = gr.getGI()!!
                    val summary = gr.getSummary()!!
                    val nli = NetLaunchInfo(activity, summary, gi, 1)  // 1: nPlayers
                    attach = makeJsonFor(tmpdir, nli)
                }

                if (null == attach) { // no attachment
                    intent.setType("message/rfc822")
                } else {
                    val mime = LocUtils.getString(activity, R.string.invite_mime)
                    intent.setType(mime)
                    val uri = Uri.fromFile(attach)
                    intent.putExtra(Intent.EXTRA_STREAM, uri)
                }

                val choiceType = LocUtils.getString(activity, R.string.invite_chooser_email)
                val chooserMsg =
                    LocUtils.getString(
                        activity, R.string.invite_chooser_fmt,
                        choiceType
                    )
                activity.startActivity(Intent.createChooser(intent, chooserMsg))
            }
        }
    }

    // There seems to be no standard on how to launch an SMS app to send a
    // message. So let's gather here the stuff that works, and try in order
    // until something succeeds.
    //
    // And, added later and without the ability to test all of these, let's
    // not include a phone number.
    fun launchSMSInviteActivity(
        activity: Activity,
        gr: GameRef
    ) {
        Utils.launch {
        val message = makeInviteMessage(
            activity, gr,
            R.string.invite_sms_fmt
        )
        if (null != message) {
            var succeeded = false
            val defaultSmsPkg = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT
            ) Telephony.Sms.getDefaultSmsPackage(activity)
            else Settings.Secure.getString(
                activity.contentResolver,
                "sms_default_application"
            )
            Log.d(TAG, "launchSMSInviteActivity(): default app: %s", defaultSmsPkg)

            var ii = 0
            outer@ while (!succeeded) {
                var intent = when (ii) {
                    0 -> Intent(Intent.ACTION_SEND)
                        .setPackage(defaultSmsPkg)
                        .setType("text/plain")
                        .putExtra(Intent.EXTRA_TEXT, message)
                        .putExtra("sms_body", message)

                    1 -> Intent(Intent.ACTION_SENDTO)
                        .putExtra("sms_body", message)
                        .setPackage(defaultSmsPkg)

                    2 -> Intent(Intent.ACTION_VIEW)
                        .putExtra("sms_body", message)

                    else -> break@outer
                }
                try {
                    if (intent.resolveActivity(activity.packageManager) != null) {
                        activity.startActivity(intent)
                        succeeded = true
                    }
                } catch (ex: Exception) {
                    Log.e(TAG, "launchSMSInviteActivity(): ex: %s", ex)
                }
                ++ii
            }
            if (!succeeded) {
                DbgUtils.showf(activity, R.string.sms_invite_fail)
            }
        }
    }
}

    private suspend fun makeInviteMessage(
        context: Context, gr: GameRef, fmtID: Int
    ): String? {
        var result =
            gr.inviteUrl(context).let {
                LocUtils.getString(context, fmtID, it)
            } ?: null
        return result
    }

    // fun dictNames(context: Context, lock: GameLock): Array<String?>? {
    //     var result: Array<String?>? = null
    //     val stream = savedGame(context, lock)
    //     val gi = giFromStream(context, stream)
    //     if (null != gi) {
    //         result = gi.dictNames()
    //     }
    //     return result
    // }

    // suspend fun dictNames(
    //     context: Context, rowid: Long,
    //     missingLang: Array<ISOCode?>? = null
    // ): Array<String?>? {
    //     // val gi: CurGameInfo?
    //     // if (JNIThread.gameIsOpen(rowid)) {
    //     //     val jnit = JNIThread.getRetained(rowid)
    //     //     gi = jnit?.getGI()
    //     //     jnit?.release()
    //     // } else {
    //     //     gi = giFromStream(context, savedGame(context, rowid))
    //     // }

    //     val gi = gr.getGI()

    //     var result = gi?.let {
    //         missingLang?.set(0, gi.isoCode())
    //         gi.dictNames()
    //     }

    //     return result
    // }

    // fun gameDictsHere(context: Context, lock: GameLock): Boolean {
    //     val gameDicts = dictNames(context, lock)
    //     return null != gameDicts && gameDictsHere(context, null, gameDicts)
    // }

    // // Return true if all dicts present.  Return list of those that
    // // are not.
    // @JvmOverloads
    // fun gameDictsHere(
    //     context: Context, gr: GameRef,
    //     missingNames: Array<Array<String?>?>? = null,
    //     missingLang: Array<ISOCode?>? = null
    // ): Boolean {
    //     val gameDicts = dictNames(context, gr, missingLang)
    //     return (null != gameDicts
    //             && gameDictsHere(context, missingNames, gameDicts))
    // }

    // fun gameDictsHere(
    //     context: Context,
    //     missingNames: Array<Array<String?>?>?,
    //     gameDicts: Array<String?>
    // ): Boolean {
    //     val installed = DictUtils.dictList(context).orEmpty()

    //     val missingSet = HashSet(Arrays.asList(*gameDicts))
    //     missingSet.remove(null)
    //     var allHere = 0 != missingSet.size // need some non-null!
    //     if (allHere) {
    //         for (dal in installed) {
    //             missingSet.remove(dal.name)
    //         }
    //         allHere = 0 == missingSet.size
    //     } else {
    //         Log.w(TAG, "gameDictsHere: game has no dicts!")
    //     }
    //     if (null != missingNames) {
    //         missingNames[0] =
    //             missingSet.toTypedArray<String?>()
    //     }

    //     return allHere
    // }

    fun newName(context: Context): String {
        return "untitled"

        // String name = null;
        // Integer num = 1;
        // int ii;
        // long[] rowids = DBUtils.gamesList( context );
        // String fmt = context.getString( R.string.gamef );

        // while ( name == null ) {
        //     name = String.format( fmt + XWConstants.GAME_EXTN, num );
        //     for ( ii = 0; ii < files.length; ++ii ) {
        //         if ( files[ii].equals(name) ) {
        //             ++num;
        //             name = null;
        //         }
        //     }
        // }
        // return name;
    }

    private fun isGame(file: String): Boolean {
        return file.endsWith(XWConstants.GAME_EXTN)
    }

    private fun makeLaunchExtras(rowid: Long): Bundle {
        val bundle = Bundle()
        bundle.putLong(INTENT_KEY_ROWID, rowid)
        return bundle
    }

    private fun makeLaunchExtras(gr: GameRef): Bundle {
        val bundle = Bundle()
        bundle.putLong(INTENT_KEY_GAMEREF, gr.gr)
        return bundle
    }

    @JvmOverloads
    fun launchGame(
        delegator: Delegator, rowid: Long,
        moreExtras: Bundle? = null
    ) {
        val extras = makeLaunchExtras(rowid)
        if (null != moreExtras) {
            extras.putAll(moreExtras)
        }

        delegator.addFragment(BoardFrag.newInstance(delegator), extras)
    }

    @JvmOverloads
    fun launchGame(
        delegator: Delegator, gr: GameRef,
        moreExtras: Bundle? = null
    ) {
        val extras = makeLaunchExtras(gr)
        if (null != moreExtras) {
            extras.putAll(moreExtras)
        }

        delegator.addFragment(BoardFrag.newInstance(delegator), extras)
    }

    fun feedMessage(
        context: Context, rowid: Long, msg: ByteArray?,
        ret: CommsAddrRec?, sink: MultiMsgSink?,
        bmr: BackMoveResult?, isLocalOut: BooleanArray?
    ): Boolean {
        Assert.failDbg()
        return false
        // Assert.assertTrue(DBUtils.ROWID_NOTFOUND != rowid)
        // var draw = false
        // Assert.assertTrue(-1L != rowid)
        // if (null != msg) {
        //     // timed lock: If a game is opened by BoardActivity just
        //     // as we're trying to deliver this message to it it'll
        //     // have the lock and we'll never get it.  Better to drop
        //     // the message than fire the hung-lock assert.  Messages
        //     // belong in local pre-delivery storage anyway.
        //     try {
        //         GameLock.lock(rowid, 150).use { lock ->
        //             if (null != lock) {
        //                 val gi = CurGameInfo(context)
        //                 val feedImpl = FeedUtilsImpl(context, rowid, gi)
        //                 loadMakeGame(
        //                     context, gi, feedImpl,
        //                     sink, lock
        //                 ).use { gamePtr ->
        //                     if (null != gamePtr) {
        //                         XwJNI.comms_resendAll(gamePtr, false, false)

        //                         Assert.assertNotNull(ret)
        //                         draw = false // XwJNI.game_receiveMessage(gamePtr, msg, ret)
        //                         XwJNI.comms_ackAny(gamePtr)

        //                         // update gi to reflect changes due to messages
        //                         XwJNI.game_getGi(gamePtr, gi)

        //                         // if (draw && XWPrefs.getThumbEnabled(context)) {
        //                         //     val bitmap = takeSnapshot(context, gamePtr, gi)
        //                         //     DBUtils.saveThumbnail(context, lock, bitmap)
        //                         // }

        //                         if (null != bmr) {
        //                             if (null != feedImpl.m_chat) {
        //                                 bmr.m_chat = feedImpl.m_chat
        //                                 bmr.m_chatFrom = feedImpl.m_chatFrom
        //                                 bmr.m_chatTs = feedImpl.m_ts
        //                             } else {
        //                                 Assert.failDbg()
        //                                 // bmr.m_lmi = XwJNI.model_getPlayersLastScore(gamePtr, -1)
        //                             }
        //                         }

        //                         saveGame(context, gamePtr, gi, lock, false)
        //                         val summary = summarize(
        //                             context, lock,
        //                             gamePtr, gi
        //                         )
        //                         if (null != isLocalOut) {
        //                             isLocalOut[0] = (0 <= summary.turn
        //                                     && gi.players[summary.turn]!!.isLocal)
        //                         }
        //                     }
        //                     val flags = setFromFeedImpl(feedImpl)
        //                     if (GameSummary.MSG_FLAGS_NONE != flags) {
        //                         draw = true
        //                         val curFlags = DBUtils.getMsgFlags(context, rowid)
        //                         DBUtils.setMsgFlags(context, rowid, flags or curFlags)
        //                     }
        //                 }
        //             }
        //         }
        //     } catch (gle: GameLockedException) {
        //         DbgUtils.toastNoLock(
        //             TAG, context, rowid,
        //             "feedMessage(): dropping message"
        //                     + " for rowid %d", rowid
        //         )
        //     }
        // }
        // return draw
    }

    // This *must* involve a reset if the language is changing!!!
    // Which isn't possible right now, so make sure the old and new
    // dict have the same langauge code.
    fun replaceDicts(
        context: Context, rowid: Long,
        oldDict: String?, newDict: String?
    ): Boolean {
        Assert.failDbg()
        var success = false
        // GameLock.lock(rowid, 300).use { lock ->
        //     success = null != lock
        //     if (!success) {
        //         DbgUtils.toastNoLock(
        //             TAG, context, rowid,
        //             "replaceDicts(): rowid %d",
        //             rowid
        //         )
        //     } else {
        //         val stream = savedGame(context, lock!!)
        //         val gi = giFromStream(context, stream)
        //         success = null != gi
        //         if (!success) {
        //             Log.e(TAG, "replaceDicts(): unable to load rowid %d", rowid)
        //         } else {
        //             // first time required so dictNames() will work
        //             gi!!.replaceDicts(context, newDict)

        //             // XwJNI.initFromStream(
        //             //     rowid, stream!!, gi, null, null,
        //             //     CommonPrefs.get(context), null
        //             // ).use { gamePtr ->
        //             //     // second time required as game_makeFromStream can overwrite
        //             //     gi.replaceDicts(context, newDict)

        //             //     saveGame(context, gamePtr, gi, lock, false)
        //             //     summarize(context, lock, gamePtr!!, gi)
        //             // }
        //         }
        //     }
        // }
        return success
    } // replaceDicts

    fun applyChanges1(
        context: Context, gi: CurGameInfo,
        selfAddr: CommsAddrRec?,
        disab: Map<CommsConnType, BooleanArray>?,
        lock: GameLock, forceNew: Boolean
    ) {
        applyChanges2(
            context, null as MultiMsgSink?, gi, null as UtilCtxt?,
            selfAddr, disab, lock, forceNew
        )
    }

    private fun applyChanges2(
        context: Context, sink: MultiMsgSink?,
        gi: CurGameInfo, util: UtilCtxt?,
        selfAddr: CommsAddrRec?,
        disab: Map<CommsConnType, BooleanArray>?,
        lock: GameLock, forceNew: Boolean
    ) {
        Assert.failDbg()
        // This should be a separate function, commitChanges() or
        // somesuch.  But: do we have a way to save changes to a gi
        // that don't reset the game, e.g. player name for standalone
        // games?
        // var madeGame = false
        // val cp = CommonPrefs.get(context)

        // if (forceNew) {
        //     Assert.failDbg()
        //     // tellDied(context, lock, true)
        // } else {
        //     val stream = savedGame(context, lock)
        //     XwJNI.initFromStream(
        //         lock.rowid, stream!!,
        //         CurGameInfo(context),
        //         null, null, cp, null
        //     ).use { gamePtr ->
        //         if (null != gamePtr) {
        //             applyChangesImpl(context, sink, gi, disab, lock, gamePtr)
        //             madeGame = true
        //         }
        //     }
        // }

        // if (forceNew || !madeGame) {
        //     val hostAddr: CommsAddrRec? = null
        //     XwJNI.initNew(
        //         gi, selfAddr, hostAddr,
        //         util, null as DrawCtx?, cp, sink
        //     ).use { gamePtr ->
        //         if (null != gamePtr) {
        //             applyChangesImpl(context, sink, gi, disab, lock, gamePtr)
        //         }
        //     }
        // }
    }

    fun formatGameID(gameID: Int): String {
        Assert.assertTrue(0 != gameID)
        // I used to truncate this for smaller SMS messages, but gameID has
        // become important enough that we want to use all 32 bits.
        return String.format("%X", gameID)
    }

    fun makeRandomID(): String {
        val rint = newGameID()
        return formatGameID(rint)
    }

    private fun newGameID(): Int {
        var rint: Int
        do {
            rint = Utils.nextRandomInt()
        } while (0 == rint)
        Log.i(TAG, "newGameID()=>%X (%d)", rint, rint)
        return rint
    }

    fun postMoveNotification(
        context: Context, gr: GameRef,
        bmr: BackMoveResult?, isTurnNow: Boolean,
        gameName: String = ""
    ) {
        bmr?.let {
            val intent = GamesListDelegate.makeGamerefIntent(context, gr)
            var msg: String? = null
            var titleID = 0
            if (null != bmr.m_chat) {
                titleID = R.string.notify_chat_title_fmt
                msg = if (null != bmr.m_chatFrom) {
                    LocUtils.getString(
                        context, R.string.notify_chat_body_fmt,
                        bmr.m_chatFrom, bmr.m_chat
                    )
                } else {
                    bmr.m_chat
                }
            } else if (null != bmr.m_lmi) {
                titleID = if (isTurnNow) {
                    R.string.notify_title_turn_fmt
                } else {
                    R.string.notify_title_fmt
                }
                msg = bmr.m_lmi!!.format(context)
            }

            if (0 != titleID) {
                val title = LocUtils.getString(
                    context, titleID,
                    gameName
                )
                Utils.postNotification(context, intent, title, msg, gr)
            }
        } ?: run {
            Log.d(
                TAG, "postMoveNotification(): posting nothing for lack"
                        + " of brm"
            )
        }
    }

    private fun postMoveDroppedForDictNotification(
        context: Context, rowid: Long,
        gameID: Int, isoCode: ISOCode
    ) {
        val intent = GamesListDelegate.makeGameIDIntent(context, gameID)

        val langName = DictLangCache.getLangNameForISOCode(context, isoCode)
        val body = LocUtils.getString(
            context, R.string.no_dict_for_move_fmt,
            langName
        )
        Assert.failDbg()
        /*Utils.postNotification(
            context, intent, R.string.no_dict_for_move_title,
            body, rowid
        )*/
    }

    fun postInvitedNotification(
        context: Context, gameID: Int,
        body: String?, gr: GameRef
    ) {
        val intent = GamesListDelegate.makeGameIDIntent(context, gameID)
        Assert.failDbg()
        /*Utils.postNotification(
            context, intent, R.string.invite_notice_title,
            body, gr.gr
        )*/
    }

    // I think this goes away, with the work being done in common code
    private fun tellDied( context: Context, gr: GameRef, informNow: Boolean )
    {
        Utils.launch {
            gr.getGI()?.let { gi ->
                if (DeviceRole.ROLE_STANDALONE != gi.deviceRole) {
                    val gameID = gi.gameID

                    val addrs = gr.getAddrs()
                    for (addr in addrs!!) {
                        val conTypes = addr!!.conTypes
                        for (typ in conTypes!!) {
                            when (typ) {
                                CommsConnType.COMMS_CONN_RELAY -> {}
                                CommsConnType.COMMS_CONN_BT -> BTUtils.gameDied(
                                                                   context,
                                                                   addr.bt_hostName!!,
                                                                   addr.bt_btAddr,
                                                                   gameID
                                                               )

                                CommsConnType.COMMS_CONN_SMS -> NBSProto.gameDied(
                                                                    context,
                                                                    gameID,
                                                                    addr.sms_phone
                                                                )

                                CommsConnType.COMMS_CONN_P2P -> WiDirService.gameDied(
                                                                    addr.p2p_addr,
                                                                    gameID
                                                                )

                                CommsConnType.COMMS_CONN_MQTT ->
                                    MQTTUtils.gameDied( context, addr.mqtt_devID!!, gameID )

                                else -> Log.d(TAG, "tellDied(): unexpected type $typ")
                            }
                        }
                    }
                }
            }
        }
    }

    private fun makeJsonFor(dir: File, nli: NetLaunchInfo): File? {
        var result: File? = null
        if (BuildConfig.ATTACH_SUPPORTED) {
            val data = nli.makeLaunchJSON().toByteArray()

            val file = File(dir, String.format("invite_%d", nli.gameID()))
            try {
                val fos = FileOutputStream(file)
                fos.write(data, 0, data.size)
                fos.close()
                result = file
            } catch (ex: Exception) {
                Log.ex(TAG, ex)
            }
        }
        return result
    }

    interface ResendDoneProc {
        fun onResendDone(context: Context, numSent: Int)
    }

    class NoSuchGameException(private val m_rowID: Long) : RuntimeException() {
        init {
            Log.i(TAG, "NoSuchGameException(rowid=%d)", m_rowID)
            // DbgUtils.printStack( TAG );
        }
    }

    class BackMoveResult {
        var m_lmi: LastMoveInfo? = null // instantiated on demand
        var m_chat: String? = null
        var m_chatFrom: String? = null
        var m_chatTs: Long = 0
    }
}

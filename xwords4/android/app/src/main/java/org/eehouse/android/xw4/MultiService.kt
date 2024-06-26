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

import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle

import java.util.Collections
import java.util.concurrent.ConcurrentHashMap

import org.eehouse.android.xw4.MainActivity
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.loc.LocUtils

class MultiService {
    enum class DictFetchOwner {
        _NONE,
        OWNER_SMS,
        OWNER_RELAY,
        OWNER_BT,
        OWNER_P2P,
        OWNER_MQTT,
    }

    // these do not currently pass between devices so they can change.
    enum class MultiEvent {
        _INVALID,
        BAD_PROTO_BT,
        BAD_PROTO_SMS,
        APP_NOT_FOUND_BT,
        BT_ENABLED,
        BT_DISABLED,
        NEWGAME_SUCCESS,
        NEWGAME_FAILURE,
        NEWGAME_DUP_REJECTED,
        MESSAGE_ACCEPTED,
        MESSAGE_REFUSED,
        MESSAGE_NOGAME,
        MESSAGE_RESEND,
        MESSAGE_FAILOUT,
        MESSAGE_DROPPED,

        SMS_RECEIVE_OK,
        SMS_SEND_OK,
        SMS_SEND_FAILED,
        SMS_SEND_FAILED_NORADIO,
        SMS_SEND_FAILED_NOPERMISSION,

        BT_GAME_CREATED,
    }

    interface MultiEventListener {
        fun eventOccurred(event: MultiEvent, vararg args: Any?)
    }

    private val mLis: MutableSet<MultiEventListener> = Collections
        .newSetFromMap(ConcurrentHashMap())

    fun setListener(li: MultiEventListener) {
        mLis.add(li)
    }

    fun clearListener(li: MultiEventListener) {
        Assert.assertTrueNR(mLis.contains(li))
        mLis.remove(li)
    }

    fun postEvent(event: MultiEvent, vararg args: Any?): Int {
        // don't just return size(): concurrency doesn't guarantee isn't
        // changed

        val count = mLis
            .map{it.eventOccurred(event, *args)}
            .size
        return count
    }

    companion object {
        private val TAG: String = MultiService::class.java.simpleName

        const val FORCECHANNEL: String = "FC"
        const val LANG: String = "LANG"
        const val ISO: String = "ISO"
        const val DICT: String = "DICT"
        const val GAMEID: String = "GAMEID"
        const val INVITEID: String = "INVITEID" // relay only
        const val ROOM: String = "ROOM"
        const val GAMENAME: String = "GAMENAME"
        const val NPLAYERST: String = "NPLAYERST"
        const val NPLAYERSH: String = "NPLAYERSH"
        const val REMOTES_ROBOTS: String = "RR"
        const val INVITER: String = "INVITER"
        private const val OWNER = "OWNER"
        const val BT_NAME: String = "BT_NAME"
        const val BT_ADDRESS: String = "BT_ADDRESS"
        const val P2P_MAC_ADDRESS: String = "P2P_MAC_ADDRESS"
        const val MQTT_DEVID: String = "MQTT_DEVID"
        private const val NLI_DATA = "nli"
        const val DUPEMODE: String = "du"

        private const val ACTION_FETCH_DICT = "_afd"
        private const val FOR_MISSING_DICT = "_fmd"

        fun makeMissingDictIntent(
            context: Context, nli: NetLaunchInfo,
            owner: DictFetchOwner
        ): Intent {
            val intent = Intent(context, MainActivity::class.java) // PENDING TEST THIS!!!
            intent.setAction(ACTION_FETCH_DICT)
            intent.putExtra(ISO, nli.isoCodeStr)
            intent.putExtra(DICT, nli.dict)
            intent.putExtra(OWNER, owner.ordinal)
            intent.putExtra(NLI_DATA, nli.toString())
            intent.putExtra(FOR_MISSING_DICT, true)
            return intent
        }

        fun isMissingDictBundle(args: Bundle?): Boolean {
            val result = args!!.getBoolean(FOR_MISSING_DICT, false)
            return result
        }

        fun isMissingDictIntent(intent: Intent): Boolean {
            val action = intent.action
            val result =
                null != action && action == ACTION_FETCH_DICT && isMissingDictBundle(intent.extras)
            // DbgUtils.logf( "isMissingDictIntent(%s) => %b", intent.toString(), result );
            return result
        }

        fun getMissingDictData(context: Context, intent: Intent): NetLaunchInfo
        {
            Assert.assertTrueNR(isMissingDictIntent(intent))
            val nliData = intent.getStringExtra(NLI_DATA)!!
            val nli = NetLaunchInfo.makeFrom(context, nliData)
            return nli!!
        }

        fun missingDictDialog(
            context: Context, intent: Intent,
            onDownload: DialogInterface.OnClickListener?,
            onDecline: DialogInterface.OnClickListener?
        ): Dialog {
            val lang = intent.getIntExtra(LANG, -1)
            val isoCode = ISOCode.newIf(intent.getStringExtra(ISO))!!
            val langName = DictLangCache.getLangNameForISOCode(context, isoCode)
            val dict = intent.getStringExtra(DICT)
            val inviter = intent.getStringExtra(INVITER)
            val msgID = if ((null == inviter)) R.string.invite_dict_missing_body_noname_fmt
            else R.string.invite_dict_missing_body_fmt
            val msg = LocUtils.getString(context!!, msgID, inviter, dict, langName)

            return LocUtils.makeAlertBuilder(context)
                .setTitle(R.string.invite_dict_missing_title)
                .setMessage(msg)
                .setPositiveButton(R.string.button_download, onDownload)
                .setNegativeButton(R.string.button_decline, onDecline)
                .create()
        }

        fun postMissingDictNotification(
            content: Context,
            intent: Intent?, id: Int
        ) {
            Utils.postNotification(
                content!!, intent, R.string.missing_dict_title,
                R.string.missing_dict_detail, id
            )
        }

        // resend the intent, but only if the dict it names is here.  (If
        // it's not, we may need to try again later, e.g. because our cue
        // was a focus gain.)
        fun returnOnDownload(context: Context, intent: Intent): Boolean {
            var downloaded = isMissingDictIntent(intent)
            if (downloaded) {
                val isoCode = ISOCode.newIf(intent.getStringExtra(ISO))
                val dict = intent.getStringExtra(DICT)!!
                downloaded = DictLangCache.haveDict(context, isoCode, dict)
                if (downloaded) {
                    val ordinal = intent.getIntExtra(OWNER, -1)
                    if (-1 == ordinal) {
                        Log.w(TAG, "unexpected OWNER")
                    } else {
                        val owner = DictFetchOwner.entries[ordinal]
                        when (owner) {
                            DictFetchOwner.OWNER_SMS -> NBSProto.onGameDictDownload(context, intent)
                            DictFetchOwner.OWNER_RELAY, DictFetchOwner.OWNER_BT, DictFetchOwner.OWNER_MQTT -> GamesListDelegate.onGameDictDownload(
                                context,
                                intent
                            )

                            else -> Assert.failDbg()
                        }
                    }
                }
            }
            return downloaded
        }
    }
}

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

import android.app.Activity
import android.app.AlertDialog
import android.app.ProgressDialog
import android.content.DialogInterface
import android.os.Handler

import java.io.Serializable

import org.eehouse.android.xw4.MultiService.MultiEvent
import org.eehouse.android.xw4.Utils.ISOCode

class DlgDelegate(
    private val mActivity: Activity, private val mDlgt: DelegateBase
) {
    enum class Action {
        SKIP_CALLBACK,

        // GameListDelegate
        RESET_GAMES,
        DELETE_GAMES,
        DELETE_GROUPS,
        OPEN_GAME,
        CLEAR_SELS,
        NEW_NET_GAME,
        SET_HIDE_NEWGAME_BUTTONS,
        DWNLD_LOC_DICT,
        NEW_GAME_DFLT_NAME,
        SEND_EMAIL,
        CLEAR_LOG_DB,
        QUARANTINE_CLEAR,
        QUARANTINE_DELETE,
        APPLY_CONFIG,
        LAUNCH_THEME_CONFIG,
        LAUNCH_THEME_COLOR_CONFIG,
        SEND_LOGS,
        OPEN_BYOD_DICT,
        BACKUP_DO,
        BACKUP_LOADDB,
        BACKUP_OVERWRITE,
        BACKUP_RETRY,
        LAUNCH_AFTER_DEL,
        RESTART,
        NOTIFY_PERMS,
        CLEAR_STATS,

        // BoardDelegate
        UNDO_LAST_ACTION,
        LAUNCH_INVITE_ACTION,
        COMMIT_ACTION,
        SHOW_EXPL_ACTION,
        PREV_HINT_ACTION,
        NEXT_HINT_ACTION,
        JUGGLE_ACTION,
        FLIP_ACTION,
        UNDO_ACTION,
        CHAT_ACTION,
        START_TRADE_ACTION,
        LOOKUP_ACTION,
        BUTTON_BROWSE_ACTION,
        VALUES_ACTION,
        SMS_CONFIG_ACTION,
        BUTTON_BROWSEALL_ACTION,
        DROP_MQTT_ACTION,
        DROP_SMS_ACTION,
        INVITE_SMS_DATA,
        BLANK_PICKED,
        TRAY_PICKED,
        INVITE_INFO,
        DISABLE_DUALPANE,
        ARCHIVE_ACTION,
        REMATCH_ACTION,
        DELETE_ACTION,
        CUSTOM_DICT_CONFIRMED,

        // Dict Browser
        FINISH_ACTION,
        DELETE_DICT_ACTION,
        UPDATE_DICTS_ACTION,
        MOVE_CONFIRMED,
        SHOW_TILES,

        // Game configs
        LOCKED_CHANGE_ACTION,
        DELETE_AND_EXIT,

        // New Game
        NEW_GAME_ACTION,

        // SMS invite
        CLEAR_ACTION,
        USE_IMMOBILE_ACTION,
        POST_WARNING_ACTION,

        // BT Invite
        OPEN_BT_PREFS_ACTION,

        // Study list
        SL_CLEAR_ACTION,
        SL_COPY_ACTION,

        // DwnldDelegate && GamesListDelegate
        STORAGE_CONFIRMED,

        // Known Players
        KNOWN_PLAYER_DELETE,

        // classify me
        ENABLE_NBS_ASK,
        ENABLE_NBS_DO,
        ENABLE_BT_DO,
        ENABLE_MQTT_DO,
        ENABLE_MQTT_DO_OR,
        DISABLE_MQTT_DO,
        DISABLE_BT_DO,
        ASKED_PHONE_STATE,
        PERMS_QUERY,
        SHOW_FAQ,
        EXPORT_THEME,
    } // Action enum


    class ActionPair(var action: Action, var buttonStr: Int) : Serializable {
        override fun equals(obj: Any?): Boolean {
            var result: Boolean
            if (BuildConfig.DEBUG) {
                result = null != obj && obj is ActionPair
                if (result) {
                    val other = obj as ActionPair?
                    result = (buttonStr == other!!.buttonStr
                            && action == other.action)
                }
            } else {
                result = super.equals(obj)
            }
            return result
        }
    }

    inner class Builder internal constructor(dlgID: DlgID) {
        private val mState: DlgState = DlgState(dlgID)
            .setPosButton(android.R.string.ok) // default
            .setAction(Action.SKIP_CALLBACK)

        fun setMessage(msg: String?): Builder {
            mState.setMsg(msg)
            return this
        }

        fun setMessageID(msgID: Int, vararg params: Any?): Builder {
            mState.setMsg(getString(msgID, *params))
            return this
        }

        fun setActionPair(action: Action, strID: Int): Builder {
            mState.setActionPair(ActionPair(action, strID))
            return this
        }

        fun setAction(action: Action): Builder {
            mState.setAction(action)
            return this
        }

        fun setPosButton(strID: Int): Builder {
            mState.setPosButton(strID)
            return this
        }

        fun setNegButton(strID: Int): Builder {
            mState.setNegButton(strID)
            return this
        }

        fun setTitle(strID: Int): Builder {
            mState.setTitle(getString(strID))
            return this
        }

        fun setTitle(str: String): Builder {
            mState.setTitle(str)
            return this
        }

        fun setParams(vararg params: Any?): Builder {
            mState.setParams(*params)
            return this
        }

        fun setNAKey(keyID: Int): Builder {
            mState.setPrefsNAKey(keyID)
            return this
        }

        fun show() {
            val naKey = mState.m_prefsNAKey
            val action = mState.m_action!!

            // Log.d( TAG, "show(): key: %d; action: %s", naKey, action );
            if (0 == naKey || !XWPrefs.getPrefsBoolean(mActivity, naKey, false)) {
                mDlgt.show(mState)
            } else if (Action.SKIP_CALLBACK != action) {
                post {
                    Log.d(TAG, "calling onPosButton()")
                    val xwact = mActivity as XWActivity
                    xwact.onPosButton(action, *mState.getParams())
                }
            }
        }
    }


    fun makeOkOnlyBuilder(msg: String): Builder {
        // return new OkOnlyBuilder( msg );

        val builder: Builder = Builder(DlgID.DIALOG_OKONLY)
            .setMessage(msg)

        return builder
    }

    fun makeOkOnlyBuilder(msgID: Int, vararg params: Any?): Builder {
        val builder: Builder = Builder(DlgID.DIALOG_OKONLY)
            .setMessageID(msgID, *params)

        return builder
    }

    private fun makeConfirmThenBuilder(action: Action): Builder {
        return Builder(DlgID.CONFIRM_THEN)
            .setAction(action)
            .setNegButton(android.R.string.cancel)
    }

    fun makeConfirmThenBuilder(action: Action, msg: String?): Builder {
        return makeConfirmThenBuilder(action)
            .setMessage(msg)
    }

    fun makeConfirmThenBuilder(
        action: Action, msgID: Int,
        vararg params: Any?
    ): Builder {
        return makeConfirmThenBuilder(action)
            .setMessageID(msgID, *params)
    }

    private fun makeNotAgainBuilder(key: Int): Builder {
        return Builder(DlgID.DIALOG_NOTAGAIN)
            .setNAKey(key)
            .setAction(Action.SKIP_CALLBACK)
            .setTitle(R.string.newbie_title)
    }

    fun makeNotAgainBuilder(key: Int, action: Action, msg: String): Builder {
        return makeNotAgainBuilder(key)
            .setMessage(msg)
            .setAction(action)
    }

    fun makeNotAgainBuilder(
        key: Int, action: Action,
        msgID: Int, vararg params: Any?
    ): Builder {
        return makeNotAgainBuilder(key)
            .setMessageID(msgID, *params)
            .setAction(action)
    }

    fun makeNotAgainBuilder(key: Int, msg: String): Builder {
        return makeNotAgainBuilder(key)
            .setMessage(msg)
    }

    fun makeNotAgainBuilder(key: Int, msgID: Int, vararg params: Any?): Builder {
        return makeNotAgainBuilder(key)
            .setMessageID(msgID, *params)
    }

    interface DlgClickNotify {
        // These are stored in the INVITES table. Don't change order
        // gratuitously
        enum class InviteMeans(val userDescID: Int, val isForLocal: Boolean) {
            SMS_DATA(R.string.invite_choice_data_sms, false),  // classic NBS-based data sms
            EMAIL(R.string.invite_choice_email, false),
            NFC(R.string.invite_choice_nfc, true),
            BLUETOOTH(R.string.invite_choice_bt, true),
            CLIPBOARD(R.string.slmenu_copy_sel, false),
            RELAY(R.string.invite_choice_relay, false),
            WIFIDIRECT(R.string.invite_choice_p2p, false),
            SMS_USER(
                R.string.invite_choice_user_sms,
                false
            ),  // just launch the SMS app, as with email
            MQTT(R.string.invite_choice_mqtt, false),
            QRCODE(R.string.invite_choice_qrcode, true),
            ;

            fun available(): Boolean {
                return this !== MQTT || MQTTUtils.MQTTSupported()
            }
        }

        fun onPosButton(action: Action, vararg params: Any?): Boolean
        fun onNegButton(action: Action, vararg params: Any?): Boolean
        fun onDismissed(action: Action, vararg params: Any?): Boolean

        fun inviteChoiceMade(action: Action, means: InviteMeans, vararg params: Any?)
    }

    interface HasDlgDelegate {
        fun makeOkOnlyBuilder(msgID: Int, vararg params: Any?): Builder
        fun makeOkOnlyBuilder(msg: String): Builder
        fun makeNotAgainBuilder(
            prefsKey: Int,
            action: Action,
            msgID: Int,
            vararg params: Any?
        ): Builder

        fun makeNotAgainBuilder(prefsKey: Int, msgID: Int, vararg params: Any?): Builder
    }

    private val m_dictName: String? = null
    private var m_progress: ProgressDialog? = null
    private val m_handler = Handler()

    fun onPausing() {
        stopProgress()
    }

    // Puts up alert asking to choose a reason to enable SMS, and on dismiss
    // calls onPosButton/onNegButton with the action and in params a Boolean
    // indicating whether enabling is now ok.
    fun showSMSEnableDialog(action: Action) {
        val state = DlgState(DlgID.DIALOG_ENABLESMS)
            .setAction(action)

        mDlgt.show(state)
    }

    fun showInviteChoicesThen(
        action: Action,
        nli: NetLaunchInfo?, nMissing: Int,
        nInvited: Int
    ) {
        val state = DlgState(DlgID.INVITE_CHOICES_THEN)
            .setAction(action)
            .setParams(nli, nMissing, nInvited)
        mDlgt.show(state)
    }

    fun launchLookup(words: Array<String>, isoCode: ISOCode?, noStudy: Boolean) {
        mDlgt.show(LookupAlert.newInstance(words, isoCode, noStudy))
    }

    fun startProgress(titleID: Int, msgID: Int, lstnr: DialogInterface.OnCancelListener?) {
        startProgress(titleID, getString(msgID), lstnr)
    }

    fun startProgress(titleID: Int, msg: String?, lstnr: DialogInterface.OnCancelListener?) {
        val title = getString(titleID)
        startProgress(title, msg, lstnr)
    }

    fun startProgress(title: String?, msg: String?, lstnr: DialogInterface.OnCancelListener?) {
        m_progress = ProgressDialog.show(mActivity, title, msg, true, true)

        if (null != lstnr) {
            m_progress!!.setCancelable(true)
            m_progress!!.setOnCancelListener(lstnr)
        }
    }

    fun setProgressMsg(id: Int) {
        val progress = m_progress
        progress?.setMessage(getString(id))
    }

    fun stopProgress() {
        if (null != m_progress) {
            m_progress!!.dismiss()
            m_progress = null
        }
    }

    fun post(runnable: Runnable?): Boolean {
        m_handler.post(runnable!!)
        return true
    }

    fun eventOccurred(
        event: MultiEvent,
        vararg args: Any?
    ) {
        var msg: String? = null
        var asToast = true
        when (event) {
            MultiEvent.MESSAGE_RESEND -> msg = getString(
                R.string.bt_resend_fmt, (args[0] as String?)!!,
                (args[1] as Long?)!!, (args[2] as Int?)!!
            )

            MultiEvent.MESSAGE_FAILOUT -> {
                msg = getString(R.string.bt_fail_fmt, (args[0] as String?)!!)
                asToast = false
            }

            else -> Log.e(TAG, "eventOccurred: unhandled event %s", event.toString())
        }
        if (null != msg) {
            val fmsg: String = msg
            val asDlg = !asToast
            post {
                if (asDlg) {
                    makeOkOnlyBuilder(fmsg).show()
                } else {
                    DbgUtils.showf(mActivity, fmsg)
                }
            }
        }
    }

    private fun getString(id: Int, vararg params: Any?): String {
        return mDlgt.getString(id, *params)
    }

    private fun getQuantityString(id: Int, quantity: Int, vararg params: Any): String {
        return mDlgt.getQuantityString(id, quantity, *params)
    }

    companion object {
        private val TAG: String = DlgDelegate::class.java.simpleName

        const val SMS_BTN: Int = AlertDialog.BUTTON_POSITIVE
        const val NFC_BTN: Int = AlertDialog.BUTTON_NEUTRAL
        const val DISMISS_BUTTON: Int = 0
    }
}

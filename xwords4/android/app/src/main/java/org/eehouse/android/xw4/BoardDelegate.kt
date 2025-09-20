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
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.Message
import android.text.TextUtils
import android.text.format.DateUtils
import android.view.KeyEvent
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.PopupMenu
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

import java.io.Serializable
import java.lang.ref.WeakReference
import kotlin.concurrent.Volatile

import org.eehouse.android.xw4.ConnStatusHandler.ConnStatusCBacks
import org.eehouse.android.xw4.DBUtils.SentInvitesInfo
import org.eehouse.android.xw4.DictUtils.ON_SERVER
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener
import org.eehouse.android.xw4.GameOverAlert.OnDoneProc
import org.eehouse.android.xw4.GameUtils.BackMoveResult
import org.eehouse.android.xw4.GameUtils.NoSuchGameException
import org.eehouse.android.xw4.MultiService.MultiEvent
import org.eehouse.android.xw4.NFCUtils.Wrapper
import org.eehouse.android.xw4.NFCUtils.Wrapper.Procs
import org.eehouse.android.xw4.NFCUtils.getNFCDevID
import org.eehouse.android.xw4.NFCUtils.makeEnableNFCDialog
import org.eehouse.android.xw4.NFCUtils.nfcAvail
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.Perms23.PermCbck
import org.eehouse.android.xw4.TilePickAlert.TilePickState
import org.eehouse.android.xw4.Toolbar.Buttons
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.gen.PrefsWrappers
import org.eehouse.android.xw4.jni.BoardDims
import org.eehouse.android.xw4.jni.BoardHandler.DrawDoneProc
import org.eehouse.android.xw4.jni.BoardHandler.NewRecentsProc
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.CommonPrefs.TileValueType
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CommsAddrRec.ConnExpl
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.jni.CurGameInfo.XWPhoniesChoice
import org.eehouse.android.xw4.jni.DUtilCtxt
import org.eehouse.android.xw4.jni.DrawCtx
import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameMgr.GroupRef
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.GameRef.GameStateInfo
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.jni.TransportProcs.TPMsgHandler
import org.eehouse.android.xw4.jni.UtilCtxt
import org.eehouse.android.xw4.loc.LocUtils

class BoardDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.board, R.menu.board_menu),
    ConnStatusCBacks, Procs,
// ,
    // TPMsgHandler,
    // View.OnClickListener, DownloadFinishedListener, 
     InvitesNeededAlert.Callbacks
{
    private val mActivity: Activity
    private var mView: BoardView? = null
    private var mGi: CurGameInfo? = null
    private var mSummary: GameSummary? = null
    private var mHandler: Handler? = null
    private var mScreenTimer: Runnable? = null
    private var mGR: GameRef? = null
    private var mToolbar: Toolbar? = null
//     private val mTradeButtons: View? = null
//     private val mExchCommmitButton: Button? = null
//     private val mExchCancelButton: Button? = null
//     private val mSentInfo: SentInvitesInfo? = null
//     private var mPermCbck: PermCbck? = null

    private var mMissingDevs: Array<String>? = null
    private var mMissingCounts: IntArray? = null
    private var mRemotesAreRobots = false
    private var mMissingMeans: InviteMeans? = null
//     private var mIsFirstLaunch = false
    private var mFiringPrefs = false
    private var mGameOver = false

//     @Volatile
//     private var mResumeSkipped = false
    private var mStartSkipped = false
    private var mGsi: GameStateInfo? = null
//     private val mShowedReInvite = false
    private var mOverNotShown = false
    private var mDropMQTTOnDismiss = false
//     private val mHaveStartedShowing = false
    private var mSawNewShown = false
    private var mNFCWrapper: NFCUtils.Wrapper? = null
    private var mGameOverAlert: GameOverAlert? = null // how to clear after?

    // Quick hack to manage a series of alerts meant to be presented
    // one-at-a-time in order. Each tests whether it's its turn, and if so
    // checks its conditions for being shown (e.g. NO_MEANS for no way to
    // communicate). If the conditions aren't met (no need to show alert), it
    // just sets the StartAlertOrder ivar to the next value and
    // exits. Otherwise it needs to ensure that however the alert it's posting
    // exits that ivar is incremented as well.
    private enum class StartAlertOrder {
        NBS_PERMS,
        NO_MEANS,
        INVITE,
        DONE
    }

    private class MySIS : Serializable {
        var toastStr: String? = null
        var words: Array<String>? = null
        var getDict: String? = null
        var inTrade = false
        var mAlertOrder = StartAlertOrder.entries[0]
    }

    private var m_mySIS: MySIS? = null
    private fun alertOrderAt(ord: StartAlertOrder): Boolean {
        var result: Boolean
        m_mySIS!!.let {
            result = it.mAlertOrder == ord
            Log.d( TAG, "alertOrderAt($ord) => $result (at ${it.mAlertOrder})")
        }
        return result
    }

    private fun alertOrderIncrIfAt(ord: StartAlertOrder) {
        Log.d( TAG, "alertOrderIncrIfAt($ord)")
        if (alertOrderAt(ord)) {
            m_mySIS!!.mAlertOrder = StartAlertOrder.entries[ord.ordinal + 1]
            doNext()
        }
    }

    private fun doNext() {
        when (m_mySIS!!.mAlertOrder) {
            StartAlertOrder.NBS_PERMS -> askNBSPermissions()
            StartAlertOrder.NO_MEANS -> warnIfNoTransport()
            StartAlertOrder.INVITE -> showInviteAlertIf()
            else -> {}
        }
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog? {
        val dlgID = alert.dlgID
        Log.d(TAG, "makeDialog(%s)", dlgID.toString())
        val lstnr: DialogInterface.OnClickListener
        var ab = makeAlertBuilder() // used everywhere...
        Assert.assertTrueNR(!isFinishing())
        val dialog =
            when (dlgID) {
            DlgID.DLG_OKONLY -> {
                val title = params[0] as Int
                if (0 != title) {
                    ab.setTitle(title)
                }
                val msg = params[1] as String
                ab.setMessage(msg)
                    .setPositiveButton(android.R.string.ok, null)
                    .create()
            }

//             DlgID.DLG_USEDICT, DlgID.DLG_GETDICT -> {
//                 val title = params[0] as Int
//                 val msg = params[1] as String
//                 lstnr = DialogInterface.OnClickListener { dlg, whichButton ->
//                     if (DlgID.DLG_USEDICT == dlgID) {
//                         setGotGameDict(m_mySIS!!.getDict)
//                     } else {
//                         DwnldDelegate.downloadDictInBack(
//                             mActivity, mGi!!.isoCode(),
//                             m_mySIS!!.getDict!!,
//                             this@BoardDelegate
//                         )
//                     }
//                 }
//                 ab.setTitle(title)
//                     .setMessage(msg)
//                     .setPositiveButton(R.string.button_yes, lstnr)
//                     .setNegativeButton(R.string.button_no, null)
//                     .create()
//             }

//             DlgID.DLG_DELETED -> {
//                 val gameName = GameUtils.getName(mActivity, mRowid)
//                 val expl = if (params.size == 0) null else params[0] as? ConnExpl
//                 var message = getString(R.string.msg_dev_deleted_fmt, gameName)
//                 if (BuildConfig.NON_RELEASE && null != expl) {
//                     message += """
                        
                        
//                         ${expl.getUserExpl(mActivity)}
//                         """.trimIndent()
//                 }
//                 ab = ab.setMessage(message)
//                     .setPositiveButton(android.R.string.ok, null)
//                 lstnr = DialogInterface.OnClickListener { dlg, whichButton -> deleteAndClose() }
//                 ab.setNegativeButton(R.string.button_delete, lstnr)
//                 ab.setNeutralButton(
//                     R.string.button_archive
//                 ) { dlg, whichButton -> showArchiveNA(false) }
//                 ab.create()
//             }

            DlgID.QUERY_TRADE, DlgID.QUERY_MOVE -> {
                val msg = params[0] as String
                lstnr = DialogInterface.OnClickListener { dialog, whichButton ->
                    mGR!!.commitTurn(true, 0, true)
                }
                ab.setMessage(msg)
                    .setPositiveButton(R.string.button_yes, lstnr)
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            DlgID.ASK_BADWORDS -> {
                val count = params[1] as Int
                val badWordsKey = params[2] as Int
                lstnr = DialogInterface.OnClickListener { dlg, bx ->
                    mGR!!.commitTurn(true, 0, false)
                }
                val lstnr2 = DialogInterface.OnClickListener { dlg, bx ->
                    mGR!!.commitTurn(true, badWordsKey, false)
                }
                val buttonTxt = LocUtils.getString(
                    mActivity,
                    R.string.buttonYesAnd
                )
                val withSaveMsg = LocUtils
                    .getQuantityString(
                        mActivity, R.plurals.yesAndMsgFmt,
                        count, buttonTxt
                    )
                ab.setTitle(R.string.phonies_found_title)
                    .setMessage(
                        """
                        ${params[0] as String}
                        
                        $withSaveMsg
                        """.trimIndent()
                    )
                    .setPositiveButton(R.string.button_yes, lstnr)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setNeutralButton(buttonTxt, lstnr2)
                    .create()
            }

            DlgID.DLG_BADWORDS, DlgID.DLG_SCORES -> {
                val title = params[0] as Int
                val msg = params[1] as String
                ab.setMessage(msg)
                if (0 != title) {
                    ab.setTitle(title)
                }
                ab.setPositiveButton(android.R.string.ok, null)
                if (DlgID.DLG_SCORES == dlgID) {
                    if (null != m_mySIS!!.words && m_mySIS!!.words!!.size > 0) {
                        val buttonTxt: String
                        val studyOn = XWPrefs.getStudyEnabled(mActivity)
                        buttonTxt = if (m_mySIS!!.words!!.size == 1) {
                            val resID =
                                if (studyOn) R.string.button_lookup_study_fmt
                                else R.string.button_lookup_fmt
                            getString(resID, m_mySIS!!.words!![0])
                        } else {
                            val resID =
                                if (studyOn) R.string.button_lookup_study
                                else R.string.button_lookup
                            getString(resID)
                        }
                        lstnr = DialogInterface.OnClickListener { dialog, whichButton ->
                            makeNotAgainBuilder(
                                R.string.key_na_lookup,
                                Action.LOOKUP_ACTION,
                                R.string.not_again_lookup
                            )
                                .show()
                        }
                        ab.setNegativeButton(buttonTxt, lstnr)
                    }
                }
                ab.create()
            }

//             DlgID.ASK_PASSWORD -> {
//                 val player = params[0] as Int
//                 val name = params[1] as String
//                 val pwdLayout = inflate(R.layout.passwd_view) as LinearLayout
//                 val edit = pwdLayout.findViewById<View>(R.id.edit) as EditText
//                 ab.setTitle(getString(R.string.msg_ask_password_fmt, name))
//                     .setView(pwdLayout)
//                     .setPositiveButton(
//                         android.R.string.ok
//                     ) { dlg, whichButton ->
//                         val pwd = edit.getText().toString()
//                         handleViaThread(
//                             JNICmd.CMD_PASS_PASSWD,
//                             player, pwd
//                         )
//                     }
//                 ab.create()
//             }

            DlgID.GET_DEVID -> {
                val et = inflate(R.layout.edittext) as EditText
                ab.setTitle(R.string.title_pasteDevid)
                    .setView(et)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setPositiveButton(
                        android.R.string.ok
                    ) { dlg, bttn ->
                        val msg = et.getText().toString()
                        post {
                            mMissingDevs = arrayOf(msg)
                            mMissingCounts = intArrayOf(1)
                            mMissingMeans = InviteMeans.MQTT
                            tryInvites()
                        }
                    }
                    .create()
            }

//             DlgID.MQTT_PEERS -> {
//                 val psv = inflate(R.layout.peers_status) as PeerStatusView
//                 val selfAddr = XwJNI.comms_getSelfAddr(mJniGamePtr)
//                 psv.configure(mGi!!.gameID, selfAddr.mqtt_devID!!)
//                 ab
//                     .setTitle(R.string.menu_about_peers)
//                     .setView(psv)
//                     .setPositiveButton(android.R.string.ok, null)
//                     .setNegativeButton(R.string.button_refresh) { dlg, bttn ->
//                         showDialogFragment(
//                             DlgID.MQTT_PEERS
//                         )
//                     }
//                     .create()
//             }

//             DlgID.ASK_DUP_PAUSE -> {
//                 val isPause = params[0] as Boolean
//                 val pauseView = (inflate(R.layout.pause_view) as ConfirmPauseView)
//                     .setIsPause(isPause)
//                 val buttonId =
//                     if (isPause) R.string.board_menu_game_pause else R.string.board_menu_game_unpause
//                 ab
//                     .setTitle(if (isPause) R.string.pause_title else R.string.unpause_title)
//                     .setView(pauseView)
//                     .setPositiveButton(buttonId) { dlg, whichButton ->
//                         val msg = pauseView.msg
//                         handleViaThread(if (isPause) JNICmd.CMD_PAUSE else JNICmd.CMD_UNPAUSE, msg)
//                     }
//                     .setNegativeButton(android.R.string.cancel, null)
//                     .create()
//             }

            DlgID.QUERY_ENDGAME -> {
                ab.setTitle(R.string.query_title)
                    .setMessage(R.string.ids_endnow)
                    .setPositiveButton( R.string.button_yes ) { dlg, item ->
                        mGR!!.endGame()
                    }
                    .setNegativeButton(R.string.button_no, null)
                    .create()
            }

            DlgID.DLG_INVITE -> iNAWrapper.make(alert, *params)
//             DlgID.ENABLE_NFC -> makeEnableNFCDialog(mActivity)
            else -> super.makeDialog(alert, *params)
            }
         return dialog
    } // makeDialog

    private var mDeletePosted = false
    private fun postDeleteOnce(expl: ConnExpl? = null) {
        if (!mDeletePosted) {
            // PENDING: could clear this if user says "ok" rather than "delete"
            mDeletePosted = true
            post { showDialogFragment(DlgID.DLG_DELETED, expl) }
        }
    }

    override fun init(savedInstanceState: Bundle?) {
        Log.d(TAG, "init()")
//         mIsFirstLaunch = null == savedInstanceState
        getBundledData(savedInstanceState)
        val devID = getNFCDevID(mActivity)
        mNFCWrapper = Wrapper.init(mActivity, this, devID)
        mView = findViewById(R.id.board_view) as BoardView

        val args = arguments!!
        //         mRowid = args.getLong(GameUtils.INTENT_KEY_ROWID, -1)
        mGR = GameRef(args.getLong(GameUtils.INTENT_KEY_GAMEREF, 0))
        mGR!!.let { gr ->
            mView!!.setUtils(BoardUtilCtxt(gr))
            Log.d(TAG, "gameRef: %X", gr.gr)
            noteOpened(mActivity, gr, this)
        }
        mOverNotShown = true
    } // init

//     private fun getLock()
//     {
//         val then = object: GameLock.GotLockProc {
//             override fun gotLock(lock: GameLock?) {
//                 if (null == lock) {
//                     finish()
//                     if (BuildConfig.REPORT_LOCKS && ++s_noLockCount == 3) {
//                         val msg = ("BoardDelegate unable to get lock; holder stack: "
//                                    + GameLock.getHolderDump(mRowid))
//                         Log.e(TAG, msg)
//                     }
//                 } else {
//                     s_noLockCount = 0
//                     mJniThreadRef = JNIThread.getRetained(lock)
//                     lock.release()

//                     // see http://stackoverflow.com/questions/680180/where-to-stop- \
//                     // destroy-threads-in-android-service-class
//                     mJniThreadRef!!.setDaemonOnce(true)
//                     mJniThreadRef!!.startOnce()
//                     setBackgroundColor()
//                     setKeepScreenOn()
//                     if (mStartSkipped) {
//                         doResume(true)
//                     }
//                     if (mResumeSkipped) {
//                         doResume(false)
//                     }

//                     mSummary?.let {
//                         if ( it.quashed && !inArchiveGroup()) {
//                             postDeleteOnce(null)
//                         }
//                     }
//                 }
//             }
//         }
//         GameLock.getLockThen(mRowid, 100L, Handler(Looper.getMainLooper()), then)
//     } // getLock

    override fun onStart() {
        super.onStart()
        if (null != mGR) {
            doResume(true)
        } else {
            mStartSkipped = true
        }
        // newThemeFeatureAlert()
    }

//     private fun newThemeFeatureAlert() {
//         if (!s_themeNAShown) {
//             s_themeNAShown = true
//             if (CommonPrefs.darkThemeEnabled(mActivity)) {
//                 val prefsName = LocUtils.getString(mActivity, R.string.theme_which)
//                 makeNotAgainBuilder(
//                     R.string.key_na_boardThemes,
//                     R.string.not_again_boardThemes_fmt,
//                     prefsName
//                 )
//                     .setTitle(R.string.new_feature_title)
//                     .setActionPair(Action.LAUNCH_THEME_CONFIG,
//                                    R.string.button_settings)
//                     .show()
//             }
//         }
//     }

    override fun onResume() {
        super.onResume()
        if (null != mGR) {
            doResume(false)
        }
        Wrapper.setResumed(mNFCWrapper, true)
        checkButtons()
        //         if (null != mJniThreadRef) {
        //             doResume(false)
        //         } else {
        //             mResumeSkipped = true
        //             getLock()
        //         }
    }

     override fun onPause() {
         Wrapper.setResumed(mNFCWrapper, false)
         closeIfFinishing(false)
         mHandler = null
         ConnStatusHandler.setHandler(null)
         pauseGame()
         super.onPause()
     }

    override fun onDestroy() {
        Log.d( TAG, "onDestroy()" )
        closeIfFinishing(true)
        GamesListDelegate.boardDestroyed(mGR!!)
        noteClosed(mGR!!)
        super.onDestroy()
        Log.d( TAG, "onDestroy() DONE" )
    }

//     @Throws(Throwable::class)
//     fun finalize() {
//         // This logging never shows up. Likely a logging limit
//         Log.d(TAG, "finalize()")
//         if (releaseThreadOnce()) {
//             Log.e(TAG, "oops! Caught the leak")
//         }
//     }

     override fun onSaveInstanceState(outState: Bundle) {
         outState.putSerializable(SAVE_MYSIS, m_mySIS)
         super.onSaveInstanceState(outState)
     }

    private fun getBundledData(bundle: Bundle?) {
        m_mySIS = bundle?.getSerializable(SAVE_MYSIS) as MySIS? ?: MySIS()
    }

    override fun onActivityResult(
        requestCode: RequestCode, resultCode: Int,
        data: Intent
    ) {
        Log.d(TAG, "onActivityResult($requestCode)")
        if (Activity.RESULT_CANCELED != resultCode) {
            val missingMeans =
                when (requestCode) {
                    RequestCode.BT_INVITE_RESULT -> InviteMeans.BLUETOOTH
                    RequestCode.SMS_DATA_INVITE_RESULT -> InviteMeans.SMS_DATA
                    RequestCode.SMS_USER_INVITE_RESULT -> InviteMeans.SMS_USER
                    RequestCode.RELAY_INVITE_RESULT -> InviteMeans.RELAY
                    RequestCode.MQTT_INVITE_RESULT -> InviteMeans.MQTT
                    RequestCode.P2P_INVITE_RESULT -> InviteMeans.WIFIDIRECT
                    else -> null
                }
            missingMeans?.let {
                // onActivityResult is called immediately *before* onResume --
                // meaning m_gi etc are still null.
                val data = data!!
                mMissingDevs = data.getStringArrayExtra(InviteDelegate.DEVS)
                mMissingCounts = data.getIntArrayExtra(InviteDelegate.COUNTS)
                mRemotesAreRobots = data.getBooleanExtra(InviteDelegate.RAR, false)
                mMissingMeans = it
                post { tryInvites() }
            }
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        // This is not called when dialog fragment comes/goes away
        if (hasFocus) {
            if (mFiringPrefs) {
                mFiringPrefs = false
                mGR!!.prefsChanged(CommonPrefs.get(mActivity))
                // in case of change...
                setBackgroundColor()
                setKeepScreenOn()
            } else {
                warnIfNoTransport()
                showInviteAlertIf()
            }
        }
    }

//     // Invitations need to check phone state to decide whether to offer SMS
//     // invitation. Complexity (showRationale) boolean is to prevent infinite
//     // loop of showing the rationale over and over. Android will always tell
//     // us to show the rationale, but if we've done it already we need to go
//     // straight to asking for the permission.
    private fun callInviteChoices() {
        if (!Perms23.NBSPermsInManifest(mActivity)) {
            showInviteChoicesThen()
        } else {
            Perms23.tryGetPermsNA(
                this, Perm.READ_PHONE_STATE,
                R.string.phone_state_rationale,
                R.string.key_na_perms_phonestate,
                Action.ASKED_PHONE_STATE
            )
        }
    }

    private fun showInviteChoicesThen() {
        Log.d(TAG, "showInviteChoicesThen()")
        val nli = nliForMe()
        if (ON_SERVER.NO != DictLangCache.getOnServer(mActivity, nli.dict)) {
            onPosButton(Action.CUSTOM_DICT_CONFIRMED, nli)
        } else {
            val txt = LocUtils
                .getString(
                    mActivity, R.string.invite_custom_warning_fmt,
                    nli.dict
                )
            makeConfirmThenBuilder(Action.CUSTOM_DICT_CONFIRMED, txt)
                .setNegButton(R.string.list_item_config)
                .setActionPair(
                    Action.DELETE_AND_EXIT,
                    R.string.button_delete_game
                )
                .setParams(nli)
                .show()
        }
     }

    private fun setTitle(gi: CurGameInfo) {
        var title = gi.gameName
        if (gi.inDuplicateMode) {
            title = LocUtils.getString(mActivity, R.string.dupe_title_fmt, title)
        }
        title?.let{setTitle(it)}
    }

    private fun initToolbar() {
        findViewById(R.id.tbar_parent_hor)?.let {
            // Wait until we're attached....
            if (null == mToolbar) {
                mToolbar = Toolbar(mActivity, this)
                populateToolbar()
            }
        }
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        mGsi?.let { gsi ->
            launch {
                val inTrade = gsi.inTrade
                Log.d(TAG, "onPrepareOptionsMenu(): inTrade: $inTrade")
                menu.setGroupVisible(R.id.group_done, !inTrade)
                menu.setGroupVisible(R.id.group_exchange, inTrade)
                val strId =
                    if (UtilCtxt.TRAY_REVEALED == gsi.trayVisState) {
                        R.string.board_menu_tray_hide
                    } else {
                        R.string.board_menu_tray_show
                    }
                val item = menu.findItem(R.id.board_menu_tray)
                item.setTitle(getString(strId))
                Utils.setItemVisible(
                    menu, R.id.board_menu_flip,
                    gsi.visTileCount >= 1
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_juggle,
                    gsi.canShuffle
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_undo_current,
                    gsi.canRedo
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_hint_prev,
                    gsi.canHint
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_hint_next,
                    gsi.canHint
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_chat,
                    gsi.canChat
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_tray,
                    !inTrade && gsi.canHideRack
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_trade,
                    gsi.canTrade
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_undo_last,
                    gsi.canUndo
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_game_pause,
                    gsi.canPause
                )
                Utils.setItemVisible(
                    menu, R.id.board_menu_game_unpause,
                    gsi.canUnpause
                )

                Utils.setItemVisible(menu, R.id.board_menu_trade_cancel, inTrade)
                Utils.setItemVisible(
                    menu, R.id.board_menu_trade_commit,
                    inTrade && gsi.tradeTilesSelected
                )
                Utils.setItemVisible(menu, R.id.board_menu_game_resign, !inTrade)
                var enable: Boolean
                val summary = mGR!!.getSummary()!!
                if (!inTrade) {
                    enable = gsi.curTurnSelected
                    val item = menu.findItem(R.id.board_menu_done)
                    item.setVisible(enable)
                    if (enable) {
                        val strId =
                            if (0 >= mView!!.curPending()) {
                                R.string.board_menu_pass
                            } else {
                                R.string.board_menu_done
                            }
                        item.setTitle(getString(strId))
                    }
                    if (mGameOver || summary.gameOver) {
                        mGameOver = true
                        val item = menu.findItem(R.id.board_menu_game_resign)
                        item.setTitle(getString(R.string.board_menu_game_final))
                    }
                }
                enable = summary.canRematch ?: false
                Utils.setItemVisible(menu, R.id.board_menu_rematch, enable)
                enable = mGameOver && !mGR!!.isArchived()
                Utils.setItemVisible(menu, R.id.board_menu_archive, enable)
                val netGame = (null != mGi
                                   && DeviceRole.ROLE_STANDALONE != mGi!!.deviceRole)
                enable = netGame && (BuildConfig.DEBUG
                                         || XWPrefs.getDebugEnabled(mActivity))
                Utils.setItemVisible(menu, R.id.board_menu_game_netstats, enable)
                enable = XWPrefs.getStudyEnabled(mActivity) && null != mGi
                    && !DBUtils.studyListWords(mActivity, mGi!!.isoCode()!!).isEmpty()
                Utils.setItemVisible(menu, R.id.board_menu_study, enable)
            }   // launch
        }
        return true
    } // onPrepareOptionsMenu

    private fun doGRStringThing(item: MenuItem) {
        val gr = mGR!!
        var id: Int = 0
        var str: String? = null
        launch {
            when (item.itemId) {
                R.id.board_menu_game_netstats -> {
                    id = R.string.netstats_title
                    str = gr.getStats()
                }
                R.id.board_menu_game_left -> {
                    id = R.string.tiles_left_title
                    str = gr.formatRemainingTiles()
                }
                R.id.board_menu_game_history -> {
                    id = R.string.history_title
                    str = gr.writeGameHistory(mGameOver)
                }
                R.id.board_menu_game_counts -> {
                    id = R.string.counts_values_title
                    str = gr.formatDictCounts()
                }
                else -> Assert.failDbg()
            }
            if (0 != id ) {
                showDialogFragment(DlgID.DLG_OKONLY, id, str!!)
            }
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        var handled = true
        val id = item.itemId
        when (id) {
            R.id.board_menu_done -> {
                launch {
                    val nTiles = mGR!!.getNumTilesInTray(mView!!.curPlayer)
                    if (mGi!!.traySize > nTiles) {
                        makeNotAgainBuilder(
                            R.string.key_notagain_done,
                            Action.COMMIT_ACTION, R.string.not_again_done
                        )
                            .show()
                    } else {
                        onPosButton(Action.COMMIT_ACTION)
                    }
                }
            }

            R.id.board_menu_rematch -> doRematchIf(false, false)
            R.id.board_menu_archive -> showArchiveNA(false)
            R.id.board_menu_trade_commit -> mGR!!.commitTurn()

            R.id.board_menu_trade_cancel -> mGR!!.endTrade()
//             R.id.board_menu_hint_prev -> cmd = JNICmd.CMD_PREV_HINT
//             R.id.board_menu_hint_next -> cmd = JNICmd.CMD_NEXT_HINT
//             R.id.board_menu_juggle -> cmd = JNICmd.CMD_JUGGLE
//             R.id.board_menu_flip -> cmd = JNICmd.CMD_FLIP
//             R.id.board_menu_chat -> startChatActivity()
            R.id.board_menu_trade -> {
                var msg = getString(R.string.not_again_trading)
                msg += getString(R.string.not_again_trading_menu)
                makeNotAgainBuilder(
                    R.string.key_notagain_trading,
                    Action.START_TRADE_ACTION, msg
                )
                    .show()
            }

            R.id.board_menu_tray -> mGR!!.toggleTray()
//             R.id.board_menu_study -> StudyListDelegate.launch(getDelegator(), mGi!!.isoCode())
            R.id.board_menu_study -> StudyListDelegate.launch(getDelegator(), mGi!!.isoCode())

            R.id.board_menu_game_netstats, R.id.board_menu_game_left,
            R.id.board_menu_game_history, R.id.board_menu_game_counts
                -> doGRStringThing(item)

//             R.id.board_menu_undo_current -> cmd = JNICmd.CMD_UNDO_CUR
//             R.id.board_menu_undo_last -> makeConfirmThenBuilder(
//                 Action.UNDO_LAST_ACTION,
//                 R.string.confirm_undo_last
//             )
//                 .show()

//             R.id.board_menu_game_pause, R.id.board_menu_game_unpause ->
//                 getConfirmPause(R.id.board_menu_game_pause == id)

//             R.id.board_menu_dict -> {
//                 val dictName = mGi!!.dictName(mView!!.curPlayer)
//                 if (null != dictName) DictBrowseDelegate.launch(getDelegator(), dictName)
//             }

            R.id.board_menu_game_resign -> {
                showDialogFragment(DlgID.QUERY_ENDGAME)
            }
            R.id.board_menu_file_prefs -> {
                mFiringPrefs = true
                PrefsDelegate.launch(mActivity)
            }

            else -> {
                Log.w(TAG, "menuitem %d not handled", id)
                handled = false
            }
        }
        return handled || super.onOptionsItemSelected(item)
    }

//     //////////////////////////////////////////////////
//     // DlgDelegate.DlgClickNotify interface
//     //////////////////////////////////////////////////
     override fun onPosButton(action: Action, vararg params: Any?): Boolean {
         Log.d(TAG, "onPosButton(%s, %s)", action, DbgUtils.fmtAny(arrayOf(params)))
         var handled = false
//         var cmd: JNICmd? = null
         val gi = mGi!!
         when (action) {
             Action.ENABLE_MQTT_DO_OR -> {
                if ( MQTTUtils.MQTTSupported() ) {
                    XWPrefs.setMQTTEnabled(mActivity, true)
                    MQTTUtils.setEnabled(mActivity, true)
                } else { // email will have been chosen
                    // val count = DBUtils.countOpenGamesUsingMQTT(mActivity)
                    // val msg = getString(R.string.have_mtqq_games_fmt, count)
                    // Utils.emailAuthor(mActivity, msg)
                }
            }

//             Action.UNDO_LAST_ACTION -> cmd = JNICmd.CMD_UNDO_LAST
//             Action.SMS_CONFIG_ACTION -> PrefsDelegate.launch(mActivity)
             Action.COMMIT_ACTION -> mGR!!.commitTurn()

//             Action.SHOW_EXPL_ACTION -> {
//                 showToast(m_mySIS!!.toastStr!!)
//                 m_mySIS!!.toastStr = null
//             }

            Action.BUTTON_BROWSEALL_ACTION, Action.BUTTON_BROWSE_ACTION -> {
                val curDict = gi.dictName(mView!!.curPlayer)
                val isoCode = gi.isoCode()!!
                val button: View? = mToolbar!!.getButtonFor(Buttons.BUTTON_BROWSE_DICT)
                if (Action.BUTTON_BROWSEALL_ACTION == action
                        && null != curDict
                        && null != button
                        && DictsDelegate.handleDictsPopup(getDelegator(), button, curDict, isoCode)
                ) {
                    // do nothing
                } else {
                    val selDict = DictsDelegate.prevSelFor(mActivity, isoCode) ?: curDict
                    selDict?.let{ DictBrowseDelegate.launch(getDelegator(), it) }
                }
            }

             Action.PREV_HINT_ACTION, Action.NEXT_HINT_ACTION -> {
                 launch {
                     val workRemains = BooleanArray(1)
                     while ( true ) {
                         val found = mGR!!.requestHint(action == Action.NEXT_HINT_ACTION, workRemains)
                         if ( !found || !workRemains[0] ) {
                             break;
                         }
                     }
                 }

             }

             Action.UNDO_ACTION -> mGR!!.replaceTiles()
             Action.JUGGLE_ACTION -> mGR!!.juggleTray()
             Action.FLIP_ACTION -> mGR!!.flip()

             Action.VALUES_ACTION ->
                 mToolbar!!.getButtonFor(Buttons.BUTTON_VALUES)?.let{doValuesPopup(it)}

             Action.CHAT_ACTION -> startChatActivity()

             Action.START_TRADE_ACTION -> {
                 showTradeToastOnce(true)
                 mGR!!.beginTrade()
             }

//             Action.LOOKUP_ACTION -> launchLookup(m_mySIS!!.words!!, gi.isoCode())
//             Action.DROP_MQTT_ACTION -> dropConViaAndRestart(CommsConnType.COMMS_CONN_MQTT)
             Action.DELETE_AND_EXIT -> deleteAndClose()
             Action.DROP_SMS_ACTION -> alertOrderIncrIfAt(StartAlertOrder.NBS_PERMS)
            Action.INVITE_SMS_DATA -> {
                val nMissing = params[0] as Int
                val info = params[1] as? SentInvitesInfo
                launchPhoneNumberInvite(
                    nMissing, info,
                    RequestCode.SMS_DATA_INVITE_RESULT
                )
            }

             Action.ASKED_PHONE_STATE -> showInviteChoicesThen()
             Action.BLANK_PICKED -> {
                 val tps = params[0] as TilePickState
                 val newTiles = params[1] as IntArray
                 mGR!!.setBlankValue(tps.playerNum, tps.col, tps.row, newTiles[0])
             }

             Action.TRAY_PICKED -> {
                 val tps = params[0] as TilePickState
                 val newTiles = params[1] as IntArray
                 launch {
                     if (tps.isInitial) {
                         mGR!!.tilesPicked(tps.playerNum, newTiles)
                         // handleViaThread(JNICmd.CMD_TILES_PICKED, tps.playerNum, newTiles)
                     } else {
                         // handleViaThread(JNICmd.CMD_COMMIT, true, true, newTiles)
                         mGR!!.commitTurn(true, 0, true, newTiles)
                     }
                 }
             }

//             Action.DISABLE_DUALPANE -> {
//                 XWPrefs.setPrefsString(
//                     mActivity, R.string.key_force_tablet,
//                     getString(R.string.force_tablet_phone)
//                 )
//                 makeOkOnlyBuilder(R.string.after_restart).show()
//             }

             Action.ARCHIVE_ACTION -> {
                 Log.d(TAG, "got ARCHIVE_ACTION")
                 val rematchAfter = params.size >= 1 && params[0] as Boolean
                 if (rematchAfter) {
                     doRematchIf(true, false) // closes game
                 } else {
                     archiveGame(true)
                 }
             }

            Action.REMATCH_ACTION -> {
                val archiveAfter = params.size >= 1 && params[0] as Boolean
                val deleteAfter = params.size >= 2 && params[1] as Boolean
                Assert.assertTrueNR(false == archiveAfter || false == deleteAfter)
                if (archiveAfter) {
                    showArchiveNA(true)
                } else {
                    doRematchIf(false, deleteAfter) // closes game
                }
            }

            Action.DELETE_ACTION ->
                if (0 < params.size && params[0] as Boolean) {
                    deleteAndClose()
                } else {
                    makeConfirmThenBuilder(
                        Action.DELETE_ACTION,
                        R.string.confirm_delete
                    )
                        .setParams(true)
                        .show()
                }

            Action.CUSTOM_DICT_CONFIRMED -> {
                mSummary!!.let { summary ->
                    val nli = params[0] as NetLaunchInfo
                    showInviteChoicesThen(
                        Action.LAUNCH_INVITE_ACTION, nli,
                        summary.nMissing, summary.nInvited
                    )
                }
            }

            Action.LAUNCH_INVITE_ACTION -> {
                for (obj in params) {
                    if (obj is CommsAddrRec) {
                        tryOtherInvites(obj)
                    } else {
                        break
                    }
                }
            }

//             Action.LAUNCH_THEME_CONFIG -> PrefsDelegate.launch(
//                 mActivity, PrefsWrappers.prefs_appear_themes::class.java
//             )

//             Action.LAUNCH_THEME_COLOR_CONFIG -> {
//                 val clazz: Class<*> =
//                     if (CommonPrefs.darkThemeInUse(mActivity)) {
//                         PrefsWrappers.prefs_appear_colors_dark::class.java
//                     } else {
//                         PrefsWrappers.prefs_appear_colors_light::class.java
//                     }
//                 PrefsDelegate.launch(mActivity, clazz)
//             }

//             Action.ENABLE_NBS_DO -> {
//                 post { retryNBSInvites(params) }
//                 handled = super.onPosButton(action, *params)
//             }

             else -> handled = super.onPosButton(action, *params)
         }
         //         cmd?.let { handleViaThread(it) }

         return handled
     }

     override fun onNegButton(action: Action, vararg params: Any?): Boolean {
         Log.d(TAG, "onNegButton(%s, %s)", action, DbgUtils.fmtAny(params))
         var handled = true
         when (action) {
//             Action.ENABLE_MQTT_DO_OR -> mDropMQTTOnDismiss = true
             Action.DROP_SMS_ACTION -> dropConViaAndRestart(CommsConnType.COMMS_CONN_SMS)
             Action.DELETE_AND_EXIT -> finish()
             Action.ASKED_PHONE_STATE -> showInviteChoicesThen()
             Action.CUSTOM_DICT_CONFIRMED -> {
                 Assert.failDbg()
                 // GamesListDelegate.launchGameConfig(mActivity, mGR!!)
                 finish()
             }

            Action.INVITE_SMS_DATA -> if (Perms23.haveNBSPerms(mActivity)) {
                val nMissing = params[0] as Int
                val info = params[1] as? SentInvitesInfo
                launchPhoneNumberInvite(
                    nMissing, info,
                    RequestCode.SMS_DATA_INVITE_RESULT
                )
            }

            else -> handled = super.onNegButton(action, *params)
        }
        return handled
    }

     override fun onDismissed(action: Action,
                              vararg params: Any?): Boolean
     {
         Log.d(TAG, "onDismissed(%s, %s)", action, DbgUtils.fmtAny(params))
         var handled = true
         when (action) {
            Action.ENABLE_MQTT_DO_OR ->
                if (mDropMQTTOnDismiss) {
                    postDelayed({ askDropMQTT() }, 10)
                } else {
                    alertOrderIncrIfAt(StartAlertOrder.NO_MEANS)
                }

            Action.DELETE_AND_EXIT -> finish()

            Action.BLANK_PICKED, Action.TRAY_PICKED ->
                // If the user cancels the tile picker the common code doesn't
                // know, and won't put it up again as long as this game
                // remains loaded. There might be a way to fix that, but the
                // safest thing to do for now is to close. User will have to
                // begin the process of committing turn again on re-launching
                // the game.
                finish()

//             Action.DROP_SMS_ACTION ->
//                 alertOrderIncrIfAt(StartAlertOrder.NBS_PERMS)
            Action.LAUNCH_INVITE_ACTION -> showInviteAlertIf()
            else -> handled = super.onDismissed(action, *params)
        }
        return handled
    } // onDismissed

    override fun inviteChoiceMade(
        action: Action, means: InviteMeans,
        vararg params: Any?
    ) {
        Log.d(TAG, "inviteChoiceMade($action, $means)")
        if (action == Action.LAUNCH_INVITE_ACTION) {
            val info =
                if (0 < params.size && params[0] is SentInvitesInfo) params[0] as SentInvitesInfo
                else null
            when (means) {
                InviteMeans.NFC -> if (!nfcAvail(mActivity)[1]) {
                    showDialogFragment(DlgID.ENABLE_NFC)
                } else {
                    makeOkOnlyBuilder(R.string.nfc_just_tap).show()
                }

                InviteMeans.BLUETOOTH ->
                    BTInviteDelegate.launchForResult(
                        mActivity, mSummary!!.nMissing, info,
                        RequestCode.BT_INVITE_RESULT
                    )

                InviteMeans.SMS_DATA ->
                    Perms23.tryGetPerms(
                        this, Perms23.NBS_PERMS, R.string.sms_invite_rationale,
                        Action.INVITE_SMS_DATA, mSummary!!.nMissing, info
                    )

                InviteMeans.MQTT -> showDialogFragment(DlgID.GET_DEVID)

//                 InviteMeans.WIFIDIRECT -> WiDirInviteDelegate.launchForResult(
//                     mActivity,
//                     m_mySIS!!.nMissing, info,
//                     RequestCode.P2P_INVITE_RESULT
//                 )

                InviteMeans.SMS_USER, InviteMeans.EMAIL, InviteMeans.CLIPBOARD -> {
                    val nli = NetLaunchInfo(
                        mActivity, mSummary!!, mGi!!,
                        1  // nPlayers
                    ) // fc
                    when (means) {
                        InviteMeans.EMAIL -> GameUtils.launchEmailInviteActivity(mActivity, nli)
                        InviteMeans.SMS_USER -> GameUtils.launchSMSInviteActivity(mActivity, nli)
                        InviteMeans.CLIPBOARD -> GameUtils.inviteURLToClip(mActivity, nli)
                        else -> Log.d(TAG, "unexpected means $means")
                    }
                    // recordInviteSent(means, null)
                }

                InviteMeans.QRCODE -> {}
                else -> Assert.failDbg()
            }
        }
    }

//     //////////////////////////////////////////////////
//     // View.OnClickListener interface
//     //////////////////////////////////////////////////
//     override fun onClick(view: View) {
//         if (view === mExchCommmitButton) {
//             handleViaThread(JNICmd.CMD_COMMIT)
//         } else if (view === mExchCancelButton) {
//             handleViaThread(JNICmd.CMD_CANCELTRADE)
//         }
//     }

//     //////////////////////////////////////////////////
//     // MultiService.MultiEventListener interface
//     //////////////////////////////////////////////////
//     override fun eventOccurred(event: MultiEvent, vararg args: Any?) {
//         when (event) {
//             MultiEvent.MESSAGE_ACCEPTED, MultiEvent.MESSAGE_REFUSED -> ConnStatusHandler.updateStatusIn(
//                 mActivity, this, CommsConnType.COMMS_CONN_BT,
//                 MultiEvent.MESSAGE_ACCEPTED == event
//             )

//             MultiEvent.MESSAGE_NOGAME -> {
//                 val gameID = args[0] as Int
//                 if (null != mGi && gameID == mGi!!.gameID && !isFinishing()) {
//                     var expl: ConnExpl? = null
//                     if (1 < args.size && args[1] is ConnExpl) {
//                         expl = args[1] as ConnExpl
//                     }
//                     postDeleteOnce(expl)
//                 }
//             }

//             MultiEvent.BT_ENABLED -> pingBTRemotes()
//             MultiEvent.NEWGAME_FAILURE -> Log.w(TAG, "failed to create game")
//             MultiEvent.NEWGAME_DUP_REJECTED -> post {
//                 makeOkOnlyBuilder(
//                     R.string.err_dup_invite_fmt,
//                     args[0] as String
//                 )
//                     .show()
//             }

//             MultiEvent.SMS_SEND_OK -> ConnStatusHandler.showSuccessOut(this)
//             MultiEvent.SMS_RECEIVE_OK -> ConnStatusHandler.showSuccessIn(this)
//             MultiEvent.SMS_SEND_FAILED, MultiEvent.SMS_SEND_FAILED_NORADIO, MultiEvent.SMS_SEND_FAILED_NOPERMISSION ->             // Don't bother warning if they're banned. Too frequent
//                 if (Perms23.haveNBSPerms(mActivity)) {
//                     DbgUtils.showf(mActivity, R.string.sms_send_failed)
//                 }

//             else -> super.eventOccurred(event, *args)
//         }
//     }

//     //////////////////////////////////////////////////
//     // TransportProcs.TPMsgHandler interface
//     //////////////////////////////////////////////////
//     override fun tpmCountChanged(newCount: Int, quashed: Boolean) {
//         ConnStatusHandler.updateMoveCount(mActivity, newCount, quashed)
//         if (quashed) {
//             postDeleteOnce(null)
//         }
//         val goAlert = mGameOverAlert
//         if (null != goAlert) {
//             runOnUiThread { goAlert.pendingCountChanged(newCount) }
//         }
//     }

//     //////////////////////////////////////////////////
//     // DwnldActivity.DownloadFinishedListener interface
//     //////////////////////////////////////////////////
//     override fun downloadFinished(
//         isoCode: ISOCode, name: String,
//         success: Boolean
//     ) {
//         if (success) {
//             post { setGotGameDict(name) }
//         }
//     }

    //////////////////////////////////////////////////
    // ConnStatusHandler.ConnStatusCBacks
    //////////////////////////////////////////////////
    override fun invalidateParent() {
        runOnUiThread { mView!!.invalidate() }
    }

    override fun onStatusClicked() {
        if (BuildConfig.NON_RELEASE || XWPrefs.getDebugEnabled(mActivity)) {
            val view = findViewById(R.id.netstatus_view)
            val popup = PopupMenu(mActivity, view)
            popup.menuInflater.inflate(R.menu.netstat, popup.menu)
            if (!mGi!!.conTypes!!.contains(CommsConnType.COMMS_CONN_MQTT)) {
                popup.menu.removeItem(R.id.netstat_menu_traffic)
                popup.menu.removeItem(R.id.netstat_peers)
                popup.menu.removeItem(R.id.netstat_copyurl)
            }
            if (!mSummary!!.quashed) {
                popup.menu.removeItem(R.id.netstat_unquash)
            }
            popup.setOnMenuItemClickListener { item ->
                var handled = true
                when (item.itemId) {
                    R.id.netstat_menu_status -> onStatusClicked(mGR!!)
                    R.id.netstat_menu_traffic -> NetUtils.copyAndLaunchGamePage(
                        mActivity,
                        mGi!!.gameID
                    )

                    R.id.netstat_copyurl -> NetUtils.gameURLToClip(mActivity, mGi!!.gameID)
                    R.id.netstat_peers -> showDialogFragment(DlgID.MQTT_PEERS)
                    R.id.netstat_unquash -> {
                        launch {
                            mGR!!.setQuashed(false)
                        }
                    }
                    else -> handled = false
                }
                handled
            }
            popup.show()
        } else {
            onStatusClicked(mGR!!)
        }
    }

    override fun getHandler(): Handler? { return mHandler }

//     ////////////////////////////////////////////////////////////
//     // NFCCardService.Wrapper.Procs
//     ////////////////////////////////////////////////////////////
     override fun onReadingChange(nowReading: Boolean) {
         // Do we need this?
     }

     private fun updatePostDraw() {
         launch {
             mGsi = mGR!!.getState()
             val gsi = mGsi!!
             updateToolbar(gsi)
             val inTrade = gsi.inTrade
             if (m_mySIS!!.inTrade != inTrade) {
                 m_mySIS!!.inTrade = inTrade
             }
             mView!!.setInTrade(inTrade)
             showTradeToastOnce(inTrade)
             adjustTradeVisibility()
             invalidateOptionsMenuIf()
         }
     }

    ////////////////////////////////////////////////////////////
    // InvitesNeededAlert.Callbacks
    ////////////////////////////////////////////////////////////
    override fun getDelegate(): DelegateBase {
        return this
    }

    override fun onCloseClicked() {
        post(object : Runnable {
            override fun run() {
                iNAWrapper.dismiss()
                finish()
            }
        })
    }

    override fun onInviteClicked() {
        iNAWrapper.dismiss()
        callInviteChoices()
    }

    override fun getGameRef(): GameRef { return mGR!! }

    private fun getInvite(): ByteArray?
    {
        val gi = mGi!!
        val result =
            if (0 < mSummary!!.nMissing // Isn't there a better test??
                    && DeviceRole.ROLE_ISHOST == gi.deviceRole
            ) {
                NetLaunchInfo(gi).let { nli ->
                    Assert.assertTrue(0 <= mSummary!!.nGuestDevs)
                    nli.forceChannel = 1 + mSummary!!.nGuestDevs

                    mGi!!.conTypes!!.map{ typ ->
                        when (typ) {
                            CommsConnType.COMMS_CONN_RELAY -> Log.e(TAG, "Relay not supported")
                            CommsConnType.COMMS_CONN_BT -> nli.addBTInfo(mActivity)
                            CommsConnType.COMMS_CONN_SMS -> nli.addSMSInfo(mActivity)
                            CommsConnType.COMMS_CONN_P2P -> nli.addP2PInfo(mActivity)
                            CommsConnType.COMMS_CONN_NFC -> nli.addNFCInfo()
                            CommsConnType.COMMS_CONN_MQTT -> nli.addMQTTInfo()
                            else -> Log.w(TAG, "Not doing NFC join for conn type $typ")
                        }
                    }
                    nli.asByteArray()
                }
            } else null
        return result
    }

    private fun launchPhoneNumberInvite(
        nMissing: Int, info: SentInvitesInfo?,
        code: RequestCode
    ) {
        SMSInviteDelegate.launchForResult(mActivity, nMissing, info, code)
    }

     private fun deleteAndClose() {
         GameMgr.deleteGame(mGR!!)
         waitCloseGame(false)
         finish()
     }

     private fun askDropMQTT() {
         mGi?.conTypes?.let { connTypes ->
             var msg = getString(R.string.confirm_drop_mqtt)
             if (connTypes.contains(CommsConnType.COMMS_CONN_BT)) {
                 msg += " " + getString(R.string.confirm_drop_relay_bt)
             }
             if (connTypes.contains(CommsConnType.COMMS_CONN_SMS)) {
                 msg += " " + getString(R.string.confirm_drop_relay_sms)
             }
             makeConfirmThenBuilder(Action.DROP_MQTT_ACTION, msg).show()
         }
     }

     private fun dropConViaAndRestart(typ: CommsConnType) {
         mGR?.let {
             launch {
                 it.dropHostAddr(typ)
                 finish()
                 GameUtils.launchGame(getDelegator(), it)
             }
         }
     }

//     private fun setGotGameDict(getDict: String?) {
//         mJniThread!!.setSaveDict(getDict)
//         val msg = getString(R.string.reload_new_dict_fmt, getDict)
//         showToast(msg)
//         finish()
//         GameUtils.launchGame(getDelegator(), mRowid)
//     }

//     private fun keyCodeToXPKey(keyCode: Int): XP_Key {
//         var xpKey = XP_Key.XP_KEY_NONE
//         when (keyCode) {
//             KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> xpKey = XP_Key.XP_RETURN_KEY
//             KeyEvent.KEYCODE_DPAD_DOWN -> xpKey = XP_Key.XP_CURSOR_KEY_DOWN
//             KeyEvent.KEYCODE_DPAD_LEFT -> xpKey = XP_Key.XP_CURSOR_KEY_LEFT
//             KeyEvent.KEYCODE_DPAD_RIGHT -> xpKey = XP_Key.XP_CURSOR_KEY_RIGHT
//             KeyEvent.KEYCODE_DPAD_UP -> xpKey = XP_Key.XP_CURSOR_KEY_UP
//             KeyEvent.KEYCODE_SPACE -> xpKey = XP_Key.XP_RAISEFOCUS_KEY
//         }
//         return xpKey
//     }

     fun informMove(turn: Int, expl: String, words: String?) {
         m_mySIS!!.words = words?.let { wordsToArray(it) }
         nonBlockingDialog(DlgID.DLG_SCORES, expl)

         // Post a notification if in background, or play sound if not. But
         // do nothing for standalone case.
         if (DeviceRole.ROLE_STANDALONE == mGi!!.deviceRole) {
             // do nothing
         } else if (isVisible) {
             Utils.playNotificationSound(mActivity)
         } else {
             Assert.failDbg()
         }
     }

     fun notifyGameOver() {
         mGameOver = true
         launch(Dispatchers.IO) {
             val titleID = R.string.summary_gameover
             val text = mGR!!.writeFinalScores()
             handleGameOver(titleID, text)
         }
     }

     private inner class BoardUtilCtxt(gr: GameRef) : UtilCtxt(gr) {

         override fun remSelected() {
             launch {
                 val str = mGR!!.formatRemainingTiles()
                 showDialogFragment(DlgID.DLG_OKONLY, R.string.tiles_left_title, str)
             }
         }

//         override fun timerSelected(inDuplicateMode: Boolean, canPause: Boolean) {
//             if (inDuplicateMode) {
//                 runOnUiThread { getConfirmPause(canPause) }
//             }
//         }

        override fun bonusSquareHeld(bonus: Int) {
            val id =
                when (bonus) {
                    UtilCtxt.BONUS_DOUBLE_LETTER -> R.string.bonus_l2x
                    UtilCtxt.BONUS_DOUBLE_WORD -> R.string.bonus_w2x
                    UtilCtxt.BONUS_TRIPLE_LETTER -> R.string.bonus_l3x
                    UtilCtxt.BONUS_TRIPLE_WORD -> R.string.bonus_w3x
                    UtilCtxt.BONUS_QUAD_LETTER -> R.string.bonus_l4x
                    UtilCtxt.BONUS_QUAD_WORD -> R.string.bonus_w4x
                    else -> {Assert.failDbg(); 0}
                }
            post { showToast(id) }
        }

        override fun informWordsBlocked(nWords: Int, words: String, dict: String) {
            runOnUiThread {
                val fmtd = TextUtils.join(", ", wordsToArray(words))
                makeOkOnlyBuilder(R.string.word_blocked_by_phony, fmtd, dict)
                    .show()
            }
        }

        override fun playerScoreHeld(player: Int) {
            launch {
                val lmi = mGR!!.getPlayersLastScore(player)
                var expl = lmi.format(mActivity)
                if (null == expl || 0 == expl.length) {
                    expl = getString(R.string.no_moves_made)
                }
                makeOkOnlyBuilder(expl).show()
            }
        }

        override fun cellSquareHeld(words: String) {
            post { launchLookup(wordsToArray(words), mGi!!.isoCode()) }
        }

        private fun startTP(
            action: Action,
            tps: TilePickState
        ) {
            runOnUiThread { show(TilePickAlert.newInstance(action, tps)) }
        }

        override fun notifyPickTileBlank(
            playerNum: Int, col: Int, row: Int,
            texts: Array<String>
        ) {
            val tps = TilePickState(playerNum, texts, col, row)
            startTP(Action.BLANK_PICKED, tps)
        }

        override fun informNeedPickTiles(
            isInitial: Boolean, playerNum: Int,
            nToPick: Int, texts: Array<String>,
            counts: IntArray
        ) {
            val tps = TilePickState(
                isInitial, playerNum, nToPick,
                texts, counts
            )
            startTP(Action.TRAY_PICKED, tps)
        }

//         override fun informNeedPassword(player: Int, name: String?) {
//             showDialogFragment(DlgID.ASK_PASSWORD, player, name)
//         }

//         override fun turnChanged(newTurn: Int) {
//             if (0 <= newTurn) {
//                 m_mySIS!!.nMissing = 0
//                 post {
//                     makeNotAgainBuilder(
//                         R.string.key_notagain_turnchanged,
//                         R.string.not_again_turnchanged
//                     )
//                         .show()
//                 }
//                 handleViaThread(JNICmd.CMD_ZOOM, -8)
//                 handleViaThread(JNICmd.CMD_SAVE)
//             }
//         }

        override fun notifyMove(msg: String) {
            showDialogFragment(DlgID.QUERY_MOVE, msg)
        }

        override fun notifyTrade(tiles: Array<String>) {
            val dlgBytes = getQuantityString(
                R.plurals.query_trade_fmt, tiles.size,
                tiles.size, TextUtils.join(", ", tiles),
            )
            showDialogFragment(DlgID.QUERY_TRADE, dlgBytes)
        }

//         override fun notifyDupStatus(amHost: Boolean, msg: String) {
//             val key =
//                 if (amHost) R.string.key_na_dupstatus_host else R.string.key_na_dupstatus_guest
//             runOnUiThread {
//                 makeNotAgainBuilder(key, msg)
//                     .show()
//             }
//         }

        private fun showUserError(msg: String?, asToast: Boolean = false) {
            msg?.let {
                if (asToast) {
                    runOnUiThread { showToast(it) }
                } else {
                    nonBlockingDialog(DlgID.DLG_OKONLY, it)
                }
            }
        }

        override fun userError(code: Int) {
            var asToast = false
            val resid = when (code) {
                UtilCtxt.ERR_TILES_NOT_IN_LINE -> R.string.str_tiles_not_in_line
                UtilCtxt.ERR_NO_EMPTIES_IN_TURN -> R.string.str_no_empties_in_turn
                UtilCtxt.ERR_TWO_TILES_FIRST_MOVE -> R.string.str_two_tiles_first_move
                UtilCtxt.ERR_TILES_MUST_CONTACT -> R.string.str_tiles_must_contact
                UtilCtxt.ERR_NOT_YOUR_TURN -> R.string.str_not_your_turn
                UtilCtxt.ERR_NO_PEEK_ROBOT_TILES -> R.string.str_no_peek_robot_tiles
                UtilCtxt.ERR_NO_EMPTY_TRADE -> {                // This should not be possible as the button's
                    // disabled when no tiles selected.
                    Assert.failDbg()
                    0
                }

                UtilCtxt.ERR_TOO_MANY_TRADE -> {
                    launch(Dispatchers.IO) {
                        val nLeft = mGR!!.countTilesInPool()
                        withContext(Dispatchers.Main) {
                            val msg = getQuantityString(
                                R.plurals.too_many_trade_fmt,
                                nLeft, nLeft)
                            showUserError(msg)
                        }
                    }
                    0           // don't call showUserError() below
                }

                UtilCtxt.ERR_TOO_FEW_TILES_LEFT_TO_TRADE ->
                    R.string.str_too_few_tiles_left_to_trade

                UtilCtxt.ERR_CANT_UNDO_TILEASSIGN -> R.string.str_cant_undo_tileassign
                UtilCtxt.ERR_CANT_HINT_WHILE_DISABLED ->
                    R.string.str_cant_hint_while_disabled

                UtilCtxt.ERR_NO_PEEK_REMOTE_TILES -> R.string.str_no_peek_remote_tiles
                UtilCtxt.ERR_REG_UNEXPECTED_USER -> R.string.str_reg_unexpected_user
                UtilCtxt.ERR_SERVER_DICT_WINS -> R.string.str_server_dict_wins
                UtilCtxt.ERR_REG_SERVER_SANS_REMOTE -> R.string.str_reg_server_sans_remote
                UtilCtxt.ERR_NO_HINT_FOUND -> {
                    asToast = true
                    R.string.str_no_hint_found
                }
                else -> {
                    Log.d(TAG, "userError(): unexpected code $code")
                    0
                }
            }
            if (resid != 0) {
                val msg = getString(resid)
                showUserError(msg, asToast)
            }
        } // userError

        override fun countChanged( count: Int, quashed: Boolean ) {
            Log.d(TAG, "countChanged($count)")
            runOnUiThread {
                ConnStatusHandler.updateMoveCount(mActivity, count, quashed)
                if (quashed) {
                    postDeleteOnce()
                }
                mGameOverAlert?.let {
                    it.pendingCountChanged(count)
                }
            }
        }
        
//         override fun informUndo() {
//             nonBlockingDialog(
//                 DlgID.DLG_OKONLY,
//                 getString(R.string.remote_undone)
//             )
//         }

//         override fun informNetDict(
//             isoCodeStr: String, oldName: String,
//             newName: String, newSum: String,
//             phonies: XWPhoniesChoice
//         ) {
//             // If it's same dict and same sum, we're good.  That
//             // should be the normal case.  Otherwise: if same name but
//             // different sum, notify and offer to upgrade.  If
//             // different name, offer to install.
//             var msg: String? = null
//             if (oldName == newName) {
//                 val oldSum = DictLangCache
//                     .getDictMD5Sums(mActivity, oldName)[0]
//                 if (oldSum != newSum) {
//                     // Same dict, different versions
//                     msg = getString(
//                         R.string.inform_dict_diffversion_fmt,
//                         oldName
//                     )
//                 }
//             } else {
//                 // Different dict!  If we have the other one, switch
//                 // to it.  Otherwise offer to download
//                 val dlgID: DlgID
//                 msg = getString(
//                     R.string.inform_dict_diffdict_fmt,
//                     oldName, newName, newName
//                 )
//                 val isoCode = ISOCode.newIf(isoCodeStr)
//                 if (DictLangCache.haveDict(
//                         mActivity, isoCode,
//                         newName!!
//                     )
//                 ) {
//                     dlgID = DlgID.DLG_USEDICT
//                 } else {
//                     dlgID = DlgID.DLG_GETDICT
//                     msg += getString(R.string.inform_dict_download)
//                 }
//                 m_mySIS!!.getDict = newName
//                 nonBlockingDialog(dlgID, msg)
//             }
//         }

        override fun notifyIllegalWords(
            dict: String, words: Array<String>, turn: Int,
            turnLost: Boolean, badWordsKey: Int
        ) {
            val wordsString = TextUtils.join(", ", words!!)
            val message = getString(R.string.ids_badwords_fmt, wordsString, dict)
            if (turnLost) {
                showDialogFragment(
                    DlgID.DLG_BADWORDS, R.string.badwords_title,
                    message + getString(R.string.badwords_lost)
                )
            } else {
                val msg = message + getString(R.string.badwords_accept)
                showDialogFragment(
                    DlgID.ASK_BADWORDS, msg, words.size,
                    badWordsKey
                )
            }
        }

//         // Let's have this block in case there are multiple messages.  If
//         // we don't block the jni thread will continue processing messages
//         // and may stack dialogs on top of this one.  Including later
//         // chat-messages.
        override fun showChat(msg: String, fromIndx: Int,
                              tsSeconds: Int): Boolean {
            runOnUiThread {
                if (!ChatDelegate.reload()) {
                    startChatActivity()
                }
            }
            return true
        }

//         override fun formatPauseHistory(
//             pauseTyp: Int, player: Int,
//             whenPrev: Int, whenCur: Int, msg: String?
//         ): String? {
//             Log.d(TAG, "formatPauseHistory(prev: %d, cur: %d)", whenPrev, whenCur)
//             var result: String? = null
//             val name = if (0 > player) null else mGi!!.players[player]!!.name
//             when (pauseTyp) {
//                 DUtilCtxt.UNPAUSED -> {
//                     val interval = DateUtils
//                         .formatElapsedTime((whenCur - whenPrev).toLong())
//                         .toString()
//                     result = LocUtils.getString(
//                         mActivity, R.string.history_unpause_fmt,
//                         name, interval
//                     )
//                 }

//                 DUtilCtxt.PAUSED -> result = LocUtils.getString(
//                     mActivity, R.string.history_pause_fmt,
//                     name
//                 )

//                 DUtilCtxt.AUTOPAUSED -> result =
//                     LocUtils.getString(mActivity, R.string.history_autopause)
//             }
//             msg?.let {
//                 result += " " + LocUtils
//                     .getString(mActivity, R.string.history_msg_fmt, it)
//             }
//             return result
//         }
        override fun dictGone(dictName: String) {
            Log.d(TAG, "dictGone($dictName)")
            runOnUiThread {
                waitCloseGame(false)
                finish()
            }
        }
        
    } // class BoardUtilCtxt

     private fun doResume(isStart: Boolean) {
         Log.d(TAG, "doResume($isStart) (mGR: $mGR)")
         var success = null != mGR
         val firstStart = null == mHandler
         if (success && firstStart) {
             mHandler = Handler(Looper.getMainLooper())
             //             success = false
             //             // success = mJniThreadRef!!.configure(
             //             //     mActivity, mUtils, this,
             //             //     makeJNIHandler()
             //             // )
             //             // if (success) {
             //             //     mJniGamePtr = mJniThreadRef!!.getGamePtr()
             //             //     Assert.assertNotNull(mJniGamePtr)
             //             // }
             //         }
             try {
                 resumeGame(isStart)
                 if (!isStart) {
                     setKeepScreenOn()
                 }
             } catch (ex: NoSuchGameException) {
                 Log.ex(TAG, ex)
                 success = false
             } catch (ex: NullPointerException) {
                 Log.ex(TAG, ex)
                 success = false
             }
         }
         if (success) {
             ConnStatusHandler.setHandler(this)
         } else {
             finish()
         }
     }

     private var mTradeToastShown = false
     private fun showTradeToastOnce(inTrade: Boolean) {
         if (inTrade) {
             if (!mTradeToastShown) {
                 mTradeToastShown = true
                 Utils.showToast(mActivity, R.string.entering_trade)
             }
         } else {
             mTradeToastShown = false
         }
     }

//     private fun makeJNIHandler(): Handler {
//         return object : Handler(Looper.getMainLooper()) {
//             override fun handleMessage(msg: Message) {
//                 when (msg.what) {
//                     JNIThread.DIALOG -> showDialogFragment(
//                         DlgID.DLG_OKONLY, msg.arg1,
//                         msg.obj as String
//                     )

//                     JNIThread.QUERY_ENDGAME -> showDialogFragment(DlgID.QUERY_ENDGAME)
//                     JNIThread.TOOLBAR_STATES ->
//                         mJniThread?.let {
//                             mGsi = it.getGameStateInfo()
//                             updateToolbar()
//                             val inTrade = mGsi!!.inTrade
//                             if (m_mySIS!!.inTrade != inTrade) {
//                                 m_mySIS!!.inTrade = inTrade
//                             }
//                             mView!!.setInTrade(inTrade)
//                             showTradeToastOnce(inTrade)
//                             adjustTradeVisibility()
//                             invalidateOptionsMenuIf()
//                         }

//                     JNIThread.GAME_OVER -> if (mIsFirstLaunch) {
//                         handleGameOver(msg.arg1, msg.obj as String)
//                     }

//                     JNIThread.MSGS_SENT -> {
//                         val nSent = msg.obj as Int
//                         showToast(
//                             getQuantityString(
//                                 R.plurals.resent_msgs_fmt,
//                                 nSent, nSent
//                             )
//                         )
//                     }

//                     JNIThread.GOT_PAUSE -> runOnUiThread {
//                         makeOkOnlyBuilder(msg.obj as String)
//                             .show()
//                     }

//                     JNIThread.DO_DRAW -> {
//                         mView?.doJNIDraw();
//                     }

//                     JNIThread.DIMMS_CHANGED -> {
//                         mView?.dimsChanged(msg.obj as BoardDims)
//                     }
//                 }
//             }
//         }
//     }

     private fun handleGameOver(titleID: Int, msg: String, nPending: Int = 0) {
         val onDone: OnDoneProc = object : OnDoneProc {
             override fun onGameOverDone(
                 rematch: Boolean,
                 archiveAfter: Boolean,
                 deleteAfter: Boolean
             ) {
                 var postAction: Action? = null
                 val postArgs = ArrayList<Any>()
                 if (rematch) {
                     postAction = Action.REMATCH_ACTION
                     postArgs.add(archiveAfter)
                     postArgs.add(deleteAfter)
                 } else if (archiveAfter) {
                     showArchiveNA(false)
                 } else if (deleteAfter) {
                     postAction = Action.DELETE_ACTION
                 }
                 postAction?.let {
                     post { onPosButton(it, *postArgs.toTypedArray()) }
                 }
             }
         }
         launch {
             mGameOverAlert = GameOverAlert.newInstance(
                 mSummary, titleID, msg,
                 0 < nPending, mGR!!.isArchived()
             )
                 .configure(onDone, this@BoardDelegate)
             show(mGameOverAlert)
         }
     }

    private fun resumeGame(isStart: Boolean) {
        Log.d(TAG, "resumeGame($isStart)")
        launch {
            val gr = mGR!!
            mGi = gr.getGI()!!
            val gi = mGi!!
            mSummary = gr.getSummary()
            Wrapper.setGameID(mNFCWrapper, gi.gameID)
            getInvite()?.let {
                NFCUtils.addInvitationFor(it, gi.gameID)
            }
            var proc: NewRecentsProc? = null
            if (!Utils.onFirstVersion(mActivity)) {
                proc = object : NewRecentsProc {
                    override fun sawNew() {
                        runOnUiThread {
                            if (!mSawNewShown) {
                                mSawNewShown = true
                                val p1 = LocUtils.getString(
                                    mActivity,
                                    R.string.menu_prefs
                                )
                                val p2 = LocUtils.getString(
                                    mActivity,
                                    R.string.tile_back_recent
                                )
                                val msg = LocUtils.getString(
                                    mActivity,
                                    R.string.not_again_newsawnew_fmt,
                                    p1, p2
                                )
                                makeNotAgainBuilder(R.string.key_na_newsawnew, msg)
                                    .setTitle(R.string.new_feature_title)
                                    .setActionPair(
                                        Action.LAUNCH_THEME_COLOR_CONFIG,
                                        R.string.menu_prefs
                                    )
                                    .show()
                            }
                        }
                    }
                }
            }

            Log.d(TAG, "calling startHandling()")
            mView!!.startHandling(mActivity, gr, gi, proc,
                                  object : DrawDoneProc {
                                      override fun drawDone() {
                                          updatePostDraw()
                                      }
                                  })
            gr.start()
            if (!CommonPrefs.getHideTitleBar(mActivity)) {
                setTitle()
            }
            initToolbar()
            adjustTradeVisibility()
            // val flags = 0 // DBUtils.getMsgFlags(mActivity, mRowid)
            // if (0 != GameSummary.MSG_FLAGS_CHAT and flags) {
            //     // post { startChatActivity() }
            // }
            if (mOverNotShown) {
                var auto = false
                if (mGR!!.getGameIsOver()) {
                    mGameOver = true
                    auto = true
                }
                if (mGameOver) {
                    mOverNotShown = false
                    val titleID =
                        if (auto) R.string.summary_gameover
                        else R.string.finalscores_title

                    launch {
                        mGR!!.let { gr ->
                            val msg = gr.writeFinalScores()
                            val nPending = gr.countPendingPackets()
                            handleGameOver(titleID, msg, nPending)
                        }
                    }
                }
            }
            //             if (0 != flags) {
            //                 DBUtils.setMsgFlags(
            //                     mActivity, mRowid,
            //                     GameSummary.MSG_FLAGS_NONE
            //                 )
            //             }
            //             Utils.cancelNotification(mActivity, mRowid)
            if (mGi!!.deviceRole != DeviceRole.ROLE_STANDALONE) {
                askNBSPermissions()
                warnIfNoTransport()
                tryInvites()
            }
            //             val args = arguments!!
            //             if (args.getBoolean(PAUSER_KEY, false)) {
            //                 getConfirmPause(true)
            //             }
            setTitle(gi)
        }
    } // resumeGame

    private fun askNBSPermissions() {
        Log.d(TAG, "askNBSPermissions()")
        val thisOrder = StartAlertOrder.NBS_PERMS
        if (alertOrderAt(thisOrder) // already asked?
                && mGi!!.conTypes!!.contains(CommsConnType.COMMS_CONN_SMS)
        ) {
            //             if (Perms23.haveNBSPerms(mActivity)) {
            //                 // We have them or a workaround; cool! proceed
            //                 alertOrderIncrIfAt(thisOrder)
            //             } else {
            //                 mPermCbck = object:PermCbck {
            //                     override fun onPermissionResult(allGood: Boolean) {
            //                         if (allGood) {
            //                             // Yay! nothing to do
            //                             alertOrderIncrIfAt(thisOrder)
            //                         } else {
            //                             val explID =
            //                                 if (Perms23.NBSPermsInManifest(mActivity)) R.string.missing_sms_perms else R.string.variant_missing_nbs
            //                             makeConfirmThenBuilder(Action.DROP_SMS_ACTION, explID)
            //                                 .setNegButton(R.string.remove_sms)
            //                                 .show()
            //                         }
            //                     }
            //                 }
            //                 Perms23.Builder(*Perms23.NBS_PERMS)
            //                     .asyncQuery(mActivity, mPermCbck)
            //             }
        } else {
            alertOrderIncrIfAt(thisOrder)
        }
    }

//     private fun pingBTRemotes() {
//         if (null != mConnTypes && mConnTypes!!.contains(CommsConnType.COMMS_CONN_BT)
//             && !XWPrefs.getBTDisabled(mActivity)
//             && XwJNI.server_getGameIsConnected(mJniGamePtr)
//         ) {
//             val addrs = XwJNI.comms_getAddrs(mJniGamePtr)
//             for (addr in addrs!!) {
//                 if (addr!!.contains(CommsConnType.COMMS_CONN_BT)
//                     && !TextUtils.isEmpty(addr.bt_btAddr)
//                 ) {
//                     BTUtils.pingHost(mActivity, addr.bt_hostName!!, addr.bt_btAddr, mGi!!.gameID)
//                 }
//             }
//         }
//     }

    private fun populateToolbar() {
        mToolbar!!
            .setListener(
                Buttons.BUTTON_BROWSE_DICT,
                R.string.not_again_browseall,
                R.string.key_na_browseall,
                Action.BUTTON_BROWSEALL_ACTION
            )
            .setLongClickListener(
                Buttons.BUTTON_BROWSE_DICT,
                R.string.not_again_browse,
                R.string.key_na_browse,
                Action.BUTTON_BROWSE_ACTION
            )
            .setListener(
                Buttons.BUTTON_HINT_PREV,
                R.string.not_again_hintprev,
                R.string.key_notagain_hintprev,
                Action.PREV_HINT_ACTION
            )
            .setListener(
                Buttons.BUTTON_HINT_NEXT,
                R.string.not_again_hintnext,
                R.string.key_notagain_hintnext,
                Action.NEXT_HINT_ACTION
            )
            .setListener(
                Buttons.BUTTON_JUGGLE,
                R.string.not_again_juggle,
                R.string.key_notagain_juggle,
                Action.JUGGLE_ACTION
            )
            .setListener(
                Buttons.BUTTON_FLIP,
                R.string.not_again_flip,
                R.string.key_notagain_flip,
                Action.FLIP_ACTION
            )
            .setListener(
                Buttons.BUTTON_VALUES,
                R.string.not_again_values,
                R.string.key_na_values,
                Action.VALUES_ACTION
            )
            .setListener(
                Buttons.BUTTON_UNDO,
                R.string.not_again_undo,
                R.string.key_notagain_undo,
                Action.UNDO_ACTION
            )
            .setListener(
                Buttons.BUTTON_CHAT,
                R.string.not_again_chat,
                R.string.key_notagain_chat,
                Action.CHAT_ACTION
            )
        Log.d(TAG, "populateToolbar() done")
    } // populateToolbar

    private fun nonBlockingDialog(dlgID: DlgID, txt: String?) {
        var dlgTitle = 0
        when (dlgID) {
            DlgID.DLG_OKONLY, DlgID.DLG_SCORES -> {}
            DlgID.DLG_USEDICT, DlgID.DLG_GETDICT -> dlgTitle = R.string.inform_dict_title
            else -> Assert.failDbg()
        }
        showDialogFragment(dlgID, dlgTitle, txt)
    }

    // This is failing sometimes, and so the null == m_inviteAlert test means
    // we never post it. BUT on a lot of devices without the test we wind up
    // trying over and over to put the thing up.
    private fun showInviteAlertIf() {
        if (alertOrderAt(StartAlertOrder.INVITE) && !isFinishing()) {
            showOrHide(iNAWrapper)
        }
    }

    private fun showOrHide(wrapper: InvitesNeededAlert.Wrapper) {
        launch {
            mGR!!.getSummary()!!.let { summary ->
                val hostAddr = null as CommsAddrRec? // null in host case
                wrapper.showOrHide(
                    hostAddr, summary.nMissing,
                    summary.nInvited, summary.fromRematch
                )
            }
        }
    }

    private var mINAWrapper: InvitesNeededAlert.Wrapper? = null
    private val iNAWrapper: InvitesNeededAlert.Wrapper
        private get() {
            if (null == mINAWrapper) {
                mINAWrapper = InvitesNeededAlert.Wrapper(this)
                showOrHide(mINAWrapper!!)
            }
            return mINAWrapper!!
        }

//     private fun doZoom(zoomBy: Int): Boolean {
//         val handled = null != mJniThread
//         if (handled) {
//             handleViaThread(JNICmd.CMD_ZOOM, zoomBy)
//         }
//         return handled
//     }

    private fun startChatActivity() {
        launch(Dispatchers.IO) {
            val curPlayer = mGR!!.getLikelyChatter()
            val names = mGi!!.playerNames()
            val locs = mGi!!.playersLocal() // to convert old histories
            ChatDelegate.start( getDelegator(), mGR!!, curPlayer, names, locs )
        }
     }

    private fun doValuesPopup(button: View) {
        val FAKE_GROUP = 100
        val selType = CommonPrefs.get(mActivity).tvType
        val popup = PopupMenu(mActivity, button)
        val menu = popup.menu
        val map: MutableMap<MenuItem, TileValueType> = HashMap()
        for (typ in TileValueType.entries) {
            val item = menu.add(FAKE_GROUP, Menu.NONE, Menu.NONE, typ.expl)
            map[item] = typ
            if (selType == typ) {
                item.setChecked(true)
            }
        }
        menu.setGroupCheckable(FAKE_GROUP, true, true)
        popup.setOnMenuItemClickListener { item ->
            val typ = map[item]
            XWPrefs.setPrefsInt(
                mActivity,
                R.string.key_tile_valuetype,
                typ!!.ordinal
            )
            mGR!!.prefsChanged(CommonPrefs.get(mActivity))
            true
        }
        popup.show()
    }

//     private fun getConfirmPause(isPause: Boolean) {
//         showDialogFragment(DlgID.ASK_DUP_PAUSE, isPause)
//     }

    private fun closeIfFinishing(force: Boolean) {
        if (null == mHandler) {
            // DbgUtils.logf( "closeIfFinishing(): already closed" );
        } else if (force || isFinishing()) {
            // DbgUtils.logf( "closeIfFinishing: closing rowid %d", m_rowid );
            mHandler = null
            ConnStatusHandler.setHandler(null)
            waitCloseGame(true)
        } else {
            mGR!!.save()
        }
    }

    private fun pauseGame() {
        mView!!.stopHandling()
    }

    private fun waitCloseGame(save: Boolean) {
        pauseGame()
    }

    private fun warnIfNoTransport() {
        mGi!!.conTypes?.let { connTypes ->
            if ( alertOrderAt(StartAlertOrder.NO_MEANS)) {
                var pending = false
                if (connTypes.contains(CommsConnType.COMMS_CONN_SMS)) {
                    if (!XWPrefs.getNBSEnabled(mActivity)) {
                        makeConfirmThenBuilder(
                            Action.ENABLE_NBS_ASK,
                            R.string.warn_sms_disabled
                        )
                            .setPosButton(R.string.button_enable_sms)
                            .setNegButton(R.string.button_later)
                            .show()
                        pending = true
                    }
                }
                if (connTypes.contains(CommsConnType.COMMS_CONN_RELAY)) {
                    Log.e(TAG, "opened game with RELAY still")
                }
                if (connTypes.contains(CommsConnType.COMMS_CONN_MQTT)) {
                    var supported = MQTTUtils.MQTTSupported()
                    val msg =
                        if (!supported) { // User has upgraded to hivemq version
                            mDropMQTTOnDismiss = false
                            val buttonTxt = getString(R.string.board_menu_file_email)
                            getString(R.string.warn_mqtt_gone) +
                                "\n\n" +
                                getString(R.string.warn_mqtt_gone_email_fmt, buttonTxt)
                        } else if (!XWPrefs.getMQTTEnabled(mActivity)) {
                            mDropMQTTOnDismiss = false
                            """
                                        ${getString(R.string.warn_mqtt_disabled)}
                                        
                                        ${getString(R.string.warn_mqtt_remove)}
                            """.trimIndent()
                        } else null

                    msg?.let {
                        makeConfirmThenBuilder(Action.ENABLE_MQTT_DO_OR, it)
                            .setPosButton(
                                if ( supported ) R.string.button_enable_mqtt
                                else R.string.board_menu_file_email
                            )
                            .setNegButton(R.string.newgame_drop_mqtt)
                            .show()
                        pending = true
                    }
                }
                if (connTypes.isEmpty()) {
                    askNoAddrsDelete()
                } else if (!pending) {
                    alertOrderIncrIfAt(StartAlertOrder.NO_MEANS)
                }
            }
        }
    }

    private fun tryInvites() {
        Log.d(TAG, "tryInvites($mMissingDevs)")
        mMissingDevs?.let { missingDevs ->
            Assert.assertNotNull(mMissingMeans)
            //             val gameName = GameUtils.getName(mActivity, mRowid)
            for (ii in missingDevs.indices) {
                val dev = missingDevs[ii]
                val nPlayers = mMissingCounts!![ii]
                Assert.assertTrue(0 <= mSummary!!.nGuestDevs)
                val nli = NetLaunchInfo(mActivity, mSummary!!, mGi!!, nPlayers)
                    .setRemotesAreRobots(mRemotesAreRobots)
                var destAddr: CommsAddrRec? = null
                when (mMissingMeans) {
                    InviteMeans.BLUETOOTH -> destAddr = CommsAddrRec(CommsConnType.COMMS_CONN_BT)
                                                 .setBTParams(null, dev)

                    InviteMeans.SMS_DATA -> destAddr = CommsAddrRec(CommsConnType.COMMS_CONN_SMS)
                                                .setSMSParams(dev)

                    InviteMeans.WIFIDIRECT -> WiDirService.inviteRemote(mActivity, dev, nli)
                    InviteMeans.MQTT -> destAddr = CommsAddrRec(CommsConnType.COMMS_CONN_MQTT)
                                            .setMQTTParams(missingDevs[ii])

                    InviteMeans.RELAY -> Assert.failDbg() // not getting here, right?
                    else -> Assert.failDbg()
                }
                destAddr?.let {
                    launch {
                        mGR!!.invite(nli, destAddr, true)
                    }
                    //                 } else {
                    //                     recordInviteSent(mMissingMeans, dev)
                }
            }
            mMissingDevs = null
            mMissingCounts = null
            mMissingMeans = null
        }
    }

    private var m_needsResize = false
    private fun updateToolbar(gsi: GameStateInfo) {
        mToolbar?.let {
            it.update(Buttons.BUTTON_FLIP, gsi.visTileCount >= 1)
                .update(Buttons.BUTTON_VALUES, gsi.visTileCount >= 1)
                .update(Buttons.BUTTON_JUGGLE, gsi.canShuffle)
                .update(Buttons.BUTTON_UNDO, gsi.canRedo)
                .update(Buttons.BUTTON_HINT_PREV, gsi.canHint)
                .update(Buttons.BUTTON_HINT_NEXT, gsi.canHint)
                .update(Buttons.BUTTON_CHAT, gsi.canChat)
                .update(
                    Buttons.BUTTON_BROWSE_DICT,
                    null != mGi!!.dictName(mView!!.curPlayer)
                )
            val count = it.enabledCount()
            if (0 == count) {
                m_needsResize = true
            } else if (m_needsResize && 0 < count) {
                m_needsResize = false
                mView!!.orientationChanged()
            }
        }
    }

    private fun checkButtons() {
        mGR?.let {
            launch {
                mGsi = it.getState()
                updateToolbar(mGsi!!)
            }
        }
    }

    private fun adjustTradeVisibility() {
        mToolbar?.let {
            it.setVisible(!m_mySIS!!.inTrade)
        }
        //         if (null != mTradeButtons) {
        //             mTradeButtons.visibility =
        //                 if (m_mySIS!!.inTrade) View.VISIBLE else View.GONE
        //         }
        //         if (m_mySIS!!.inTrade && null != mExchCommmitButton) {
        //             mExchCommmitButton.setEnabled(mGsi!!.tradeTilesSelected)
        //         }
    }

    private fun setBackgroundColor() {
        findViewById(R.id.board_root)?.let { view ->
        // Google's reported an NPE here, so test
            val back = CommonPrefs.get(mActivity).otherColors[CommonPrefs.COLOR_BACKGRND]
            view.setBackgroundColor(back)
        }
    }

    private fun setKeepScreenOn() {
        val keepOn = CommonPrefs.getKeepScreenOn(mActivity)
        val view = mView!!
        view.keepScreenOn = keepOn
        if (keepOn) {
            if (null == mScreenTimer) {
                mScreenTimer = Runnable {
                    mView?.let{
                        it.keepScreenOn = false
                    }
                }
            }
            removeCallbacks(mScreenTimer) // needed?
            postDelayed(mScreenTimer!!, SCREEN_ON_TIME)
        }
    }

//     override fun post(runnable: Runnable): Boolean {
//         val canPost = null != mHandler
//         if (canPost) {
//             mHandler!!.post(runnable)
//         } else {
//             Log.w(TAG, "post(): dropping b/c handler null")
//             DbgUtils.printStack(TAG)
//         }
//         return canPost
//     }

    private fun postDelayed(runnable: Runnable, postWhen: Int) {
        mHandler?.postDelayed(runnable!!, postWhen.toLong())
            ?:
            Log.w(TAG, "postDelayed: dropping %d because handler null", postWhen)
    }

    private fun removeCallbacks(which: Runnable?) {
        mHandler?.let {
            it.removeCallbacks(which!!)
        }
    }

    private fun wordsToArray(words: String): Array<String> {
        val tmp = TextUtils.split(words, "\n")
            .filter { 0 < it.length }
            .toTypedArray()
        return tmp
    }

    private fun showArchiveNA(rematchAfter: Boolean) {
        makeNotAgainBuilder(
            R.string.key_na_archive, Action.ARCHIVE_ACTION,
            R.string.not_again_archive
        )
            .setParams(rematchAfter)
            .show()
    }

    private fun archiveGame(closeAfter: Boolean) {
        Log.d(TAG, "archiveGame()")
        GameMgr.archiveGame(mGR!!)
        if (closeAfter) {
            waitCloseGame(false)
            finish()
        }
    }

    private fun doRematchIf(archiveAfter: Boolean, deleteAfter: Boolean) {
        doRematchIf(mActivity, mGR!!, mSummary!!, mGi!!, archiveAfter, deleteAfter)
    }

    init {
        mActivity = delegator.getActivity()!!
    }

     private fun nliForMe(): NetLaunchInfo {
         val numHere = 1
        // This is too simple. Need to know if it's a replacement
        // Log.d( TAG, "nliForMe() => %s", nli );
         return NetLaunchInfo(
             mActivity, mSummary!!, mGi!!, numHere
         )
     }

     private fun tryOtherInvites(addr: CommsAddrRec): Boolean {
         Log.d(TAG, "tryOtherInvites(%s)", addr)
         mGR!!.invite(nliForMe(), addr, true)

         //         // Not sure what to do about this recordInviteSent stuff
         //         val conTypes = addr.conTypes
         //         for (typ in conTypes!!) {
         //             when (typ) {
         //                 CommsConnType.COMMS_CONN_MQTT -> {}
         //                 CommsConnType.COMMS_CONN_BT -> {}
         //                 CommsConnType.COMMS_CONN_SMS -> {}
         //                 CommsConnType.COMMS_CONN_NFC -> {}
         //                 else -> {
         //                     Log.d(TAG, "not inviting using addr type %s", typ)
         //                     Assert.failDbg()
         //                 }
         //             }
         //         }
         return true
     }

//     private fun sendNBSInviteIf(phone: String, nli: NetLaunchInfo, askOk: Boolean) {
//         if (XWPrefs.getNBSEnabled(mActivity)) {
//             NBSProto.inviteRemote(mActivity, phone, nli)
//             recordInviteSent(InviteMeans.SMS_DATA, phone)
//         } else if (askOk) {
//             makeConfirmThenBuilder(
//                 Action.ENABLE_NBS_ASK,
//                 R.string.warn_sms_disabled
//             )
//                 .setPosButton(R.string.button_enable_sms)
//                 .setNegButton(R.string.button_later)
//                 .setParams(nli, phone)
//                 .show()
//         }
//     }

//     private fun retryNBSInvites(params: Array<out Any?>) {
//         if (2 == params.size && params[0] is NetLaunchInfo?
//             && params[1] is String
//         ) {
//             sendNBSInviteIf(
//                 params[1] as String, params[0] as NetLaunchInfo,
//                 false
//             )
//         } else {
//             Log.w(TAG, "retryNBSInvites: tests failed")
//         }
//     }

//     private fun recordInviteSent(means: InviteMeans?, dev: String?) {
//         var invitesSent = true
//         if (!mShowedReInvite) { // have we sent since?
//             val sentInfo = DBUtils.getInvitesFor(mActivity, mRowid)
//             val nSent = sentInfo.minPlayerCount
//             invitesSent = nSent >= m_mySIS!!.nMissing
//         }
//         DBUtils.recordInviteSent(mActivity, mRowid, means!!, dev, false)
//         if (!invitesSent) {
//             Log.d(TAG, "recordInviteSent(): redoing invite alert")
//             showInviteAlertIf()
//         }
//     }

    companion object {
        private val TAG = BoardDelegate::class.java.getSimpleName()
        private const val SCREEN_ON_TIME = 10 * 60 * 1000 // 10 mins
        private val SAVE_MYSIS = TAG + "/MYSIS"
//         @JvmField
        val PAUSER_KEY = TAG + "/pauser"
//         private var s_noLockCount = 0 // supports a quick debugging hack
//         private var s_themeNAShown = false
//         private const val mCounter = 0

        private fun doRematchIf(activity: Activity, gr: GameRef,
                                summary: GameSummary, gi: CurGameInfo,
                                archiveAfter: Boolean, deleteAfter: Boolean
        ) {
            val intent = GamesListDelegate
                .makeRematchIntent(
                    activity, gr, gi,
                    gi.conTypes, archiveAfter, deleteAfter
                )
            activity.startActivity(intent)
        }

        fun setupRematchFor(activity: Activity, gr: GameRef) {
            Utils.launch {
                val summary = gr.getSummary()!!
                val gi = gr.getGI()!!
                doRematchIf(activity, gr, summary, gi, false, false )
            }
        }

//         // This might need to map rowid->openCount so opens can stack
        var sOpenGames: HashMap<Long, WeakReference<BoardDelegate>> = HashMap()
        private fun noteOpened(context: Context, gr: GameRef, self: BoardDelegate) {
            Log.d(TAG, "noteOpened($gr)")
            if (BuildConfig.NON_RELEASE && sOpenGames.contains(gr.gr)) {
                val msg = String.format("noteOpened($gr): already open" )
                Utils.showToast(context, msg)
                DbgUtils.printStack(TAG)
            } else {
                sOpenGames.put(gr.gr, WeakReference<BoardDelegate>(self))
            }
        }

        private fun noteClosed(gr: GameRef) {
            Log.d(TAG, "noteClosed($gr)")
            Assert.assertTrueNR(sOpenGames.contains(gr.gr))
            sOpenGames.remove(gr.gr)
        }

        fun gameIsOpen(gr: GameRef): Boolean {
            val result = sOpenGames.contains(gr.gr)
            Log.d(TAG, "gameIsOpen($gr) => %b", result)
            return result
        }

        fun getIfOpen(gr: GameRef): BoardDelegate? {
            val result = sOpenGames.get(gr.gr)?.get()
            Log.d(TAG, "getIfOpen($gr) => %b", result)
            return result
        }
    }
}

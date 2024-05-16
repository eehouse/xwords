/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import java.io.Serializable

import org.eehouse.android.xw4.ConnStatusHandler.ConnStatusCBacks
import org.eehouse.android.xw4.DBUtils.SentInvitesInfo
import org.eehouse.android.xw4.DictUtils.ON_SERVER
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener
import org.eehouse.android.xw4.GameOverAlert.OnDoneProc
import org.eehouse.android.xw4.GameUtils.BackMoveResult
import org.eehouse.android.xw4.GameUtils.NoSuchGameException
import org.eehouse.android.xw4.MultiService.MultiEvent
import org.eehouse.android.xw4.NFCUtils.Wrapper
import org.eehouse.android.xw4.NFCUtils.Wrapper.Procs
import org.eehouse.android.xw4.NFCUtils.addInvitationFor
import org.eehouse.android.xw4.NFCUtils.getNFCDevID
import org.eehouse.android.xw4.NFCUtils.makeEnableNFCDialog
import org.eehouse.android.xw4.NFCUtils.nfcAvail
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.Perms23.PermCbck
import org.eehouse.android.xw4.TilePickAlert.TilePickState
import org.eehouse.android.xw4.Toolbar.Buttons
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.gen.PrefsWrappers
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
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.jni.JNIThread
import org.eehouse.android.xw4.jni.JNIThread.GameStateInfo
import org.eehouse.android.xw4.jni.JNIThread.JNICmd
import org.eehouse.android.xw4.jni.TransportProcs.TPMsgHandler
import org.eehouse.android.xw4.jni.UtilCtxt
import org.eehouse.android.xw4.jni.UtilCtxtImpl
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.GamePtr
import org.eehouse.android.xw4.jni.XwJNI.XP_Key
import org.eehouse.android.xw4.loc.LocUtils
import kotlin.concurrent.Volatile

class BoardDelegate(delegator: Delegator, savedInstanceState: Bundle?) :
    DelegateBase(delegator, savedInstanceState, R.layout.board, R.menu.board_menu), TPMsgHandler,
    View.OnClickListener, DownloadFinishedListener, ConnStatusCBacks, Procs,
    InvitesNeededAlert.Callbacks {
    private val mActivity: Activity
    private var mView: BoardView? = null
    private var mJniGamePtr: GamePtr? = null
    private var mGi: CurGameInfo? = null
    private var mSummary: GameSummary? = null
    private var mHandler: Handler? = null
    private var mTimers = HashMap<Int, TimerRunnable>()
    private var mScreenTimer: Runnable? = null
    private var mRowid: Long = 0
    private var mToolbar: Toolbar? = null
    private val mTradeButtons: View? = null
    private val mExchCommmitButton: Button? = null
    private val mExchCancelButton: Button? = null
    private val mSentInfo: SentInvitesInfo? = null
    private var mPermCbck: PermCbck? = null
    private var mConnTypes: CommsConnTypeSet? = null
    private var mMissingDevs: Array<String>? = null
    private var mMissingCounts: IntArray? = null
    private var mRemotesAreRobots = false
    private var mMissingMeans: InviteMeans? = null
    private var mIsFirstLaunch = false
    private var mFiringPrefs = false
    private var mUtils: BoardUtilCtxt? = null
    private var mGameOver = false

    @Volatile
    private var mJniThread: JNIThread? = null
    private var mJniThreadRef: JNIThread? = null
    private var mResumeSkipped = false
    private var mStartSkipped = false
    private var mGsi: GameStateInfo? = null
    private val mShowedReInvite = false
    private var mOverNotShown = false
    private var mDropMQTTOnDismiss = false
    private val mHaveStartedShowing = false
    private var mSawNewShown = false
    private var mNFCWrapper: NFCUtils.Wrapper? = null
    private var mGameOverAlert: GameOverAlert? = null // how to clear after?

    inner class TimerRunnable(
        private val m_why: Int,
        private val m_when: Int,
        private val m_handle: Int
    ) : Runnable {
        override fun run() {
            mTimers.remove(m_why)
            if (null != mJniThread) {
                mJniThread!!.handleBkgrnd(
                    JNICmd.CMD_TIMER_FIRED,
                    m_why, m_when, m_handle
                )
            }
        }
    }

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
        var nMissing = -1
        var nInvited = 0
        var nGuestDevs = 0
        var hostAddr: CommsAddrRec? = null
        var inTrade = false
        var fromRematch = false
        var mAlertOrder = StartAlertOrder.entries[0]
    }

    private var m_mySIS: MySIS? = null
    private fun alertOrderAt(ord: StartAlertOrder): Boolean {
        // Log.d( TAG, "alertOrderAt(%s) => %b (at %s)", ord, result,
        // m_mySIS.mAlertOrder );
        return m_mySIS!!.mAlertOrder == ord
    }

    private fun alertOrderIncrIfAt(ord: StartAlertOrder) {
        // Log.d( TAG, "alertOrderIncrIfAt(%s)", ord );
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

    override fun makeDialog(alert: DBAlert, vararg params: Any): Dialog {
        Assert.assertVarargsNotNullNR(params)
        val dlgID = alert.dlgID
        Log.d(TAG, "makeDialog(%s)", dlgID.toString())
        val lstnr: DialogInterface.OnClickListener
        var ab = makeAlertBuilder() // used everywhere...
        Assert.assertTrueNR(!isFinishing())
        val dialog: Dialog
        when (dlgID) {
            DlgID.DLG_OKONLY -> {
                val title = params[0] as Int
                if (0 != title) {
                    ab.setTitle(title)
                }
                val msg = params[1] as String
                ab.setMessage(msg)
                    .setPositiveButton(android.R.string.ok, null)
                dialog = ab.create()
            }

            DlgID.DLG_USEDICT, DlgID.DLG_GETDICT -> {
                val title = params[0] as Int
                val msg = params[1] as String
                lstnr = DialogInterface.OnClickListener { dlg, whichButton ->
                    if (DlgID.DLG_USEDICT == dlgID) {
                        setGotGameDict(m_mySIS!!.getDict)
                    } else {
                        DwnldDelegate
                            .downloadDictInBack(
                                mActivity, mGi!!.isoCode(),
                                m_mySIS!!.getDict,
                                this@BoardDelegate
                            )
                    }
                }
                dialog = ab.setTitle(title)
                    .setMessage(msg)
                    .setPositiveButton(R.string.button_yes, lstnr)
                    .setNegativeButton(R.string.button_no, null)
                    .create()
            }

            DlgID.DLG_DELETED -> {
                val gameName = GameUtils.getName(mActivity, mRowid)
                val expl = if (params.size == 0) null else params[0] as ConnExpl
                var message = getString(R.string.msg_dev_deleted_fmt, gameName)
                if (BuildConfig.NON_RELEASE && null != expl) {
                    message += """
                        
                        
                        ${expl.getUserExpl(mActivity)}
                        """.trimIndent()
                }
                ab = ab.setMessage(message)
                    .setPositiveButton(android.R.string.ok, null)
                lstnr = DialogInterface.OnClickListener { dlg, whichButton -> deleteAndClose() }
                ab.setNegativeButton(R.string.button_delete, lstnr)
                ab.setNeutralButton(
                    R.string.button_archive
                ) { dlg, whichButton -> showArchiveNA(false) }
                dialog = ab.create()
            }

            DlgID.QUERY_TRADE, DlgID.QUERY_MOVE -> {
                val msg = params[0] as String
                lstnr = DialogInterface.OnClickListener { dialog, whichButton ->
                    handleViaThread(
                        JNICmd.CMD_COMMIT,
                        true,
                        true
                    )
                }
                dialog = ab.setMessage(msg)
                    .setTitle(R.string.query_title)
                    .setPositiveButton(R.string.button_yes, lstnr)
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            DlgID.ASK_BADWORDS -> {
                val count = params[1] as Int
                val badWordsKey = params[2] as Int
                lstnr = DialogInterface.OnClickListener { dlg, bx ->
                    handleViaThread(
                        JNICmd.CMD_COMMIT,
                        true,
                        false,
                        0
                    )
                }
                val lstnr2 = DialogInterface.OnClickListener { dlg, bx ->
                    handleViaThread(
                        JNICmd.CMD_COMMIT, true,
                        false, badWordsKey
                    )
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
                dialog = ab.setTitle(R.string.phonies_found_title)
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
                                if (studyOn) R.string.button_lookup_study_fmt else R.string.button_lookup_fmt
                            getString(resID, m_mySIS!!.words!![0])
                        } else {
                            val resID =
                                if (studyOn) R.string.button_lookup_study else R.string.button_lookup
                            getString(resID)
                        }
                        lstnr = DialogInterface.OnClickListener { dialog, whichButton ->
                            makeNotAgainBuilder(
                                R.string.key_na_lookup,
                                DlgDelegate.Action.LOOKUP_ACTION,
                                R.string.not_again_lookup
                            )
                                .show()
                        }
                        ab.setNegativeButton(buttonTxt, lstnr)
                    }
                }
                dialog = ab.create()
            }

            DlgID.ASK_PASSWORD -> {
                val player = params[0] as Int
                val name = params[1] as String
                val pwdLayout = inflate(R.layout.passwd_view) as LinearLayout
                val edit = pwdLayout.findViewById<View>(R.id.edit) as EditText
                ab.setTitle(getString(R.string.msg_ask_password_fmt, name))
                    .setView(pwdLayout)
                    .setPositiveButton(
                        android.R.string.ok
                    ) { dlg, whichButton ->
                        val pwd = edit.getText().toString()
                        handleViaThread(
                            JNICmd.CMD_PASS_PASSWD,
                            player, pwd
                        )
                    }
                dialog = ab.create()
            }

            DlgID.GET_DEVID -> {
                val et = inflate(R.layout.edittext) as EditText
                dialog = ab
                    .setTitle(R.string.title_pasteDevid)
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

            DlgID.MQTT_PEERS -> {
                val psv = inflate(R.layout.peers_status) as PeerStatusView
                val selfAddr = XwJNI.comms_getSelfAddr(mJniGamePtr)
                psv.configure(mGi!!.gameID, selfAddr.mqtt_devID)
                dialog = ab
                    .setTitle(R.string.menu_about_peers)
                    .setView(psv)
                    .setPositiveButton(android.R.string.ok, null)
                    .setNegativeButton(R.string.button_refresh) { dlg, bttn ->
                        showDialogFragment(
                            DlgID.MQTT_PEERS
                        )
                    }
                    .create()
            }

            DlgID.ASK_DUP_PAUSE -> {
                val isPause = params[0] as Boolean
                val pauseView = (inflate(R.layout.pause_view) as ConfirmPauseView)
                    .setIsPause(isPause)
                val buttonId =
                    if (isPause) R.string.board_menu_game_pause else R.string.board_menu_game_unpause
                dialog = ab
                    .setTitle(if (isPause) R.string.pause_title else R.string.unpause_title)
                    .setView(pauseView)
                    .setPositiveButton(buttonId) { dlg, whichButton ->
                        val msg = pauseView.msg
                        handleViaThread(if (isPause) JNICmd.CMD_PAUSE else JNICmd.CMD_UNPAUSE, msg)
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            DlgID.QUERY_ENDGAME -> dialog = ab.setTitle(R.string.query_title)
                .setMessage(R.string.ids_endnow)
                .setPositiveButton(
                    R.string.button_yes
                ) { dlg, item -> handleViaThread(JNICmd.CMD_ENDGAME) }
                .setNegativeButton(R.string.button_no, null)
                .create()

            DlgID.DLG_INVITE -> dialog = iNAWrapper.make(alert, params)
            DlgID.ENABLE_NFC -> dialog = makeEnableNFCDialog(mActivity)
            else -> dialog = super.makeDialog(alert, *params)
        }
        return dialog
    } // makeDialog

    private var mDeletePosted = false
    private fun postDeleteOnce(expl: ConnExpl?) {
        if (!mDeletePosted) {
            // PENDING: could clear this if user says "ok" rather than "delete"
            mDeletePosted = true
            post { showDialogFragment(DlgID.DLG_DELETED, expl) }
        }
    }

    override fun init(savedInstanceState: Bundle?) {
        mIsFirstLaunch = null == savedInstanceState
        getBundledData(savedInstanceState)
        val devID = getNFCDevID(mActivity)
        mNFCWrapper = Wrapper.init(mActivity, this, devID)
        mUtils = BoardUtilCtxt()
        // needs to be in sync with XWTimerReason
        mView = findViewById(R.id.board_view) as BoardView
        val args = arguments
        mRowid = args.getLong(GameUtils.INTENT_KEY_ROWID, -1)
        Log.i(TAG, "opening rowid %d", mRowid)
        mOverNotShown = true
        noteOpened(mActivity, mRowid)
    } // init

    private val lock: Unit
        private get() {
            GameLock.getLockThen(
                mRowid, 100L, Handler()
            )  // this doesn't unlock
            { lock ->
                if (null == lock) {
                    finish()
                    if (BuildConfig.REPORT_LOCKS && ++s_noLockCount == 3) {
                        val msg = ("BoardDelegate unable to get lock; holder stack: "
                                + GameLock.getHolderDump(mRowid))
                        Log.e(TAG, msg)
                    }
                } else {
                    s_noLockCount = 0
                    mJniThreadRef = JNIThread.getRetained(lock)
                    lock.release()

                    // see http://stackoverflow.com/questions/680180/where-to-stop- \
                    // destroy-threads-in-android-service-class
                    mJniThreadRef!!.setDaemonOnce(true)
                    mJniThreadRef!!.startOnce()
                    setBackgroundColor()
                    setKeepScreenOn()
                    if (mStartSkipped) {
                        doResume(true)
                    }
                    if (mResumeSkipped) {
                        doResume(false)
                    }
                    if (mSummary!!.quashed && !inArchiveGroup()) {
                        postDeleteOnce(null)
                    }
                }
            }
        } // getLock

    override fun onStart() {
        super.onStart()
        if (null != mJniThreadRef) {
            doResume(true)
        } else {
            mStartSkipped = true
        }
        newThemeFeatureAlert()
    }

    private fun newThemeFeatureAlert() {
        if (!s_themeNAShown) {
            s_themeNAShown = true
            if (CommonPrefs.darkThemeEnabled(mActivity)) {
                val prefsName = LocUtils.getString(mActivity, R.string.theme_which)
                makeNotAgainBuilder(
                    R.string.key_na_boardThemes,
                    R.string.not_again_boardThemes_fmt,
                    prefsName
                )
                    .setTitle(R.string.new_feature_title)
                    .setActionPair(DlgDelegate.Action.LAUNCH_THEME_CONFIG, R.string.button_settings)
                    .show()
            }
        }
    }

    override fun onResume() {
        super.onResume()
        Wrapper.setResumed(mNFCWrapper, true)
        if (null != mJniThreadRef) {
            doResume(false)
        } else {
            mResumeSkipped = true
            lock
        }
    }

    override fun onPause() {
        Wrapper.setResumed(mNFCWrapper, false)
        closeIfFinishing(false)
        mHandler = null
        ConnStatusHandler.setHandler(null)
        waitCloseGame(true)
        pauseGame() // sets m_jniThread to null
        super.onPause()
    }

    override fun onStop() {
        if (isFinishing()) {
            releaseThreadOnce()
        }
        super.onStop()
    }

    override fun onDestroy() {
        closeIfFinishing(true)
        releaseThreadOnce()
        GamesListDelegate.boardDestroyed(mRowid)
        noteClosed(mRowid)
        super.onDestroy()
    }

    @Throws(Throwable::class)
    fun finalize() {
        // This logging never shows up. Likely a logging limit
        Log.d(TAG, "finalize()")
        if (releaseThreadOnce()) {
            Log.e(TAG, "oops! Caught the leak")
        }
    }

    @Synchronized
    private fun releaseThreadOnce(): Boolean {
        val needsRelease = null != mJniThreadRef
        if (needsRelease) {
            mJniThreadRef!!.release()
            mJniThreadRef = null
        }
        return needsRelease
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putSerializable(SAVE_MYSIS, m_mySIS)
        super.onSaveInstanceState(outState)
    }

    private fun getBundledData(bundle: Bundle?) {
        m_mySIS = if (null != bundle) {
            bundle.getSerializable(SAVE_MYSIS) as MySIS?
        } else {
            MySIS()
        }
    }

    override fun onActivityResult(
        requestCode: RequestCode, resultCode: Int,
        data: Intent?
    ) {
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
            if (null != missingMeans) {
                // onActivityResult is called immediately *before* onResume --
                // meaning m_gi etc are still null.
                val data = data!!
                mMissingDevs = data.getStringArrayExtra(InviteDelegate.DEVS)
                mMissingCounts = data.getIntArrayExtra(InviteDelegate.COUNTS)
                mRemotesAreRobots = data.getBooleanExtra(InviteDelegate.RAR, false)
                mMissingMeans = missingMeans
                post { tryInvites() }
            }
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        // This is not called when dialog fragment comes/goes away
        if (hasFocus) {
            if (mFiringPrefs) {
                mFiringPrefs = false
                if (null != mJniThread) {
                    handleViaThread(JNICmd.CMD_PREFS_CHANGE)
                }
                // in case of change...
                setBackgroundColor()
                setKeepScreenOn()
            } else {
                warnIfNoTransport()
                showInviteAlertIf()
            }
        }
    }

    // Invitations need to check phone state to decide whether to offer SMS
    // invitation. Complexity (showRationale) boolean is to prevent infinite
    // loop of showing the rationale over and over. Android will always tell
    // us to show the rationale, but if we've done it already we need to go
    // straight to asking for the permission.
    private fun callInviteChoices() {
        if (!Perms23.NBSPermsInManifest(mActivity)) {
            showInviteChoicesThen()
        } else {
            Perms23.tryGetPermsNA(
                this, Perm.READ_PHONE_STATE,
                R.string.phone_state_rationale,
                R.string.key_na_perms_phonestate,
                DlgDelegate.Action.ASKED_PHONE_STATE
            )
        }
    }

    private fun showInviteChoicesThen() {
        val nli = nliForMe()
        if (ON_SERVER.NO != DictLangCache.getOnServer(mActivity, nli.dict)) {
            onPosButton(DlgDelegate.Action.CUSTOM_DICT_CONFIRMED, nli)
        } else {
            val txt = LocUtils
                .getString(
                    mActivity, R.string.invite_custom_warning_fmt,
                    nli.dict
                )
            makeConfirmThenBuilder(DlgDelegate.Action.CUSTOM_DICT_CONFIRMED, txt)
                .setNegButton(R.string.list_item_config)
                .setActionPair(
                    DlgDelegate.Action.DELETE_AND_EXIT,
                    R.string.button_delete_game
                )
                .setParams(nli)
                .show()
        }
    }

    override fun setTitle() {
        var title = GameUtils.getName(mActivity, mRowid)
        if (null != mGi && mGi!!.inDuplicateMode) {
            title = LocUtils.getString(mActivity, R.string.dupe_title_fmt, title)
        }
        setTitle(title)
    }

    private fun initToolbar() {
        // Wait until we're attached....
        if (null != findViewById(R.id.tbar_parent_hor)) {
            if (null == mToolbar) {
                mToolbar = Toolbar(mActivity, this)
                populateToolbar()
            }
        }
    }

    protected fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (null != mJniThread) {
            val xpKey = keyCodeToXPKey(keyCode)
            if (XP_Key.XP_KEY_NONE != xpKey) {
                handleViaThread(JNICmd.CMD_KEYDOWN, xpKey)
            }
        }
        return false
    }

    protected fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        var handled = false
        if (null != mJniThread) {
            val xpKey = keyCodeToXPKey(keyCode)
            if (XP_Key.XP_KEY_NONE != xpKey) {
                handleViaThread(JNICmd.CMD_KEYUP, xpKey)
                handled = true
            }
        }
        return handled
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        var inTrade = false
        var item: MenuItem
        var strId: Int
        var enable: Boolean
        if (null != mGsi) {
            inTrade = mGsi!!.inTrade
            menu.setGroupVisible(R.id.group_done, !inTrade)
            menu.setGroupVisible(R.id.group_exchange, inTrade)
            strId = if (UtilCtxt.TRAY_REVEALED == mGsi!!.trayVisState) {
                R.string.board_menu_tray_hide
            } else {
                R.string.board_menu_tray_show
            }
            item = menu.findItem(R.id.board_menu_tray)
            item.setTitle(getString(strId))
            Utils.setItemVisible(
                menu, R.id.board_menu_flip,
                mGsi!!.visTileCount >= 1
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_juggle,
                mGsi!!.canShuffle
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_undo_current,
                mGsi!!.canRedo
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_hint_prev,
                mGsi!!.canHint
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_hint_next,
                mGsi!!.canHint
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_chat,
                mGsi!!.canChat
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_tray,
                !inTrade && mGsi!!.canHideRack
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_trade,
                mGsi!!.canTrade
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_undo_last,
                mGsi!!.canUndo
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_game_pause,
                mGsi!!.canPause
            )
            Utils.setItemVisible(
                menu, R.id.board_menu_game_unpause,
                mGsi!!.canUnpause
            )
        }
        Utils.setItemVisible(menu, R.id.board_menu_trade_cancel, inTrade)
        Utils.setItemVisible(
            menu, R.id.board_menu_trade_commit,
            inTrade && mGsi!!.tradeTilesSelected
        )
        Utils.setItemVisible(menu, R.id.board_menu_game_resign, !inTrade)
        if (!inTrade) {
            enable = null == mGsi || mGsi!!.curTurnSelected
            item = menu.findItem(R.id.board_menu_done)
            item.setVisible(enable)
            if (enable) {
                strId = if (0 >= mView!!.curPending()) {
                    R.string.board_menu_pass
                } else {
                    R.string.board_menu_done
                }
                item.setTitle(getString(strId))
            }
            if (mGameOver || DBUtils.gameOver(mActivity, mRowid)) {
                mGameOver = true
                item = menu.findItem(R.id.board_menu_game_resign)
                item.setTitle(getString(R.string.board_menu_game_final))
            }
        }
        enable = null != mSummary && mSummary!!.canRematch
        Utils.setItemVisible(menu, R.id.board_menu_rematch, enable)
        enable = mGameOver && !inArchiveGroup()
        Utils.setItemVisible(menu, R.id.board_menu_archive, enable)
        val netGame = (null != mGi
                && DeviceRole.SERVER_STANDALONE != mGi!!.serverRole)
        enable = netGame && null != mGsi && 0 < mGsi!!.nPendingMessages
        Utils.setItemVisible(menu, R.id.board_menu_game_resend, enable)
        enable = netGame && (BuildConfig.DEBUG
                || XWPrefs.getDebugEnabled(mActivity))
        Utils.setItemVisible(menu, R.id.board_menu_game_netstats, enable)
        Utils.setItemVisible(menu, R.id.board_menu_game_invites, enable)
        enable = XWPrefs.getStudyEnabled(mActivity) && null != mGi && !DBUtils.studyListWords(
            mActivity,
            mGi!!.isoCode()
        ).isEmpty()
        Utils.setItemVisible(menu, R.id.board_menu_study, enable)
        return true
    } // onPrepareOptionsMenu

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        var handled = true
        var cmd = JNICmd.CMD_NONE
        val proc: Runnable? = null
        val id = item.itemId
        when (id) {
            R.id.board_menu_done -> {
                val nTiles = XwJNI.model_getNumTilesInTray(
                    mJniGamePtr,
                    mView!!.curPlayer
                )
                if (mGi!!.traySize > nTiles) {
                    makeNotAgainBuilder(
                        R.string.key_notagain_done,
                        DlgDelegate.Action.COMMIT_ACTION, R.string.not_again_done
                    )
                        .show()
                } else {
                    onPosButton(DlgDelegate.Action.COMMIT_ACTION)
                }
            }

            R.id.board_menu_rematch -> doRematchIf(false)
            R.id.board_menu_archive -> showArchiveNA(false)
            R.id.board_menu_trade_commit -> cmd = JNICmd.CMD_COMMIT
            R.id.board_menu_trade_cancel -> cmd = JNICmd.CMD_CANCELTRADE
            R.id.board_menu_hint_prev -> cmd = JNICmd.CMD_PREV_HINT
            R.id.board_menu_hint_next -> cmd = JNICmd.CMD_NEXT_HINT
            R.id.board_menu_juggle -> cmd = JNICmd.CMD_JUGGLE
            R.id.board_menu_flip -> cmd = JNICmd.CMD_FLIP
            R.id.board_menu_chat -> startChatActivity()
            R.id.board_menu_trade -> {
                var msg = getString(R.string.not_again_trading)
                msg += getString(R.string.not_again_trading_menu)
                makeNotAgainBuilder(
                    R.string.key_notagain_trading,
                    DlgDelegate.Action.START_TRADE_ACTION, msg
                )
                    .show()
            }

            R.id.board_menu_tray -> cmd = JNICmd.CMD_TOGGLE_TRAY
            R.id.board_menu_study -> StudyListDelegate.launch(delegator, mGi!!.isoCode())
            R.id.board_menu_game_netstats -> handleViaThread(
                JNICmd.CMD_NETSTATS,
                R.string.netstats_title
            )

            R.id.board_menu_game_invites -> {
                val sentInfo = DBUtils.getInvitesFor(mActivity, mRowid)
                makeOkOnlyBuilder(sentInfo.getAsText(mActivity)).show()
            }

            R.id.board_menu_undo_current -> cmd = JNICmd.CMD_UNDO_CUR
            R.id.board_menu_undo_last -> makeConfirmThenBuilder(
                DlgDelegate.Action.UNDO_LAST_ACTION,
                R.string.confirm_undo_last
            )
                .show()

            R.id.board_menu_game_pause, R.id.board_menu_game_unpause -> getConfirmPause(R.id.board_menu_game_pause == id)
            R.id.board_menu_dict -> {
                val dictName = mGi!!.dictName(mView!!.curPlayer)
                DictBrowseDelegate.launch(delegator, dictName)
            }

            R.id.board_menu_game_counts -> handleViaThread(
                JNICmd.CMD_COUNTS_VALUES,
                R.string.counts_values_title
            )

            R.id.board_menu_game_left -> handleViaThread(
                JNICmd.CMD_REMAINING,
                R.string.tiles_left_title
            )

            R.id.board_menu_game_history -> handleViaThread(
                JNICmd.CMD_HISTORY,
                R.string.history_title
            )

            R.id.board_menu_game_resign -> handleViaThread(JNICmd.CMD_FINAL, R.string.history_title)
            R.id.board_menu_game_resend -> handleViaThread(JNICmd.CMD_RESEND, true, false, true)
            R.id.board_menu_file_prefs -> {
                mFiringPrefs = true
                PrefsDelegate.launch(mActivity)
            }

            else -> {
                Log.w(TAG, "menuitem %d not handled", id)
                handled = false
            }
        }
        if (handled && cmd != JNICmd.CMD_NONE) {
            handleViaThread(cmd)
        }
        return handled
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any): Boolean {
        Assert.assertVarargsNotNullNR(params)
        Log.d(TAG, "onPosButton(%s, %s)", action, DbgUtils.toStr(arrayOf(params)))
        var handled = true
        var cmd: JNICmd? = null
        when (action) {
            DlgDelegate.Action.ENABLE_MQTT_DO_OR -> {
                XWPrefs.setMQTTEnabled(mActivity, true)
                MQTTUtils.setEnabled(mActivity, true)
            }

            DlgDelegate.Action.UNDO_LAST_ACTION -> cmd = JNICmd.CMD_UNDO_LAST
            DlgDelegate.Action.SMS_CONFIG_ACTION -> PrefsDelegate.launch(mActivity)
            DlgDelegate.Action.COMMIT_ACTION -> cmd = JNICmd.CMD_COMMIT
            DlgDelegate.Action.SHOW_EXPL_ACTION -> {
                showToast(m_mySIS!!.toastStr)
                m_mySIS!!.toastStr = null
            }

            DlgDelegate.Action.BUTTON_BROWSEALL_ACTION, DlgDelegate.Action.BUTTON_BROWSE_ACTION -> {
                val curDict = mGi!!.dictName(mView!!.curPlayer)
                val button: View = mToolbar!!.getButtonFor(Buttons.BUTTON_BROWSE_DICT)
                Assert.assertTrueNR(null != mGi!!.isoCode())
                if (DlgDelegate.Action.BUTTON_BROWSEALL_ACTION == action &&
                    DictsDelegate.handleDictsPopup(
                        delegator, button,
                        curDict, mGi!!.isoCode()
                    )
                ) {
                    // do nothing
                } else {
                    var selDict = DictsDelegate.prevSelFor(mActivity, mGi!!.isoCode())
                        ?: curDict
                    DictBrowseDelegate.launch(delegator, selDict)
                }
            }

            DlgDelegate.Action.PREV_HINT_ACTION -> cmd = JNICmd.CMD_PREV_HINT
            DlgDelegate.Action.NEXT_HINT_ACTION -> cmd = JNICmd.CMD_NEXT_HINT
            DlgDelegate.Action.JUGGLE_ACTION -> cmd = JNICmd.CMD_JUGGLE
            DlgDelegate.Action.FLIP_ACTION -> cmd = JNICmd.CMD_FLIP
            DlgDelegate.Action.UNDO_ACTION -> cmd = JNICmd.CMD_UNDO_CUR
            DlgDelegate.Action.VALUES_ACTION -> doValuesPopup(mToolbar!!.getButtonFor(Buttons.BUTTON_VALUES))
            DlgDelegate.Action.CHAT_ACTION -> startChatActivity()
            DlgDelegate.Action.START_TRADE_ACTION -> {
                showTradeToastOnce(true)
                cmd = JNICmd.CMD_TRADE
            }

            DlgDelegate.Action.LOOKUP_ACTION -> launchLookup(m_mySIS!!.words, mGi!!.isoCode())
            DlgDelegate.Action.DROP_MQTT_ACTION -> dropConViaAndRestart(CommsConnType.COMMS_CONN_MQTT)
            DlgDelegate.Action.DELETE_AND_EXIT -> deleteAndClose()
            DlgDelegate.Action.DROP_SMS_ACTION -> alertOrderIncrIfAt(StartAlertOrder.NBS_PERMS)
            DlgDelegate.Action.INVITE_SMS_DATA -> {
                val nMissing = params[0] as Int
                val info = params[1] as SentInvitesInfo
                launchPhoneNumberInvite(
                    nMissing, info,
                    RequestCode.SMS_DATA_INVITE_RESULT
                )
            }

            DlgDelegate.Action.ASKED_PHONE_STATE -> showInviteChoicesThen()
            DlgDelegate.Action.BLANK_PICKED -> {
                val tps = params[0] as TilePickState
                val newTiles = params[1] as IntArray
                handleViaThread(
                    JNICmd.CMD_SET_BLANK, tps.playerNum,
                    tps.col, tps.row, newTiles[0]
                )
            }

            DlgDelegate.Action.TRAY_PICKED -> {
                val tps = params[0] as TilePickState
                val newTiles = params[1] as IntArray
                if (tps.isInitial) {
                    handleViaThread(JNICmd.CMD_TILES_PICKED, tps.playerNum, newTiles)
                } else {
                    handleViaThread(JNICmd.CMD_COMMIT, true, true, newTiles)
                }
            }

            DlgDelegate.Action.DISABLE_DUALPANE -> {
                XWPrefs.setPrefsString(
                    mActivity, R.string.key_force_tablet,
                    getString(R.string.force_tablet_phone)
                )
                makeOkOnlyBuilder(R.string.after_restart).show()
            }

            DlgDelegate.Action.ARCHIVE_ACTION -> {
                val rematchAfter = params.size >= 1 && params[0] as Boolean
                val curGroup = DBUtils.getGroupForGame(mActivity, mRowid)
                archiveGame(!rematchAfter)
                if (rematchAfter) {
                    doRematchIf(curGroup, false) // closes game
                }
            }

            DlgDelegate.Action.REMATCH_ACTION -> {
                val archiveAfter = params.size >= 1 && params[0] as Boolean
                val deleteAfter = params.size >= 2 && params[1] as Boolean
                Assert.assertTrueNR(false == archiveAfter || false == deleteAfter)
                if (archiveAfter) {
                    showArchiveNA(true)
                } else {
                    doRematchIf(deleteAfter) // closes game
                }
            }

            DlgDelegate.Action.DELETE_ACTION -> if (0 < params.size && params[0] as Boolean) {
                deleteAndClose()
            } else {
                makeConfirmThenBuilder(
                    DlgDelegate.Action.DELETE_ACTION,
                    R.string.confirm_delete
                )
                    .setParams(true)
                    .show()
            }

            DlgDelegate.Action.CUSTOM_DICT_CONFIRMED -> {
                val nli = params[0] as NetLaunchInfo
                showInviteChoicesThen(
                    DlgDelegate.Action.LAUNCH_INVITE_ACTION, nli,
                    m_mySIS!!.nMissing, m_mySIS!!.nInvited
                )
            }

            DlgDelegate.Action.LAUNCH_INVITE_ACTION -> for (obj in params) {
                if (obj is CommsAddrRec) {
                    tryOtherInvites(obj)
                } else {
                    break
                }
            }

            DlgDelegate.Action.LAUNCH_THEME_CONFIG -> PrefsDelegate.launch(
                mActivity, PrefsWrappers.prefs_appear_themes::class.java
            )

            DlgDelegate.Action.LAUNCH_THEME_COLOR_CONFIG -> {
                val clazz: Class<*> =
                    if (CommonPrefs.darkThemeInUse(mActivity)) {
                        PrefsWrappers.prefs_appear_colors_dark::class.java
                    } else {
                        PrefsWrappers.prefs_appear_colors_light::class.java
                    }
                PrefsDelegate.launch(mActivity, clazz)
            }

            DlgDelegate.Action.ENABLE_NBS_DO -> {
                post { retryNBSInvites(params) }
                handled = super.onPosButton(action, *params)
            }

            else -> handled = super.onPosButton(action, *params)
        }
        cmd?.let { handleViaThread(it) }
        return handled
    }

    override fun onNegButton(action: DlgDelegate.Action, vararg params: Any): Boolean {
        Assert.assertVarargsNotNullNR(params)
        Log.d(TAG, "onNegButton(%s, %s)", action, DbgUtils.toStr(arrayOf(params)))
        var handled = true
        when (action) {
            DlgDelegate.Action.ENABLE_MQTT_DO_OR -> mDropMQTTOnDismiss = true
            DlgDelegate.Action.DROP_SMS_ACTION -> dropConViaAndRestart(CommsConnType.COMMS_CONN_SMS)
            DlgDelegate.Action.DELETE_AND_EXIT -> finish()
            DlgDelegate.Action.ASKED_PHONE_STATE -> showInviteChoicesThen()
            DlgDelegate.Action.CUSTOM_DICT_CONFIRMED -> {
                GamesListDelegate.launchGameConfig(mActivity, mRowid)
                finish()
            }

            DlgDelegate.Action.INVITE_SMS_DATA -> if (Perms23.haveNBSPerms(mActivity)) {
                val nMissing = params[0] as Int
                val info = params[1] as SentInvitesInfo
                launchPhoneNumberInvite(
                    nMissing, info,
                    RequestCode.SMS_DATA_INVITE_RESULT
                )
            }

            else -> handled = super.onNegButton(action, *params)
        }
        return handled
    }

    override fun onDismissed(action: DlgDelegate.Action, vararg params: Any): Boolean {
        Assert.assertVarargsNotNullNR(params)
        Log.d(TAG, "onDismissed(%s, %s)", action, DbgUtils.toStr(arrayOf(params)))
        var handled = true
        when (action) {
            DlgDelegate.Action.ENABLE_MQTT_DO_OR -> if (mDropMQTTOnDismiss) {
                postDelayed({ askDropMQTT() }, 10)
            }

            DlgDelegate.Action.DELETE_AND_EXIT -> finish()
            DlgDelegate.Action.BLANK_PICKED, DlgDelegate.Action.TRAY_PICKED ->             // If the user cancels the tile picker the common code doesn't
                // know, and won't put it up again as long as this game remains
                // loaded. There might be a way to fix that, but the safest thing
                // to do for now is to close. User will have to begin the process
                // of committing turn again on re-launching the game.
                finish()

            DlgDelegate.Action.DROP_SMS_ACTION -> alertOrderIncrIfAt(StartAlertOrder.NBS_PERMS)
            DlgDelegate.Action.LAUNCH_INVITE_ACTION -> showInviteAlertIf()
            else -> handled = super.onDismissed(action, *params)
        }
        return handled
    }

    override fun inviteChoiceMade(
        action: DlgDelegate.Action, means: InviteMeans,
        vararg params: Any
    ) {
        Assert.assertVarargsNotNullNR(params)
        if (action == DlgDelegate.Action.LAUNCH_INVITE_ACTION) {
            val info = if (0 < params.size
                && params[0] is SentInvitesInfo
            ) params[0] as SentInvitesInfo else null
            when (means) {
                InviteMeans.NFC -> if (!nfcAvail(mActivity)[1]) {
                    showDialogFragment(DlgID.ENABLE_NFC)
                } else {
                    makeOkOnlyBuilder(R.string.nfc_just_tap).show()
                }

                InviteMeans.BLUETOOTH -> BTInviteDelegate.launchForResult(
                    mActivity, m_mySIS!!.nMissing, info,
                    RequestCode.BT_INVITE_RESULT
                )

                InviteMeans.SMS_DATA -> Perms23.tryGetPerms(
                    this, Perms23.NBS_PERMS, R.string.sms_invite_rationale,
                    DlgDelegate.Action.INVITE_SMS_DATA, m_mySIS!!.nMissing, info
                )

                InviteMeans.MQTT -> showDialogFragment(DlgID.GET_DEVID)
                InviteMeans.RELAY ->                 // These have been removed as options
                    Assert.failDbg()

                InviteMeans.WIFIDIRECT -> WiDirInviteDelegate.launchForResult(
                    mActivity,
                    m_mySIS!!.nMissing, info,
                    RequestCode.P2P_INVITE_RESULT
                )

                InviteMeans.SMS_USER, InviteMeans.EMAIL, InviteMeans.CLIPBOARD -> {
                    val nli = NetLaunchInfo(
                        mActivity, mSummary, mGi,
                        1,  // nPlayers
                        1 + m_mySIS!!.nGuestDevs
                    ) // fc
                    when (means) {
                        InviteMeans.EMAIL -> GameUtils.launchEmailInviteActivity(mActivity, nli)
                        InviteMeans.SMS_USER -> GameUtils.launchSMSInviteActivity(mActivity, nli)
                        InviteMeans.CLIPBOARD -> GameUtils.inviteURLToClip(mActivity, nli)
                        else -> Log.d(TAG, "unexpected means $means")
                    }
                    recordInviteSent(means, null)
                }

                InviteMeans.QRCODE -> {}
                else -> Assert.failDbg()
            }
        }
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    override fun onClick(view: View) {
        if (view === mExchCommmitButton) {
            handleViaThread(JNICmd.CMD_COMMIT)
        } else if (view === mExchCancelButton) {
            handleViaThread(JNICmd.CMD_CANCELTRADE)
        }
    }

    //////////////////////////////////////////////////
    // MultiService.MultiEventListener interface
    //////////////////////////////////////////////////
    override fun eventOccurred(event: MultiEvent, vararg args: Any) {
        when (event) {
            MultiEvent.MESSAGE_ACCEPTED, MultiEvent.MESSAGE_REFUSED -> ConnStatusHandler.updateStatusIn(
                mActivity, this, CommsConnType.COMMS_CONN_BT,
                MultiEvent.MESSAGE_ACCEPTED == event
            )

            MultiEvent.MESSAGE_NOGAME -> {
                val gameID = args[0] as Int
                if (null != mGi && gameID == mGi!!.gameID && !isFinishing()) {
                    var expl: ConnExpl? = null
                    if (1 < args.size && args[1] is ConnExpl) {
                        expl = args[1] as ConnExpl
                    }
                    postDeleteOnce(expl)
                }
            }

            MultiEvent.BT_ENABLED -> pingBTRemotes()
            MultiEvent.NEWGAME_FAILURE -> Log.w(TAG, "failed to create game")
            MultiEvent.NEWGAME_DUP_REJECTED -> post {
                makeOkOnlyBuilder(
                    R.string.err_dup_invite_fmt,
                    args[0] as String
                )
                    .show()
            }

            MultiEvent.SMS_SEND_OK -> ConnStatusHandler.showSuccessOut(this)
            MultiEvent.SMS_RECEIVE_OK -> ConnStatusHandler.showSuccessIn(this)
            MultiEvent.SMS_SEND_FAILED, MultiEvent.SMS_SEND_FAILED_NORADIO, MultiEvent.SMS_SEND_FAILED_NOPERMISSION ->             // Don't bother warning if they're banned. Too frequent
                if (Perms23.haveNBSPerms(mActivity)) {
                    DbgUtils.showf(mActivity, R.string.sms_send_failed)
                }

            else -> super.eventOccurred(event, *args)
        }
    }

    //////////////////////////////////////////////////
    // TransportProcs.TPMsgHandler interface
    //////////////////////////////////////////////////
    override fun tpmCountChanged(newCount: Int, quashed: Boolean) {
        ConnStatusHandler.updateMoveCount(mActivity, newCount, quashed)
        if (quashed) {
            postDeleteOnce(null)
        }
        val goAlert = mGameOverAlert
        if (null != goAlert) {
            runOnUiThread { goAlert.pendingCountChanged(newCount) }
        }
    }

    //////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////
    override fun downloadFinished(
        isoCode: ISOCode, name: String,
        success: Boolean
    ) {
        if (success) {
            post { setGotGameDict(name) }
        }
    }

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
            if (!mConnTypes!!.contains(CommsConnType.COMMS_CONN_MQTT)) {
                popup.menu.removeItem(R.id.netstat_menu_traffic)
                popup.menu.removeItem(R.id.netstat_peers)
            }
            if (!mSummary!!.quashed) {
                popup.menu.removeItem(R.id.netstat_unquash)
            }
            popup.setOnMenuItemClickListener { item ->
                var handled = true
                when (item.itemId) {
                    R.id.netstat_menu_status -> onStatusClicked(mJniGamePtr)
                    R.id.netstat_menu_traffic -> NetUtils.copyAndLaunchGamePage(
                        mActivity,
                        mGi!!.gameID
                    )

                    R.id.netstat_copyurl -> NetUtils.gameURLToClip(mActivity, mGi!!.gameID)
                    R.id.netstat_peers -> showDialogFragment(DlgID.MQTT_PEERS)
                    R.id.netstat_unquash -> handleViaThread(JNICmd.CMD_UNQUASH)
                    else -> handled = false
                }
                handled
            }
            popup.show()
        } else {
            onStatusClicked(mJniGamePtr)
        }
    }

    override fun getHandler(): Handler {
        return mHandler!!
    }

    ////////////////////////////////////////////////////////////
    // NFCCardService.Wrapper.Procs
    ////////////////////////////////////////////////////////////
    override fun onReadingChange(nowReading: Boolean) {
        // Do we need this?
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

    override fun getRowID(): Long {
        return mRowid
    }

    private val invite: ByteArray?
        private get() {
            var result: ByteArray? = null
            if (0 < m_mySIS!!.nMissing // Isn't there a better test??
                && DeviceRole.SERVER_ISSERVER == mGi!!.serverRole
            ) {
                val nli = NetLaunchInfo(mGi)
                Assert.assertTrue(0 <= m_mySIS!!.nGuestDevs)
                nli.forceChannel = 1 + m_mySIS!!.nGuestDevs
                val iter: Iterator<CommsConnType> = mConnTypes!!.iterator()
                while (iter.hasNext()) {
                    val typ = iter.next()
                    when (typ) {
                        CommsConnType.COMMS_CONN_RELAY -> {
                            val room = mSummary!!.roomName
                            Assert.assertNotNull(room)
                            val inviteID = String.format("%X", mGi!!.gameID)
                            nli.addRelayInfo(room, inviteID)
                        }

                        CommsConnType.COMMS_CONN_BT -> nli.addBTInfo(mActivity)
                        CommsConnType.COMMS_CONN_SMS -> nli.addSMSInfo(mActivity)
                        CommsConnType.COMMS_CONN_P2P -> nli.addP2PInfo(mActivity)
                        CommsConnType.COMMS_CONN_NFC -> nli.addNFCInfo()
                        CommsConnType.COMMS_CONN_MQTT -> nli.addMQTTInfo()
                        else -> Log.w(
                            TAG, "Not doing NFC join for conn type %s",
                            typ.toString()
                        )
                    }
                }
                result = nli.asByteArray()
            }
            return result
        }

    private fun launchPhoneNumberInvite(
        nMissing: Int, info: SentInvitesInfo,
        code: RequestCode
    ) {
        SMSInviteDelegate.launchForResult(mActivity, nMissing, info, code)
    }

    private fun deleteAndClose() {
        if (null != mJniThread) { // this does still happen
            GameUtils.deleteGame(mActivity, mJniThread!!.getLock(), false, false)
        }
        waitCloseGame(false)
        finish()
    }

    private fun askDropMQTT() {
        var msg = getString(R.string.confirm_drop_mqtt)
        if (mConnTypes!!.contains(CommsConnType.COMMS_CONN_BT)) {
            msg += " " + getString(R.string.confirm_drop_relay_bt)
        }
        if (mConnTypes!!.contains(CommsConnType.COMMS_CONN_SMS)) {
            msg += " " + getString(R.string.confirm_drop_relay_sms)
        }
        makeConfirmThenBuilder(DlgDelegate.Action.DROP_MQTT_ACTION, msg).show()
    }

    private fun dropConViaAndRestart(typ: CommsConnType) {
        XwJNI.comms_dropHostAddr(mJniGamePtr, typ)
        finish()
        GameUtils.launchGame(delegator, mRowid)
    }

    private fun setGotGameDict(getDict: String?) {
        mJniThread!!.setSaveDict(getDict)
        val msg = getString(R.string.reload_new_dict_fmt, getDict)
        showToast(msg)
        finish()
        GameUtils.launchGame(delegator, mRowid)
    }

    private fun keyCodeToXPKey(keyCode: Int): XP_Key {
        var xpKey = XP_Key.XP_KEY_NONE
        when (keyCode) {
            KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> xpKey = XP_Key.XP_RETURN_KEY
            KeyEvent.KEYCODE_DPAD_DOWN -> xpKey = XP_Key.XP_CURSOR_KEY_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> xpKey = XP_Key.XP_CURSOR_KEY_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> xpKey = XP_Key.XP_CURSOR_KEY_RIGHT
            KeyEvent.KEYCODE_DPAD_UP -> xpKey = XP_Key.XP_CURSOR_KEY_UP
            KeyEvent.KEYCODE_SPACE -> xpKey = XP_Key.XP_RAISEFOCUS_KEY
        }
        return xpKey
    }

    private inner class BoardUtilCtxt : UtilCtxtImpl(mActivity) {
        override fun requestTime() {
            runOnUiThread {
                if (null != mJniThread) {
                    mJniThread!!.handleBkgrnd(JNICmd.CMD_DO)
                }
            }
        }

        override fun remSelected() {
            handleViaThread(JNICmd.CMD_REMAINING, R.string.tiles_left_title)
        }

        override fun timerSelected(inDuplicateMode: Boolean, canPause: Boolean) {
            if (inDuplicateMode) {
                runOnUiThread { getConfirmPause(canPause) }
            }
        }

        override fun bonusSquareHeld(bonus: Int) {
            var id = 0
            when (bonus) {
                UtilCtxt.BONUS_DOUBLE_LETTER -> id = R.string.bonus_l2x
                UtilCtxt.BONUS_DOUBLE_WORD -> id = R.string.bonus_w2x
                UtilCtxt.BONUS_TRIPLE_LETTER -> id = R.string.bonus_l3x
                UtilCtxt.BONUS_TRIPLE_WORD -> id = R.string.bonus_w3x
                UtilCtxt.BONUS_QUAD_LETTER -> id = R.string.bonus_l4x
                UtilCtxt.BONUS_QUAD_WORD -> id = R.string.bonus_w4x
                else -> Assert.failDbg()
            }
            if (0 != id) {
                val bonusStr = getString(id)
                post { showToast(bonusStr) }
            }
        }

        override fun informWordsBlocked(nWords: Int, words: String?, dict: String?) {
            runOnUiThread {
                val fmtd = TextUtils.join(", ", wordsToArray(words))
                makeOkOnlyBuilder(R.string.word_blocked_by_phony, fmtd, dict)
                    .show()
            }
        }

        override fun getInviteeName(plyrNum: Int): String? {
            return if (null == mSummary) null else mSummary!!.summarizePlayer(
                mActivity,
                mRowid,
                plyrNum
            )
        }

        override fun playerScoreHeld(player: Int) {
            val lmi = XwJNI.model_getPlayersLastScore(mJniGamePtr, player)
            var expl = lmi.format(mActivity)
            if (null == expl || 0 == expl.length) {
                expl = getString(R.string.no_moves_made)
            }
            val text = expl
            post { makeOkOnlyBuilder(text).show() }
        }

        override fun cellSquareHeld(words: String?) {
            post { launchLookup(wordsToArray(words), mGi!!.isoCode()) }
        }

        override fun setTimer(why: Int, `when`: Int, handle: Int) {
            if (null != mTimers[why]) {
                removeCallbacks(mTimers[why])
            }
            mTimers[why] = TimerRunnable(why, `when`, handle)
            val inHowLong: Int
            inHowLong = when (why) {
                UtilCtxt.TIMER_COMMS, UtilCtxt.TIMER_DUP_TIMERCHECK -> `when` * 1000
                UtilCtxt.TIMER_TIMERTICK -> 1000 // when is 0 for TIMER_TIMERTICK
                else -> 500
            }
            postDelayed(mTimers[why], inHowLong)
        }

        override fun clearTimer(why: Int) {
            val timer = mTimers.remove(why)
            if (null != timer) {
                removeCallbacks(timer)
            }
        }

        private fun startTP(
            action: DlgDelegate.Action,
            tps: TilePickState
        ) {
            runOnUiThread { show(TilePickAlert.newInstance(action, tps)) }
        }

        // This is supposed to be called from the jni thread
        override fun notifyPickTileBlank(
            playerNum: Int, col: Int, row: Int,
            texts: Array<String>
        ) {
            val tps = TilePickState(playerNum, texts, col, row)
            startTP(DlgDelegate.Action.BLANK_PICKED, tps)
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
            startTP(DlgDelegate.Action.TRAY_PICKED, tps)
        }

        override fun informNeedPassword(player: Int, name: String?) {
            showDialogFragment(DlgID.ASK_PASSWORD, player, name)
        }

        override fun turnChanged(newTurn: Int) {
            if (0 <= newTurn) {
                m_mySIS!!.nMissing = 0
                post {
                    makeNotAgainBuilder(
                        R.string.key_notagain_turnchanged,
                        R.string.not_again_turnchanged
                    )
                        .show()
                }
                handleViaThread(JNICmd.CMD_ZOOM, -8)
                handleViaThread(JNICmd.CMD_SAVE)
            }
        }

        override fun engineProgressCallback(): Boolean {
            // return true if engine should keep going
            val jnit = mJniThread
            return jnit != null && !jnit.busy()
        }

        override fun notifyMove(msg: String?) {
            showDialogFragment(DlgID.QUERY_MOVE, msg)
        }

        override fun notifyTrade(tiles: Array<String?>?) {
            val dlgBytes = getQuantityString(
                R.plurals.query_trade_fmt, tiles!!.size,
                tiles.size, TextUtils.join(", ", tiles)
            )
            showDialogFragment(DlgID.QUERY_TRADE, dlgBytes)
        }

        override fun notifyDupStatus(amHost: Boolean, msg: String?) {
            val key =
                if (amHost) R.string.key_na_dupstatus_host else R.string.key_na_dupstatus_guest
            runOnUiThread {
                makeNotAgainBuilder(key, msg)
                    .show()
            }
        }

        override fun userError(code: Int) {
            var resid = 0
            var asToast = false
            var msg: String? = null
            when (code) {
                UtilCtxt.ERR_TILES_NOT_IN_LINE -> resid = R.string.str_tiles_not_in_line
                UtilCtxt.ERR_NO_EMPTIES_IN_TURN -> resid = R.string.str_no_empties_in_turn
                UtilCtxt.ERR_TWO_TILES_FIRST_MOVE -> resid = R.string.str_two_tiles_first_move
                UtilCtxt.ERR_TILES_MUST_CONTACT -> resid = R.string.str_tiles_must_contact
                UtilCtxt.ERR_NOT_YOUR_TURN -> resid = R.string.str_not_your_turn
                UtilCtxt.ERR_NO_PEEK_ROBOT_TILES -> resid = R.string.str_no_peek_robot_tiles
                UtilCtxt.ERR_NO_EMPTY_TRADE ->                 // This should not be possible as the button's
                    // disabled when no tiles selected.
                    Assert.failDbg()

                UtilCtxt.ERR_TOO_MANY_TRADE -> {
                    val nLeft = XwJNI.server_countTilesInPool(mJniGamePtr)
                    msg = getQuantityString(
                        R.plurals.too_many_trade_fmt,
                        nLeft, nLeft
                    )
                }

                UtilCtxt.ERR_TOO_FEW_TILES_LEFT_TO_TRADE -> resid =
                    R.string.str_too_few_tiles_left_to_trade

                UtilCtxt.ERR_CANT_UNDO_TILEASSIGN -> resid = R.string.str_cant_undo_tileassign
                UtilCtxt.ERR_CANT_HINT_WHILE_DISABLED -> resid =
                    R.string.str_cant_hint_while_disabled

                UtilCtxt.ERR_NO_PEEK_REMOTE_TILES -> resid = R.string.str_no_peek_remote_tiles
                UtilCtxt.ERR_REG_UNEXPECTED_USER -> resid = R.string.str_reg_unexpected_user
                UtilCtxt.ERR_SERVER_DICT_WINS -> resid = R.string.str_server_dict_wins
                UtilCtxt.ERR_REG_SERVER_SANS_REMOTE -> resid = R.string.str_reg_server_sans_remote
                UtilCtxt.ERR_NO_HINT_FOUND -> {
                    resid = R.string.str_no_hint_found
                    asToast = true
                }
            }
            if (null == msg && resid != 0) {
                msg = getString(resid)
            }
            if (null != msg) {
                if (asToast) {
                    val msgf: String = msg
                    runOnUiThread { showToast(msgf) }
                } else {
                    nonBlockingDialog(DlgID.DLG_OKONLY, msg)
                }
            }
        } // userError

        // Called from server_makeFromStream, whether there's something
        // missing or not.
        override fun informMissing(
            isServer: Boolean, hostAddr: CommsAddrRec?,
            connTypes: CommsConnTypeSet?, nDevs: Int,
            nMissing: Int, nInvited: Int, fromRematch: Boolean
        ) {
            // Log.d( TAG, "informMissing(isServer: %b, nDevs: %d; nMissing: %d, "
            //        + " nInvited: %d", isServer, nDevs, nMissing, nInvited );
            Assert.assertTrueNR(nInvited <= nMissing)
            m_mySIS!!.nMissing = nMissing // will be 0 unless isServer is true
            m_mySIS!!.nInvited = nInvited
            m_mySIS!!.nGuestDevs = nDevs
            m_mySIS!!.fromRematch = fromRematch
            m_mySIS!!.hostAddr = hostAddr
            mConnTypes = connTypes
            runOnUiThread { showInviteAlertIf() }
        }

        override fun informMove(turn: Int, expl: String?, words: String?) {
            m_mySIS!!.words = words?.let { wordsToArray(it) }
            nonBlockingDialog(DlgID.DLG_SCORES, expl)

            // Post a notification if in background, or play sound if not. But
            // do nothing for standalone case.
            if (DeviceRole.SERVER_STANDALONE == mGi!!.serverRole) {
                // do nothing
            } else if (isVisible) {
                Utils.playNotificationSound(mActivity)
            } else {
                val bmr = BackMoveResult()
                bmr.m_lmi = XwJNI.model_getPlayersLastScore(mJniGamePtr, turn)
                val locals = mGi!!.playersLocal()
                GameUtils.postMoveNotification(
                    mActivity, mRowid,
                    bmr, locals[turn]
                )
            }
        }

        override fun informUndo() {
            nonBlockingDialog(
                DlgID.DLG_OKONLY,
                getString(R.string.remote_undone)
            )
        }

        override fun informNetDict(
            isoCodeStr: String?, oldName: String?,
            newName: String?, newSum: String?,
            phonies: XWPhoniesChoice?
        ) {
            // If it's same dict and same sum, we're good.  That
            // should be the normal case.  Otherwise: if same name but
            // different sum, notify and offer to upgrade.  If
            // different name, offer to install.
            var msg: String? = null
            if (oldName == newName) {
                val oldSum = DictLangCache
                    .getDictMD5Sums(mActivity, oldName)[0]
                if (oldSum != newSum) {
                    // Same dict, different versions
                    msg = getString(
                        R.string.inform_dict_diffversion_fmt,
                        oldName
                    )
                }
            } else {
                // Different dict!  If we have the other one, switch
                // to it.  Otherwise offer to download
                val dlgID: DlgID
                msg = getString(
                    R.string.inform_dict_diffdict_fmt,
                    oldName, newName, newName
                )
                val isoCode = ISOCode.newIf(isoCodeStr)
                if (DictLangCache.haveDict(
                        mActivity, isoCode,
                        newName
                    )
                ) {
                    dlgID = DlgID.DLG_USEDICT
                } else {
                    dlgID = DlgID.DLG_GETDICT
                    msg += getString(R.string.inform_dict_download)
                }
                m_mySIS!!.getDict = newName
                nonBlockingDialog(dlgID, msg)
            }
        }

        override fun notifyGameOver() {
            mGameOver = true
            handleViaThread(JNICmd.CMD_POST_OVER)
        }

        override fun notifyIllegalWords(
            dict: String?, words: Array<String?>?, turn: Int,
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

        // Let's have this block in case there are multiple messages.  If
        // we don't block the jni thread will continue processing messages
        // and may stack dialogs on top of this one.  Including later
        // chat-messages.
        override fun showChat(
            msg: String?, fromIndx: Int,
            tsSeconds: Int
        ) {
            runOnUiThread {
                DBUtils.appendChatHistory(
                    mActivity, mRowid, msg!!,
                    fromIndx, tsSeconds.toLong()
                )
                if (!ChatDelegate.append(
                        mRowid, msg,
                        fromIndx, tsSeconds.toLong()
                    )
                ) {
                    startChatActivity()
                }
            }
        }

        override fun formatPauseHistory(
            pauseTyp: Int, player: Int,
            whenPrev: Int, whenCur: Int, msg: String?
        ): String? {
            Log.d(TAG, "formatPauseHistory(prev: %d, cur: %d)", whenPrev, whenCur)
            var result: String? = null
            val name = if (0 > player) null else mGi!!.players[player].name
            when (pauseTyp) {
                DUtilCtxt.UNPAUSED -> {
                    val interval = DateUtils
                        .formatElapsedTime((whenCur - whenPrev).toLong())
                        .toString()
                    result = LocUtils.getString(
                        mActivity, R.string.history_unpause_fmt,
                        name, interval
                    )
                }

                DUtilCtxt.PAUSED -> result = LocUtils.getString(
                    mActivity, R.string.history_pause_fmt,
                    name
                )

                DUtilCtxt.AUTOPAUSED -> result =
                    LocUtils.getString(mActivity, R.string.history_autopause)
            }
            if (null != msg) {
                result += " " + LocUtils
                    .getString(mActivity, R.string.history_msg_fmt, msg)
            }
            return result
        }

        override fun getRowID(): Long {
            return mRowid
        }
    } // class BoardUtilCtxt

    private fun doResume(isStart: Boolean) {
        var success = null != mJniThreadRef
        val firstStart = null == mHandler
        if (success && firstStart) {
            mHandler = Handler()
            success = mJniThreadRef!!.configure(
                mActivity, mView, mUtils, this,
                makeJNIHandler()
            )
            if (success) {
                mJniGamePtr = mJniThreadRef!!.gamePtr // .retain()?
                Assert.assertNotNull(mJniGamePtr)
            }
        }
        if (success) {
            try {
                resumeGame(isStart)
                if (!isStart) {
                    setKeepScreenOn()
                    ConnStatusHandler.setHandler(this)
                }
            } catch (ex: NoSuchGameException) {
                Log.ex(TAG, ex)
                success = false
            } catch (ex: NullPointerException) {
                Log.ex(TAG, ex)
                success = false
            }
        }
        if (!success) {
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

    private fun makeJNIHandler(): Handler {
        return object : Handler() {
            override fun handleMessage(msg: Message) {
                when (msg.what) {
                    JNIThread.DIALOG -> showDialogFragment(
                        DlgID.DLG_OKONLY, msg.arg1,
                        msg.obj as String
                    )

                    JNIThread.QUERY_ENDGAME -> showDialogFragment(DlgID.QUERY_ENDGAME)
                    JNIThread.TOOLBAR_STATES -> if (null != mJniThread) {
                        mGsi = mJniThread!!.gameStateInfo
                        updateToolbar()
                        if (m_mySIS!!.inTrade != mGsi!!.inTrade) {
                            m_mySIS!!.inTrade = mGsi!!.inTrade
                        }
                        mView!!.setInTrade(m_mySIS!!.inTrade)
                        showTradeToastOnce(m_mySIS!!.inTrade)
                        adjustTradeVisibility()
                        invalidateOptionsMenuIf()
                    }

                    JNIThread.GAME_OVER -> if (mIsFirstLaunch) {
                        handleGameOver(msg.arg1, msg.obj as String)
                    }

                    JNIThread.MSGS_SENT -> {
                        val nSent = msg.obj as Int
                        showToast(
                            getQuantityString(
                                R.plurals.resent_msgs_fmt,
                                nSent, nSent
                            )
                        )
                    }

                    JNIThread.GOT_PAUSE -> runOnUiThread {
                        makeOkOnlyBuilder(msg.obj as String)
                            .show()
                    }
                }
            }
        }
    }

    private fun handleGameOver(titleID: Int, msg: String) {
        val onDone: OnDoneProc = object : OnDoneProc {
            override fun onGameOverDone(
                rematch: Boolean,
                archiveAfter: Boolean,
                deleteAfter: Boolean
            ) {
                var postAction: DlgDelegate.Action? = null
                val postArgs = ArrayList<Any>()
                if (rematch) {
                    postAction = DlgDelegate.Action.REMATCH_ACTION
                    postArgs.add(archiveAfter)
                    postArgs.add(deleteAfter)
                } else if (archiveAfter) {
                    showArchiveNA(false)
                } else if (deleteAfter) {
                    postAction = DlgDelegate.Action.DELETE_ACTION
                }
                if (null != postAction) {
                    post { onPosButton(postAction, *postArgs.toTypedArray()) }
                }
            }
        }
        runOnUiThread {
            if (mJniGamePtr!!.isRetained) {
                val hasPending = 0 < XwJNI.comms_countPendingPackets(mJniGamePtr)
                mGameOverAlert = GameOverAlert.newInstance(
                    mSummary, titleID, msg,
                    hasPending, inArchiveGroup()
                )
                    .configure(onDone, this@BoardDelegate)
                show(mGameOverAlert)
            } else {
                Log.e(TAG, "gamePtr not retained")
            }
        }
    }

    private fun resumeGame(isStart: Boolean) {
        if (null == mJniThread) {
            mJniThread = mJniThreadRef!!.retain()
            mGi = mJniThread!!.getGI() // this can be null, per Play Store report
            mSummary = mJniThread!!.getSummary()
            Wrapper.setGameID(mNFCWrapper, mGi!!.gameID)
            val invite = invite
            if (null != invite) {
                addInvitationFor(invite, mGi!!.gameID)
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
                                        DlgDelegate.Action.LAUNCH_THEME_COLOR_CONFIG,
                                        R.string.menu_prefs
                                    )
                                    .show()
                            }
                        }
                    }
                }
            }
            mView!!.startHandling(mActivity, mJniThread!!, mConnTypes, proc)
            handleViaThread(JNICmd.CMD_START)
            if (!CommonPrefs.getHideTitleBar(mActivity)) {
                setTitle()
            }
            initToolbar()
            adjustTradeVisibility()
            val flags = DBUtils.getMsgFlags(mActivity, mRowid)
            if (0 != GameSummary.MSG_FLAGS_CHAT and flags) {
                post { startChatActivity() }
            }
            if (mOverNotShown) {
                var auto = false
                if (0 != GameSummary.MSG_FLAGS_GAMEOVER and flags) {
                    mGameOver = true
                } else if (DBUtils.gameOver(mActivity, mRowid)) {
                    mGameOver = true
                    auto = true
                }
                if (mGameOver) {
                    mOverNotShown = false
                    handleViaThread(JNICmd.CMD_POST_OVER, auto)
                }
            }
            if (0 != flags) {
                DBUtils.setMsgFlags(
                    mActivity, mRowid,
                    GameSummary.MSG_FLAGS_NONE
                )
            }
            Utils.cancelNotification(mActivity, mRowid)
            askNBSPermissions()
            if (mGi!!.serverRole != DeviceRole.SERVER_STANDALONE) {
                warnIfNoTransport()
                tickle(isStart)
                tryInvites()
            }
            val args = arguments
            if (args.getBoolean(PAUSER_KEY, false)) {
                getConfirmPause(true)
            }
        }
    } // resumeGame

    private fun askNBSPermissions() {
        val thisOrder = StartAlertOrder.NBS_PERMS
        if (alertOrderAt(thisOrder) // already asked?
            && mSummary!!.conTypes.contains(CommsConnType.COMMS_CONN_SMS)
        ) {
            if (Perms23.haveNBSPerms(mActivity)) {
                // We have them or a workaround; cool! proceed
                alertOrderIncrIfAt(thisOrder)
            } else {
                mPermCbck = PermCbck { allGood ->
                    if (allGood) {
                        // Yay! nothing to do
                        alertOrderIncrIfAt(thisOrder)
                    } else {
                        val explID =
                            if (Perms23.NBSPermsInManifest(mActivity)) R.string.missing_sms_perms else R.string.variant_missing_nbs
                        makeConfirmThenBuilder(DlgDelegate.Action.DROP_SMS_ACTION, explID)
                            .setNegButton(R.string.remove_sms)
                            .show()
                    }
                }
                Perms23.Builder(*Perms23.NBS_PERMS)
                    .asyncQuery(mActivity, mPermCbck)
            }
        } else {
            alertOrderIncrIfAt(thisOrder)
        }
    }

    private fun tickle(force: Boolean) {
        val iter: Iterator<CommsConnType> = mConnTypes!!.iterator()
        while (iter.hasNext()) {
            val typ = iter.next()
            when (typ) {
                CommsConnType.COMMS_CONN_BT -> pingBTRemotes()
                CommsConnType.COMMS_CONN_RELAY, CommsConnType.COMMS_CONN_SMS, CommsConnType.COMMS_CONN_P2P, CommsConnType.COMMS_CONN_NFC, CommsConnType.COMMS_CONN_MQTT -> {}
                else -> {
                    Log.w(TAG, "tickle: unexpected type %s", typ.toString())
                    Assert.failDbg()
                }
            }
        }
        if (0 < mConnTypes!!.size) {
            handleViaThread(JNICmd.CMD_RESEND, force, true, false)
        }
    }

    private fun pingBTRemotes() {
        if (null != mConnTypes && mConnTypes!!.contains(CommsConnType.COMMS_CONN_BT)
            && !XWPrefs.getBTDisabled(mActivity)
            && XwJNI.server_getGameIsConnected(mJniGamePtr)
        ) {
            val addrs = XwJNI.comms_getAddrs(mJniGamePtr)
            for (addr in addrs!!) {
                if (addr!!.contains(CommsConnType.COMMS_CONN_BT)
                    && !TextUtils.isEmpty(addr.bt_btAddr)
                ) {
                    BTUtils.pingHost(mActivity, addr.bt_hostName, addr.bt_btAddr, mGi!!.gameID)
                }
            }
        }
    }

    private fun populateToolbar() {
        Assert.assertTrue(null != mToolbar || !BuildConfig.DEBUG)
        if (null != mToolbar) {
            mToolbar!!.setListener(
                Buttons.BUTTON_BROWSE_DICT,
                R.string.not_again_browseall,
                R.string.key_na_browseall,
                DlgDelegate.Action.BUTTON_BROWSEALL_ACTION
            )
                .setLongClickListener(
                    Buttons.BUTTON_BROWSE_DICT,
                    R.string.not_again_browse,
                    R.string.key_na_browse,
                    DlgDelegate.Action.BUTTON_BROWSE_ACTION
                )
                .setListener(
                    Buttons.BUTTON_HINT_PREV,
                    R.string.not_again_hintprev,
                    R.string.key_notagain_hintprev,
                    DlgDelegate.Action.PREV_HINT_ACTION
                )
                .setListener(
                    Buttons.BUTTON_HINT_NEXT,
                    R.string.not_again_hintnext,
                    R.string.key_notagain_hintnext,
                    DlgDelegate.Action.NEXT_HINT_ACTION
                )
                .setListener(
                    Buttons.BUTTON_JUGGLE,
                    R.string.not_again_juggle,
                    R.string.key_notagain_juggle,
                    DlgDelegate.Action.JUGGLE_ACTION
                )
                .setListener(
                    Buttons.BUTTON_FLIP,
                    R.string.not_again_flip,
                    R.string.key_notagain_flip,
                    DlgDelegate.Action.FLIP_ACTION
                )
                .setListener(
                    Buttons.BUTTON_VALUES,
                    R.string.not_again_values,
                    R.string.key_na_values,
                    DlgDelegate.Action.VALUES_ACTION
                )
                .setListener(
                    Buttons.BUTTON_UNDO,
                    R.string.not_again_undo,
                    R.string.key_notagain_undo,
                    DlgDelegate.Action.UNDO_ACTION
                )
                .setListener(
                    Buttons.BUTTON_CHAT,
                    R.string.not_again_chat,
                    R.string.key_notagain_chat,
                    DlgDelegate.Action.CHAT_ACTION
                )
                .installListeners()
        } else {
            Log.e(TAG, "not initing toolbar; still null")
        }
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
        wrapper.showOrHide(
            m_mySIS!!.hostAddr, m_mySIS!!.nMissing,
            m_mySIS!!.nInvited, m_mySIS!!.fromRematch
        )
    }

    private var mINAWrapper: InvitesNeededAlert.Wrapper? = null
    private val iNAWrapper: InvitesNeededAlert.Wrapper
        private get() {
            if (null == mINAWrapper) {
                mINAWrapper = InvitesNeededAlert.Wrapper(this, mJniGamePtr)
                showOrHide(mINAWrapper!!)
            }
            return mINAWrapper!!
        }

    private fun doZoom(zoomBy: Int): Boolean {
        val handled = null != mJniThread
        if (handled) {
            handleViaThread(JNICmd.CMD_ZOOM, zoomBy)
        }
        return handled
    }

    private fun startChatActivity() {
        val curPlayer = XwJNI.board_getLikelyChatter(mJniGamePtr)
        val names = mGi!!.playerNames()
        val locs = mGi!!.playersLocal() // to convert old histories
        ChatDelegate.start(
            delegator, mRowid, curPlayer,
            names, locs
        )
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
            handleViaThread(JNICmd.CMD_PREFS_CHANGE)
            true
        }
        popup.show()
    }

    private fun getConfirmPause(isPause: Boolean) {
        showDialogFragment(DlgID.ASK_DUP_PAUSE, isPause)
    }

    private fun closeIfFinishing(force: Boolean) {
        if (null == mHandler) {
            // DbgUtils.logf( "closeIfFinishing(): already closed" );
        } else if (force || isFinishing()) {
            // DbgUtils.logf( "closeIfFinishing: closing rowid %d", m_rowid );
            mHandler = null
            ConnStatusHandler.setHandler(null)
            waitCloseGame(true)
        } else {
            handleViaThread(JNICmd.CMD_SAVE)
            // DbgUtils.logf( "closeIfFinishing(): not finishing (yet)" );
        }
    }

    private fun pauseGame() {
        if (null != mJniThread) {
            mJniThread!!.release()
            mJniThread = null
            mView!!.stopHandling()
        }
    }

    private fun waitCloseGame(save: Boolean) {
        pauseGame()
        if (null != mJniThread) {
            // m_jniGamePtr.release();
            // m_jniGamePtr = null;

            // m_gameLock.unlock(); // likely the problem
        }
    }

    private fun warnIfNoTransport() {
        if (null != mConnTypes && alertOrderAt(StartAlertOrder.NO_MEANS)) {
            if (mConnTypes!!.contains(CommsConnType.COMMS_CONN_SMS)) {
                if (!XWPrefs.getNBSEnabled(mActivity)) {
                    makeConfirmThenBuilder(
                        DlgDelegate.Action.ENABLE_NBS_ASK,
                        R.string.warn_sms_disabled
                    )
                        .setPosButton(R.string.button_enable_sms)
                        .setNegButton(R.string.button_later)
                        .show()
                }
            }
            if (mConnTypes!!.contains(CommsConnType.COMMS_CONN_RELAY)) {
                Log.e(TAG, "opened game with RELAY still")
            }
            if (mConnTypes!!.contains(CommsConnType.COMMS_CONN_MQTT)) {
                if (!XWPrefs.getMQTTEnabled(mActivity)) {
                    mDropMQTTOnDismiss = false
                    val msg = """
                        ${getString(R.string.warn_mqtt_disabled)}
                        
                        ${getString(R.string.warn_mqtt_remove)}
                        """.trimIndent()
                    makeConfirmThenBuilder(DlgDelegate.Action.ENABLE_MQTT_DO_OR, msg)
                        .setPosButton(R.string.button_enable_mqtt)
                        .setNegButton(R.string.newgame_drop_mqtt)
                        .show()
                }
            }
            if (mConnTypes!!.isEmpty()) {
                askNoAddrsDelete()
            } else {
                alertOrderIncrIfAt(StartAlertOrder.NO_MEANS)
            }
        }
    }

    private fun tryInvites() {
        if (null != mMissingDevs) {
            Assert.assertNotNull(mMissingMeans)
            val gameName = GameUtils.getName(mActivity, mRowid)
            for (ii in mMissingDevs!!.indices) {
                val dev = mMissingDevs!![ii]
                val nPlayers = mMissingCounts!![ii]
                Assert.assertTrue(0 <= m_mySIS!!.nGuestDevs)
                val forceChannel = ii + m_mySIS!!.nGuestDevs + 1
                val nli = NetLaunchInfo(
                    mActivity, mSummary, mGi,
                    nPlayers, forceChannel
                )
                    .setRemotesAreRobots(mRemotesAreRobots)
                var destAddr: CommsAddrRec? = null
                when (mMissingMeans) {
                    InviteMeans.BLUETOOTH -> destAddr = CommsAddrRec(CommsConnType.COMMS_CONN_BT)
                        .setBTParams(null, dev)

                    InviteMeans.SMS_DATA -> destAddr = CommsAddrRec(CommsConnType.COMMS_CONN_SMS)
                        .setSMSParams(dev)

                    InviteMeans.WIFIDIRECT -> WiDirService.inviteRemote(mActivity, dev, nli)
                    InviteMeans.MQTT -> destAddr = CommsAddrRec(CommsConnType.COMMS_CONN_MQTT)
                        .setMQTTParams(mMissingDevs!![ii])

                    InviteMeans.RELAY -> Assert.failDbg() // not getting here, right?
                    else -> Assert.failDbg()
                }
                if (null != destAddr) {
                    XwJNI.comms_invite(mJniGamePtr, nli, destAddr, true)
                } else if (null != dev) {
                    recordInviteSent(mMissingMeans, dev)
                }
            }
            mMissingDevs = null
            mMissingCounts = null
            mMissingMeans = null
        }
    }

    private var m_needsResize = false
    private fun updateToolbar() {
        if (null != mToolbar) {
            mToolbar!!.update(Buttons.BUTTON_FLIP, mGsi!!.visTileCount >= 1)
                .update(Buttons.BUTTON_VALUES, mGsi!!.visTileCount >= 1)
                .update(Buttons.BUTTON_JUGGLE, mGsi!!.canShuffle)
                .update(Buttons.BUTTON_UNDO, mGsi!!.canRedo)
                .update(Buttons.BUTTON_HINT_PREV, mGsi!!.canHint)
                .update(Buttons.BUTTON_HINT_NEXT, mGsi!!.canHint)
                .update(Buttons.BUTTON_CHAT, mGsi!!.canChat)
                .update(
                    Buttons.BUTTON_BROWSE_DICT,
                    null != mGi!!.dictName(mView!!.curPlayer)
                )
            val count = mToolbar!!.enabledCount()
            if (0 == count) {
                m_needsResize = true
            } else if (m_needsResize && 0 < count) {
                m_needsResize = false
                mView!!.orientationChanged()
            }
        }
    }

    private fun adjustTradeVisibility() {
        if (null != mToolbar) {
            mToolbar!!.setVisible(!m_mySIS!!.inTrade)
        }
        if (null != mTradeButtons) {
            mTradeButtons.visibility =
                if (m_mySIS!!.inTrade) View.VISIBLE else View.GONE
        }
        if (m_mySIS!!.inTrade && null != mExchCommmitButton) {
            mExchCommmitButton.setEnabled(mGsi!!.tradeTilesSelected)
        }
    }

    private fun setBackgroundColor() {
        val view = findViewById(R.id.board_root)
        // Google's reported an NPE here, so test
        if (null != view) {
            val back = CommonPrefs.get(mActivity).otherColors[CommonPrefs.COLOR_BACKGRND]
            view.setBackgroundColor(back)
        }
    }

    private fun setKeepScreenOn() {
        val keepOn = CommonPrefs.getKeepScreenOn(mActivity)
        mView!!.keepScreenOn = keepOn
        if (keepOn) {
            if (null == mScreenTimer) {
                mScreenTimer = Runnable {
                    if (null != mView) {
                        mView!!.keepScreenOn = false
                    }
                }
            }
            removeCallbacks(mScreenTimer) // needed?
            postDelayed(mScreenTimer, SCREEN_ON_TIME)
        }
    }

    override fun post(runnable: Runnable): Boolean {
        val canPost = null != mHandler
        if (canPost) {
            mHandler!!.post(runnable)
        } else {
            Log.w(TAG, "post(): dropping b/c handler null")
            DbgUtils.printStack(TAG)
        }
        return canPost
    }

    private fun postDelayed(runnable: Runnable?, `when`: Int) {
        if (null != mHandler) {
            mHandler!!.postDelayed(runnable!!, `when`.toLong())
        } else {
            Log.w(TAG, "postDelayed: dropping %d because handler null", `when`)
        }
    }

    private fun removeCallbacks(which: Runnable?) {
        if (null != mHandler) {
            mHandler!!.removeCallbacks(which!!)
        } else {
            Log.w(
                TAG, "removeCallbacks: dropping %h because handler null",
                which
            )
        }
    }

    private fun wordsToArray(words: String?): Array<String> {
        val tmp = TextUtils.split(words, "\n")
        val list: MutableList<String> = ArrayList()
        for (one in tmp) {
            if (0 < one.length) {
                list.add(one)
            }
        }
        return list.toTypedArray<String>()
    }

    private fun inArchiveGroup(): Boolean {
        val archiveGroup = DBUtils.getArchiveGroup(mActivity)
        val curGroup = DBUtils.getGroupForGame(mActivity, mRowid)
        return curGroup == archiveGroup
    }

    private fun showArchiveNA(rematchAfter: Boolean) {
        makeNotAgainBuilder(
            R.string.key_na_archive, DlgDelegate.Action.ARCHIVE_ACTION,
            R.string.not_again_archive
        )
            .setParams(rematchAfter)
            .show()
    }

    private fun archiveGame(closeAfter: Boolean) {
        val gid = DBUtils.getArchiveGroup(mActivity)
        DBUtils.moveGame(mActivity, mRowid, gid)
        if (closeAfter) {
            waitCloseGame(false)
            finish()
        }
    }

    private fun doRematchIf(deleteAfter: Boolean) {
        doRematchIf(DBUtils.GROUPID_UNSPEC, deleteAfter)
    }

    private fun doRematchIf(groupID: Long, deleteAfter: Boolean) {
        doRematchIf(
            mActivity, this, mRowid, groupID, mSummary,
            mGi, mJniGamePtr, deleteAfter
        )
    }

    init {
        mActivity = delegator.getActivity()
    }

    private fun nliForMe(): NetLaunchInfo {
        val numHere = 1
        // This is too simple. Need to know if it's a replacement
        val forceChannel = 1 + m_mySIS!!.nGuestDevs
        // Log.d( TAG, "nliForMe() => %s", nli );
        return NetLaunchInfo(
            mActivity, mSummary, mGi,
            numHere, forceChannel
        )
    }

    private fun tryOtherInvites(addr: CommsAddrRec): Boolean {
        Log.d(TAG, "tryOtherInvites(%s)", addr)
        XwJNI.comms_invite(mJniGamePtr, nliForMe(), addr, true)

        // Not sure what to do about this recordInviteSent stuff
        val conTypes = addr.conTypes
        for (typ in conTypes!!) {
            when (typ) {
                CommsConnType.COMMS_CONN_MQTT -> {}
                CommsConnType.COMMS_CONN_BT -> {}
                CommsConnType.COMMS_CONN_SMS -> {}
                CommsConnType.COMMS_CONN_NFC -> {}
                else -> {
                    Log.d(TAG, "not inviting using addr type %s", typ)
                    Assert.failDbg()
                }
            }
        }
        return true
    }

    private fun sendNBSInviteIf(phone: String, nli: NetLaunchInfo, askOk: Boolean) {
        if (XWPrefs.getNBSEnabled(mActivity)) {
            NBSProto.inviteRemote(mActivity, phone, nli)
            recordInviteSent(InviteMeans.SMS_DATA, phone)
        } else if (askOk) {
            makeConfirmThenBuilder(
                DlgDelegate.Action.ENABLE_NBS_ASK,
                R.string.warn_sms_disabled
            )
                .setPosButton(R.string.button_enable_sms)
                .setNegButton(R.string.button_later)
                .setParams(nli, phone)
                .show()
        }
    }

    private fun retryNBSInvites(params: Array<out Any>) {
        Assert.assertVarargsNotNullNR(params)
        if (2 == params.size && params[0] is NetLaunchInfo
            && params[1] is String
        ) {
            sendNBSInviteIf(
                params[1] as String, params[0] as NetLaunchInfo,
                false
            )
        } else {
            Log.w(TAG, "retryNBSInvites: tests failed")
        }
    }

    private fun recordInviteSent(means: InviteMeans?, dev: String?) {
        var invitesSent = true
        if (!mShowedReInvite) { // have we sent since?
            val sentInfo = DBUtils.getInvitesFor(mActivity, mRowid)
            val nSent = sentInfo.minPlayerCount
            invitesSent = nSent >= m_mySIS!!.nMissing
        }
        DBUtils.recordInviteSent(mActivity, mRowid, means!!, dev, false)
        if (!invitesSent) {
            Log.d(TAG, "recordInviteSent(): redoing invite alert")
            showInviteAlertIf()
        }
    }

    private fun handleViaThread(cmd: JNICmd, vararg args: Any) {
        Assert.assertVarargsNotNullNR(args)
        if (null == mJniThread) {
            Log.w(
                TAG, "m_jniThread null: not calling m_jniThread.handle(%s)",
                cmd
            )
            DbgUtils.printStack(TAG)
        } else {
            mJniThread!!.handle(cmd, *args)
        }
    }

    companion object {
        private val TAG = BoardDelegate::class.java.getSimpleName()
        private const val SCREEN_ON_TIME = 10 * 60 * 1000 // 10 mins
        private val SAVE_MYSIS = TAG + "/MYSIS"
        @JvmField
        val PAUSER_KEY = TAG + "/pauser"
        private var s_noLockCount = 0 // supports a quick debugging hack
        private var s_themeNAShown = false
        private const val mCounter = 0
        private fun doRematchIf(
            activity: Activity, dlgt: DelegateBase?,
            rowid: Long, groupID: Long,
            summary: GameSummary?, gi: CurGameInfo?,
            jniGamePtr: GamePtr?, deleteAfter: Boolean
        ) {
            val intent = GamesListDelegate
                .makeRematchIntent(
                    activity, rowid, groupID, gi,
                    summary!!.conTypes, deleteAfter
                )
            if (null != intent) {
                activity.startActivity(intent)
            }
        }

        @JvmStatic
        fun setupRematchFor(activity: Activity, rowID: Long) {
            var summary: GameSummary? = null
            var gi: CurGameInfo? = null
            JNIThread.getRetained(rowID).use { thread ->
                if (null != thread) {
                    thread.gamePtr.retain().use { gamePtr ->
                        summary = thread.summary
                        gi = thread.gi
                        setupRematchFor(activity, gamePtr, summary, gi)
                    }
                } else {
                    GameUtils.GameWrapper.make(activity, rowID).use { gw ->
                        if (null == gw) {
                            DbgUtils.toastNoLock(
                                TAG, activity, rowID,
                                "setupRematchFor(%d)", rowID
                            )
                        } else {
                            summary = DBUtils.getSummary(activity, gw.lock)
                            setupRematchFor(activity, gw.gamePtr(), summary, gw.gi())
                        }
                    }
                }
            }
        }

        // This might need to map rowid->openCount so opens can stack
        var sOpenRows: MutableSet<Long> = HashSet()
        private fun noteOpened(context: Context, rowid: Long) {
            Log.d(TAG, "noteOpened(%d)", rowid)
            if (BuildConfig.NON_RELEASE && sOpenRows.contains(rowid)) {
                val msg = String.format("noteOpened(%d): already open", rowid)
                Utils.showToast(context, msg)
                DbgUtils.printStack(TAG)
            } else {
                sOpenRows.add(rowid)
            }
        }

        private fun noteClosed(rowid: Long) {
            Log.d(TAG, "noteClosed(%d)", rowid)
            Assert.assertTrueNR(sOpenRows.contains(rowid)) // fired!!
            sOpenRows.remove(rowid)
        }

        @JvmStatic
        fun gameIsOpen(rowid: Long): Boolean {
            val result = sOpenRows.contains(rowid)
            Log.d(TAG, "gameIsOpen(%d) => %b", rowid, result)
            return result
        }

        private fun setupRematchFor(
            activity: Activity, gamePtr: GamePtr?,
            summary: GameSummary?, gi: CurGameInfo?
        ) {
            if (null != gamePtr) {
                doRematchIf(
                    activity, null, gamePtr.rowid,
                    DBUtils.GROUPID_UNSPEC, summary, gi, gamePtr, false
                )
            } else {
                Log.w(TAG, "setupRematchFor(): unable to lock game")
            }
        }
    }
} // class BoardDelegate

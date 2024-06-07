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
import android.app.AlertDialog
import android.app.Dialog
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.text.TextUtils
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.AdapterView.OnItemSelectedListener
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.CompoundButton
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.Spinner
import android.widget.TextView

import org.eehouse.android.xw4.DictLangCache.LangsArrayAdapter
import org.eehouse.android.xw4.GameLock.GameLockedException
import org.eehouse.android.xw4.NFCUtils.nfcAvail
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.Utils.OnNothingSelDoesNothing
import org.eehouse.android.xw4.XWListItem.DeleteCallback
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole
import org.eehouse.android.xw4.jni.CurGameInfo.XWPhoniesChoice
import org.eehouse.android.xw4.jni.JNIThread
import org.eehouse.android.xw4.jni.LocalPlayer
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.GamePtr

class GameConfigDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.game_config), View.OnClickListener,
    DeleteCallback {
    private val mActivity: Activity
    private var m_gameLockedCheck: CheckBox? = null
    private var mIsLocked = false
    private var mHaveClosed = false
    private var mSub7HintShown = false
    private var mConTypes: CommsConnTypeSet? = null
    private var mAddPlayerButton: Button? = null
    private var mChangeConnButton: Button? = null
    private var mJugglePlayersButton: Button? = null
    private var mDictSpinner: Spinner? = null
    private var mPlayerDictSpinner: Spinner? = null
    private var mRowid: Long = 0
    private var mIsNewGame = false
    private var mNewGameIsSolo = false // only used if m_isNewGame is true
    private var mNewGameName: String? = null
    private var mGi: CurGameInfo? = null
    private var mGiOrig: CurGameInfo? = null
    private var mJniThread: JNIThread? = null
    private var mWhichPlayer = 0
    private var mPhoniesSpinner: Spinner? = null
    private var mBoardsizeSpinner: Spinner? = null
    private var mTraysizeSpinner: Spinner? = null
    private var mLangSpinner: Spinner? = null
    private var mSmartnessSpinner: Spinner? = null
    private var mConnLabel: TextView? = null
    private var mBrowseText: String? = null
    private var mPlayerLayout: LinearLayout? = null
    private var mCarOrig: CommsAddrRec? = null
    private var mCar: CommsAddrRec? = null
    private var mCp: CommonPrefs? = null
    private var mGameStarted = false
    private var mDisabMap: HashMap<CommsConnType, BooleanArray>? = null

    internal inner class RemoteChoices : XWListAdapter(mGi!!.nPlayers) {
        override fun getItem(position: Int): Any {
            return mGi!!.players[position]!!
        }

        override fun getView(
            position: Int, convertView: View,
            parent: ViewGroup
        ): View {
            val gi = mGi!!
            val lstnr: CompoundButton.OnCheckedChangeListener
            lstnr = CompoundButton.OnCheckedChangeListener { buttonView, isChecked ->
                gi.players[position]!!.isLocal = !isChecked
            }
            val cb = CheckBox(mActivity)
            val lp = gi.players[position]!!
            cb.text = lp.name
            cb.setChecked(!lp.isLocal)
            cb.setOnCheckedChangeListener(lstnr)
            return cb
        }
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any): Dialog {
        Assert.assertVarargsNotNullNR(params)
        var dialog: Dialog? = null
        val dlgID = alert.dlgID
        Log.d(TAG, "makeDialog(%s)", dlgID.toString())
        var dlpos: DialogInterface.OnClickListener?
        val ab: AlertDialog.Builder
        when (dlgID) {
            DlgID.PLAYER_EDIT -> {
                val playerEditView = inflate(R.layout.player_edit)
                setPlayerSettings(playerEditView)
                dialog = makeAlertBuilder()
                    .setTitle(R.string.player_edit_title)
                    .setView(playerEditView)
                    .setPositiveButton(
                        android.R.string.ok
                    ) { dlg, button ->
                        getPlayerSettings(dlg)
                        loadPlayersList()
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            DlgID.FORCE_REMOTE -> {
                dlpos = DialogInterface.OnClickListener { dlg, whichButton -> loadPlayersList() }
                val view = inflate(layoutForDlg(dlgID))
                val listview = view.findViewById<View>(R.id.players) as ListView
                listview.setAdapter(RemoteChoices())
                dialog = makeAlertBuilder()
                    .setTitle(R.string.force_title)
                    .setView(view)
                    .setPositiveButton(android.R.string.ok, dlpos)
                    .create()
                alert.setOnDismissListener {
                    if (mGi!!.forceRemoteConsistent()) {
                        showToast(R.string.forced_consistent)
                        loadPlayersList()
                    }
                }
            }

            DlgID.CONFIRM_CHANGE_PLAY, DlgID.CONFIRM_CHANGE -> {
                dlpos = DialogInterface.OnClickListener { dlg, whichButton ->
                    applyChanges(true)
                    if (DlgID.CONFIRM_CHANGE_PLAY == dlgID) {
                        saveAndClose(true)
                    }
                }
                ab = makeAlertBuilder()
                    .setTitle(R.string.confirm_save_title)
                    .setMessage(R.string.confirm_save)
                    .setPositiveButton(R.string.button_save, dlpos)
                dlpos = if (DlgID.CONFIRM_CHANGE_PLAY == dlgID) {
                    DialogInterface.OnClickListener { dlg, whichButton -> finishAndLaunch() }
                } else {
                    null
                }
                ab.setNegativeButton(R.string.button_discard_changes, dlpos)
                dialog = ab.create()
                alert.setOnDismissListener { closeNoSave() }
            }

            DlgID.CHANGE_CONN -> {
                val conTypes = params[0] as CommsConnTypeSet
                val layout = inflate(R.layout.conn_types_display) as LinearLayout
                val items = layout.findViewById<View>(R.id.conn_types) as ConnViaViewLayout
                items.configure(this, conTypes,
                    { typ ->
                        when (typ) {
                            CommsConnType.COMMS_CONN_SMS -> makeConfirmThenBuilder(
                                DlgDelegate.Action.ENABLE_NBS_ASK,
                                R.string.warn_sms_disabled
                            )
                                .setPosButton(R.string.button_enable_sms)
                                .setNegButton(R.string.button_later)
                                .show()

                            CommsConnType.COMMS_CONN_BT -> makeConfirmThenBuilder(
                                DlgDelegate.Action.ENABLE_BT_DO,
                                R.string.warn_bt_disabled
                            )
                                .setPosButton(R.string.button_enable_bt)
                                .setNegButton(R.string.button_later)
                                .show()

                            CommsConnType.COMMS_CONN_RELAY -> Assert.failDbg()
                            CommsConnType.COMMS_CONN_MQTT -> {
                                val msg = """
                                    ${getString(R.string.warn_mqtt_disabled)}
                                    
                                    ${getString(R.string.warn_mqtt_later)}
                                    """.trimIndent()
                                makeConfirmThenBuilder(DlgDelegate.Action.ENABLE_MQTT_DO, msg)
                                    .setPosButton(R.string.button_enable_mqtt)
                                    .setNegButton(R.string.button_later)
                                    .show()
                            }

                            else -> Assert.failDbg()
                        }
                    }, null, this
                )
                val cb = layout
                    .findViewById<View>(R.id.default_check) as CheckBox
                cb.visibility = View.VISIBLE // "gone" in .xml file
                val lstnr = DialogInterface.OnClickListener { dlg, button ->
                    mConTypes = items.types
                    // Remove it if it's actually possible it's there
                    Assert.assertTrueNR(!mConTypes!!.contains(CommsConnType.COMMS_CONN_RELAY))
                    if (cb.isChecked) {
                        XWPrefs.setAddrTypes(mActivity, mConTypes!!)
                    }
                    mCar!!.populate(mActivity, mConTypes!!)
                    setConnLabel()
                    setDisableds()
                }
                dialog = makeAlertBuilder()
                    .setTitle(R.string.title_addrs_pref)
                    .setView(layout)
                    .setPositiveButton(android.R.string.ok, lstnr)
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            else -> dialog = super.makeDialog(alert, *params)
        }
        Assert.assertTrue(dialog != null || !BuildConfig.DEBUG)
        return dialog!!
    } // makeDialog

    private fun setPlayerSettings(playerView: View) {
        Log.d(TAG, "setPlayerSettings()")
        val isServer = !localOnlyGame()

        // Independent of other hide/show logic, these guys are
        // information-only if the game's locked.  (Except that in a
        // local game you can always toggle a player's robot state.)
        Utils.setEnabled(playerView, R.id.remote_check, !mIsLocked)
        Utils.setEnabled(playerView, R.id.player_name_edit, !mIsLocked)
        Utils.setEnabled(
            playerView, R.id.robot_check,
            !mIsLocked || !isServer
        )

        // Hide remote option if in standalone mode...
        val gi = mGi!!
        val lp = gi.players[mWhichPlayer]!!
        Utils.setText(playerView, R.id.player_name_edit, lp.name)
        if (BuildConfig.HAVE_PASSWORD) {
            Utils.setText(playerView, R.id.password_edit, lp.password)
        } else {
            playerView.findViewById<View>(R.id.password_set).visibility = View.GONE
        }

        // Dicts spinner with label
        val dictLabel = playerView
            .findViewById<View>(R.id.dict_label) as TextView
        if (localOnlyGame()) {
            val langName = DictLangCache.getLangNameForISOCode(
                mActivity,
                gi.isoCode()
            )
            val label = getString(R.string.dict_lang_label_fmt, langName)
            dictLabel.text = label
        } else {
            dictLabel.visibility = View.GONE
        }
        mPlayerDictSpinner = (playerView
            .findViewById<View>(R.id.player_dict_spinner) as LabeledSpinner)
            .getSpinner()
        if (localOnlyGame()) {
            configDictSpinner(mPlayerDictSpinner, gi.isoCode()!!, gi.dictName(lp))
        } else {
            mPlayerDictSpinner!!.setVisibility(View.GONE)
            mPlayerDictSpinner = null
        }
        val localSet = playerView.findViewById<View>(R.id.local_player_set)
        var check = playerView.findViewById<View>(R.id.remote_check) as CheckBox
        if (isServer) {
            val lstnr = CompoundButton.OnCheckedChangeListener { buttonView, checked ->
                lp.isLocal = !checked
                Utils.setEnabled(localSet, !checked)
                checkShowPassword(playerView, lp)
            }
            check.setOnCheckedChangeListener(lstnr)
            check.visibility = View.VISIBLE
        } else {
            check.visibility = View.GONE
            Utils.setEnabled(localSet, true)
        }
        check = playerView.findViewById<View>(R.id.robot_check) as CheckBox
        val lstnr = CompoundButton.OnCheckedChangeListener { buttonView, checked ->
            lp.setIsRobot(checked)
            setPlayerName(playerView, lp)
            checkShowPassword(playerView, lp)
        }
        check.setOnCheckedChangeListener(lstnr)
        Utils.setChecked(playerView, R.id.robot_check, lp.isRobot())
        Utils.setChecked(playerView, R.id.remote_check, !lp.isLocal)
        checkShowPassword(playerView, lp)
        Log.d(TAG, "setPlayerSettings() DONE")
    }

    private fun setPlayerName(playerView: View, lp: LocalPlayer) {
        val name =
            if (lp.isRobot()) CommonPrefs.getDefaultRobotName(mActivity)
            else CommonPrefs.getDefaultPlayerName(mActivity, mWhichPlayer)
        setText(playerView, R.id.player_name_edit, name)
    }

    // We show the password stuff only if: non-robot player AND there's more
    // than one local non-robot OR there's already a password set.
    private fun checkShowPassword(playerView: View, lp: LocalPlayer) {
        val isRobotChecked = lp.isRobot()
        // Log.d( TAG, "checkShowPassword(isRobotChecked=%b)", isRobotChecked );
        var showPassword = !isRobotChecked && BuildConfig.HAVE_PASSWORD
        if (showPassword) {
            val pwd = getText(playerView, R.id.password_edit)
            // If it's non-empty, we show it. Else count players
            if (TextUtils.isEmpty(pwd)) {
                var nLocalNonRobots = 0
                for (ii in 0 until mGi!!.nPlayers) {
                    val oneLP = mGi!!.players[ii]!!
                    if (oneLP.isLocal && !oneLP.isRobot()) {
                        ++nLocalNonRobots
                    }
                }
                // Log.d( TAG, "nLocalNonRobots: %d", nLocalNonRobots );
                showPassword = 1 < nLocalNonRobots
            }
        }
        playerView.findViewById<View>(R.id.password_set).visibility =
            if (showPassword) View.VISIBLE else View.GONE
    }

    private fun getPlayerSettings(di: DialogInterface) {
        val dialog = di as Dialog
        val lp = mGi!!.players[mWhichPlayer]!!
        lp.name = Utils.getText(dialog, R.id.player_name_edit)
        if (BuildConfig.HAVE_PASSWORD) {
            lp.password = Utils.getText(dialog, R.id.password_edit)
        }
        if (localOnlyGame()) {
            val position = mPlayerDictSpinner!!.selectedItemPosition
            val adapter = mPlayerDictSpinner!!.adapter
            if (null != adapter && position < adapter.count) {
                val name = adapter.getItem(position) as String
                if (name != mBrowseText) {
                    lp.dictName = name
                }
            }
        }
        lp.setIsRobot(Utils.getChecked(dialog, R.id.robot_check))
        lp.isLocal = !Utils.getChecked(dialog, R.id.remote_check)
    }

    override fun init(savedInstanceState: Bundle?) {
        getBundledData(savedInstanceState)
        mBrowseText = getString(R.string.download_more)
        DictLangCache.setLast(mBrowseText)
        mCp = CommonPrefs.get(mActivity)
        val args = arguments
        mRowid = args.getLong(GameUtils.INTENT_KEY_ROWID, DBUtils.ROWID_NOTFOUND.toLong())
        mNewGameIsSolo = args.getBoolean(INTENT_FORRESULT_SOLO, false)
        mNewGameName = args.getString(INTENT_FORRESULT_NAME)
        mIsNewGame = DBUtils.ROWID_NOTFOUND.toLong() == mRowid
        mAddPlayerButton = findViewById(R.id.add_player) as Button
        mAddPlayerButton!!.setOnClickListener(this)
        mChangeConnButton = findViewById(R.id.change_connection) as Button
        mChangeConnButton!!.setOnClickListener(this)
        mJugglePlayersButton = findViewById(R.id.juggle_players) as Button
        mJugglePlayersButton!!.setOnClickListener(this)
        findViewById(R.id.play_button).setOnClickListener(this)
        mPlayerLayout = findViewById(R.id.player_list) as LinearLayout
        mPhoniesSpinner = (findViewById(R.id.phonies_spinner) as LabeledSpinner)
            .getSpinner()
        mBoardsizeSpinner = (findViewById(R.id.boardsize_spinner) as LabeledSpinner)
            .getSpinner()
        mTraysizeSpinner = (findViewById(R.id.traysize_spinner) as LabeledSpinner)
            .getSpinner()
        mSmartnessSpinner = (findViewById(R.id.smart_robot) as LabeledSpinner)
            .getSpinner()
        mConnLabel = findViewById(R.id.conns_label) as TextView
    } // init

    override fun onResume() {
        if (!mIsNewGame) {
            mJniThread = JNIThread.getRetained(mRowid)
        }
        super.onResume()
        loadGame()
    }

    override fun onPause() {
        saveChanges() // save before clearing m_giOrig!
        mGiOrig = null // flag for onStart and onResume
        super.onPause()
        if (null != mJniThread) {
            mJniThread!!.release()
            mJniThread = null
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putInt(WHICH_PLAYER, mWhichPlayer)
        outState.putSerializable(LOCAL_GI, mGi)
        outState.putSerializable(LOCAL_TYPES, mConTypes)
        if (BuildConfig.DEBUG) {
            outState.putSerializable(DIS_MAP, mDisabMap)
        }
    }

    override fun onActivityResult(requestCode: RequestCode, resultCode: Int, data: Intent?) {
        val cancelled = Activity.RESULT_CANCELED == resultCode
        loadGame()

        val gi = mGi!!
        when (requestCode) {
            RequestCode.REQUEST_DICT -> {
                val dictName =
                    if (cancelled) {
                        gi.dictName!!
                    } else {
                        data!!.getStringExtra(DictsDelegate.RESULT_LAST_DICT)!!
                    }
                configDictSpinner(mDictSpinner, gi.isoCode()!!, dictName)
                configDictSpinner(mPlayerDictSpinner, gi.isoCode()!!, dictName)
            }

            RequestCode.REQUEST_LANG_GC -> {
                val isoCode = if (cancelled) {
                    gi.isoCode()
                } else {
                    ISOCode.newIf(data!!.getStringExtra(DictsDelegate.RESULT_LAST_LANG))
                }
                val langName = DictLangCache.getLangNameForISOCode(mActivity, isoCode)
                selLangChanged(langName)
                setLangSpinnerSelection(langName)
            }

            else -> Assert.failDbg()
        }
    }

    private fun loadGame() {
        if (null == mGiOrig) {
            mGiOrig = CurGameInfo(mActivity)
            if (mIsNewGame) {
                mGiOrig!!.addDefaults(mActivity, mNewGameIsSolo)
            }
            if (mIsNewGame) {
                loadGame(null)
            } else if (null != mJniThread) {
                mJniThread!!.getGamePtr()?.retain().use { gamePtr -> loadGame(gamePtr) }
            } else {
                GameLock.tryLockRO(mRowid).use { lock ->
                    if (null != lock) {
                        GameUtils.loadMakeGame(mActivity, mGiOrig!!, lock)
                            .use { gamePtr -> loadGame(gamePtr) }
                    }
                }
            }
        }
    }

    // Exists only to be called from inside two try-with-resource blocks above
    private fun loadGame(gamePtr: GamePtr?) {
        if (null == gamePtr && !mIsNewGame) {
            Assert.failDbg()
        } else {
            mGameStarted = !mIsNewGame
            if (mGameStarted) {
                mGameStarted = (XwJNI.model_getNMoves(gamePtr) > 0
                        || XwJNI.comms_isConnected(gamePtr))
            }
            if (mGameStarted) {
                if (null == m_gameLockedCheck) {
                    m_gameLockedCheck = findViewById(R.id.game_locked_check) as CheckBox
                    m_gameLockedCheck!!.visibility = View.VISIBLE
                    m_gameLockedCheck!!.setOnClickListener(this)
                }
                handleLockedChange()
            }
            if (null == mGi) {
                mGi = CurGameInfo(mGiOrig!!)
            }
            mCarOrig = if (mIsNewGame) {
                if (mNewGameIsSolo) {
                    CommsAddrRec() // empty
                } else {
                    CommsAddrRec.getSelfAddr(mActivity)
                }
            } else if (XwJNI.game_hasComms(gamePtr)) {
                XwJNI.comms_getSelfAddr(gamePtr)
            } else if (!localOnlyGame()) {
                CommsAddrRec.getSelfAddr(mActivity)
            } else {
                // Leaving this null breaks stuff: an empty set, rather than a
                // null one, represents a standalone game
                CommsAddrRec()
            }

            // load if the first time through....
            if (null == mConTypes) {
                mConTypes = mCarOrig!!.conTypes!!.clone() as CommsConnTypeSet
                if (nfcAvail(mActivity)[0]) {
                    mConTypes!!.add(CommsConnType.COMMS_CONN_NFC)
                }
            }
            if (!mIsNewGame) {
                buildDisabledsMap(gamePtr)
                setDisableds()
            }
            mCar = CommsAddrRec(mCarOrig!!)
            setTitle()
            val label = findViewById(R.id.lang_separator) as TextView
            label.text =
                getString(if (localOnlyGame()) R.string.lang_label else R.string.langdict_label)
            mDictSpinner = findViewById(R.id.dict_spinner) as Spinner
            if (localOnlyGame()) {
                mDictSpinner!!.visibility = View.GONE
                mDictSpinner = null
            }
            setConnLabel()
            loadPlayersList()
            configLangSpinner()
            if (mIsNewGame) {
                val et = findViewById(R.id.game_name_edit) as EditText
                et.setText(mNewGameName)
            } else {
                findViewById(R.id.game_name_edit_row).visibility = View.GONE
            }
            val gi = mGi!!
            mPhoniesSpinner!!.setSelection(gi.phoniesAction!!.ordinal)
            setSmartnessSpinner()
            tweakTimerStuff()
            setChecked(R.id.hints_allowed, !gi.hintsNotAllowed)
            setChecked(R.id.trade_sub_seven, gi.tradeSub7)
            setChecked(R.id.pick_faceup, gi.allowPickTiles)
            findViewById(R.id.trade_sub_seven)
                .setOnClickListener {
                    if (!mSub7HintShown) {
                        mSub7HintShown = true
                        makeNotAgainBuilder(
                            R.string.key_na_sub7new,
                            R.string.sub_seven_allowed_sum
                        )
                            .setTitle(R.string.new_feature_title)
                            .show()
                    }
                }
            setBoardsizeSpinner()
            val curSel = intArrayOf(-1)
            val value = String.format("%d", gi.traySize)
            val adapter = mTraysizeSpinner!!.adapter
            for (ii in 0 until adapter.count) {
                if (value == adapter.getItem(ii)) {
                    mTraysizeSpinner!!.setSelection(ii)
                    curSel[0] = ii
                    break
                }
            }
            mTraysizeSpinner!!
                .setOnItemSelectedListener(object : OnNothingSelDoesNothing() {
                    override fun onItemSelected(
                        parent: AdapterView<*>?, spinner: View,
                        position: Int, id: Long
                    ) {
                        if (curSel[0] != position) {
                            curSel[0] = position
                            makeNotAgainBuilder(
                                R.string.key_na_traysize,
                                R.string.not_again_traysize
                            )
                                .show()
                        }
                    }
                })
        }
    } // loadGame

    private var mTimerStuffInited = false

    init {
        mActivity = delegator.getActivity()
    }

    private fun tweakTimerStuff() {
        val gi = mGi!!
        // one-time only stuff
        if (!mTimerStuffInited) {
            mTimerStuffInited = true

            // dupe-mode check is GONE by default (in the .xml)
            if (CommonPrefs.getDupModeHidden(mActivity)) {
                setChecked(R.id.duplicate_check, false)
            } else {
                val dupCheck = findViewById(R.id.duplicate_check) as CheckBox
                dupCheck.visibility = View.VISIBLE
                dupCheck.setChecked(gi.inDuplicateMode)
                dupCheck.setOnCheckedChangeListener { buttonView, checked -> tweakTimerStuff() }
            }
            val check = findViewById(R.id.use_timer) as CheckBox
            val lstnr =
                CompoundButton.OnCheckedChangeListener { buttonView, checked -> tweakTimerStuff() }
            check.setOnCheckedChangeListener(lstnr)
            check.setChecked(gi.timerEnabled)
        }
        val dupModeChecked = getChecked(R.id.duplicate_check)
        val check = findViewById(R.id.use_timer) as CheckBox
        check.setText(if (dupModeChecked) R.string.use_duptimer else R.string.use_timer)
        val timerSet = getChecked(R.id.use_timer)
        showTimerSet(timerSet)
        val id = if (dupModeChecked) R.string.dup_minutes_label else R.string.minutes_label
        val label = findViewById(R.id.timer_label) as TextView
        label.setText(id)
        var seconds = gi.gameSeconds / 60
        if (!gi.inDuplicateMode) {
            seconds /= gi.nPlayers
        }
        setInt(R.id.timer_minutes_edit, seconds)
    }

    private fun showTimerSet(show: Boolean) {
        val view = findViewById(R.id.timer_set)
        view.visibility = if (show) View.VISIBLE else View.GONE
    }

    private fun getBundledData(bundle: Bundle?) {
        if (null != bundle) {
            mWhichPlayer = bundle.getInt(WHICH_PLAYER)
            mGi = bundle.getSerializable(LOCAL_GI) as CurGameInfo?
            mConTypes = bundle.getSerializable(LOCAL_TYPES) as CommsConnTypeSet?
            if (BuildConfig.DEBUG) {
                mDisabMap = bundle.getSerializable(DIS_MAP) as? HashMap<CommsConnType, BooleanArray>?
            }
        }
    }

    // DeleteCallback interface
    override fun deleteCalled(item: XWListItem) {
        if (mGi!!.delete(item.getPosition())) {
            loadPlayersList()
        }
    }

    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any): Boolean {
        Assert.assertVarargsNotNullNR(params)
        var handled = true
        Assert.assertTrue(curThis() === this)
        when (action) {
            DlgDelegate.Action.LOCKED_CHANGE_ACTION -> handleLockedChange()
            DlgDelegate.Action.SMS_CONFIG_ACTION -> PrefsDelegate.launch(mActivity)
            DlgDelegate.Action.DELETE_AND_EXIT -> closeNoSave()
            DlgDelegate.Action.ASKED_PHONE_STATE -> showDialogFragment(
                DlgID.CHANGE_CONN,
                mConTypes
            )

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    override fun onNegButton(action: DlgDelegate.Action, vararg params: Any): Boolean {
        Assert.assertVarargsNotNullNR(params)
        var handled = true
        when (action) {
            DlgDelegate.Action.DELETE_AND_EXIT -> showConnAfterCheck()
            DlgDelegate.Action.ASKED_PHONE_STATE -> showDialogFragment(
                DlgID.CHANGE_CONN,
                mConTypes
            )

            else -> handled = super.onNegButton(action, *params)
        }
        return handled
    }

    override fun onClick(view: View) {
        if (isFinishing()) {
            // do nothing; we're on the way out
        } else {
            val gi = mGi!!
            when (view.id) {
                R.id.add_player -> {
                    val curIndex = gi.nPlayers
                    if (curIndex < CurGameInfo.MAX_NUM_PLAYERS) {
                        gi.addPlayer() // ups nPlayers
                        loadPlayersList()
                    }
                }

                R.id.juggle_players -> {
                    gi.juggle()
                    loadPlayersList()
                }

                R.id.game_locked_check -> makeNotAgainBuilder(
                    R.string.key_notagain_unlock,
                    DlgDelegate.Action.LOCKED_CHANGE_ACTION,
                    R.string.not_again_unlock
                )
                    .show()

                R.id.change_connection -> showConnAfterCheck()
                R.id.play_button -> {
                    // Launch BoardActivity for m_name, but ONLY IF user
                    // confirms any changes required.  So we either launch
                    // from here if there's no confirmation needed, or launch
                    // a new dialog whose OK button does the same thing.
                    saveChanges()
                    if (!localOnlyGame() && 0 == mConTypes!!.size) {
                        makeConfirmThenBuilder(
                            DlgDelegate.Action.DELETE_AND_EXIT,
                            R.string.config_no_connvia
                        )
                            .setPosButton(R.string.button_discard)
                            .setNegButton(R.string.button_edit)
                            .show()
                    } else if (mIsNewGame || !mGameStarted) {
                        saveAndClose(true)
                    } else if (mGiOrig!!.changesMatter(gi)
                        || mCarOrig!!.changesMatter(mCar!!)
                    ) {
                        showDialogFragment(DlgID.CONFIRM_CHANGE_PLAY)
                    } else {
                        saveAndClose(false)
                    }
                }

                else -> {
                    Log.w(TAG, "unknown v: $view")
                    Assert.failDbg()
                }
            }
        }
    } // onClick

    private fun showConnAfterCheck() {
        if (Perms23.NBSPermsInManifest(mActivity)
            && null == SMSPhoneInfo.get(mActivity)
        ) {
            Perms23.tryGetNBSPermsNA(
                this, R.string.phone_state_rationale,
                R.string.key_na_perms_phonestate,
                DlgDelegate.Action.ASKED_PHONE_STATE
            )
        } else {
            showDialogFragment(DlgID.CHANGE_CONN, mConTypes)
        }
    }

    private fun saveAndClose(forceNew: Boolean) {
        Log.i(TAG, "saveAndClose(forceNew=%b)", forceNew)
        applyChanges(forceNew)
        finishAndLaunch()
    }

    private fun finishAndLaunch() {
        if (!mHaveClosed) {
            mHaveClosed = true
            val intent = Intent()
            if (mIsNewGame) {
                intent.putExtra(INTENT_KEY_GI, mGi)
                // PENDING pass only types, not full addr. Types are defaults
                // we can insert later.
                intent.putExtra(INTENT_KEY_SADDR, mCar)
                val et = findViewById(R.id.game_name_edit) as EditText
                intent.putExtra(INTENT_KEY_NAME, et.getText().toString())
            } else {
                intent.putExtra(GameUtils.INTENT_KEY_ROWID, mRowid)
            }
            setResult(Activity.RESULT_OK, intent)
            finish()
        }
    }

    private fun closeNoSave() {
        if (!mHaveClosed) {
            mHaveClosed = true
            setResult(Activity.RESULT_CANCELED, null)
            finish()
        }
    }

    override fun handleBackPressed(): Boolean {
        var consumed = false
        if (!isFinishing() && null != mGi) {
            val gi = mGi!!
            if (!mIsNewGame) {
                saveChanges()
                if (!mGameStarted) { // no confirm needed
                    applyChanges(true)
                } else if (mGiOrig!!.changesMatter(gi)
                    || mCarOrig!!.changesMatter(mCar!!)
                ) {
                    showDialogFragment(DlgID.CONFIRM_CHANGE)
                    consumed = true // don't dismiss activity yet!
                } else {
                    applyChanges(false)
                }
            }
        }
        return consumed
    }

    override fun curThis(): GameConfigDelegate {
        return super.curThis() as GameConfigDelegate
    }

    private fun loadPlayersList() {
        if (!isFinishing()) {
            val gi = mGi!!
            mPlayerLayout!!.removeAllViews()
            val names = gi.visibleNames(mActivity, false)
            // only enable delete if one will remain (or two if networked)
            val canDelete = names.size > 2 || localOnlyGame() && names.size > 1
            val lstnr = View.OnClickListener { view ->
                mWhichPlayer = (view as XWListItem).getPosition()
                showDialogFragment(DlgID.PLAYER_EDIT)
            }
            val localGame = localOnlyGame()
            val unlocked = (null == m_gameLockedCheck
                    || !m_gameLockedCheck!!.isChecked)
            for (ii in names.indices) {
                val view = XWListItem.inflate(mActivity)
                view.setPosition(ii)
                view.setText(names[ii])
                if (localGame && gi.players[ii]!!.isLocal) {
                    view.setComment(gi.dictName(ii))
                }
                if (canDelete) {
                    view.setDeleteCallback(this)
                }
                view.setEnabled(unlocked)
                view.setOnClickListener(lstnr)
                mPlayerLayout!!.addView(view)
                val divider = inflate(R.layout.divider_view)
                mPlayerLayout!!.addView(divider)
            }
            mAddPlayerButton!!
                .setVisibility(if (names.size >= CurGameInfo.MAX_NUM_PLAYERS) View.GONE else View.VISIBLE)
            mJugglePlayersButton!!
                .setVisibility(if (names.size <= 1) View.GONE else View.VISIBLE)
            if (!localOnlyGame()
                && (0 == gi.remoteCount() || gi.nPlayers == gi.remoteCount())
            ) {
                showDialogFragment(DlgID.FORCE_REMOTE)
            }
            adjustPlayersLabel()
        }
    } // loadPlayersList

    private fun configDictSpinner(
        dictsSpinner: Spinner?, isoCode: ISOCode,
        curDict: String
    ) {
        if (null != dictsSpinner) {
            val langName = DictLangCache.getLangNameForISOCode(mActivity, isoCode)
            dictsSpinner.prompt = getString(
                R.string.dicts_list_prompt_fmt,
                langName
            )
            val onSel: OnItemSelectedListener = object : OnNothingSelDoesNothing() {
                override fun onItemSelected(
                    parentView: AdapterView<*>,
                    selectedItemView: View,
                    position: Int, id: Long
                ) {
                    val chosen = parentView.getItemAtPosition(position) as String
                    val gi = mGi!!
                    if (chosen == mBrowseText) {
                        DictsDelegate.downloadForResult(
                            delegator,
                            RequestCode.REQUEST_DICT,
                            gi.isoCode()!!
                        )
                        Assert.assertTrueNR(isoCode == gi.isoCode())
                    }
                }
            }
            val adapter = DictLangCache.getDictsAdapter(mActivity, isoCode)
            configSpinnerWDownload(dictsSpinner, adapter, onSel, curDict)
        }
    }

    private fun configLangSpinner() {
        if (null == mLangSpinner) {
            mLangSpinner = findViewById(R.id.lang_spinner) as Spinner
            val adapter = DictLangCache.getLangsAdapter(mActivity)
            val onSel: OnItemSelectedListener = object : OnNothingSelDoesNothing() {
                override fun onItemSelected(
                    parentView: AdapterView<*>,
                    selectedItemView: View,
                    position: Int, id: Long
                ) {
                    if (!isFinishing()) { // not on the way out?
                        val chosen = parentView.getItemAtPosition(position) as String
                        if (chosen == mBrowseText) {
                            DictsDelegate.downloadForResult(
                                delegator,
                                RequestCode.REQUEST_LANG_GC
                            )
                        } else {
                            val langName = adapter.getLangAtPosition(position)
                            selLangChanged(langName)
                        }
                    }
                }
            }
            val lang = DictLangCache.getLangNameForISOCode(mActivity, mGi!!.isoCode())
            configSpinnerWDownload(mLangSpinner, adapter, onSel, lang)
        }
    }

    private fun selLangChanged(langName: String) {
        val isoCode = DictLangCache.getLangIsoCode(mActivity, langName)
        val gi = mGi!!
        gi.setLang(mActivity, isoCode)
        loadPlayersList()
        configDictSpinner(mDictSpinner, gi.isoCode()!!, gi.dictName!!)
    }

    private fun configSpinnerWDownload(
        spinner: Spinner?,
        adapter: ArrayAdapter<*>,
        onSel: OnItemSelectedListener,
        curSel: String
    ) {
        val resID = android.R.layout.simple_spinner_dropdown_item
        adapter.setDropDownViewResource(resID)
        spinner!!.setAdapter(adapter)
        spinner.onItemSelectedListener = onSel
        if (mLangSpinner === spinner) {
            setLangSpinnerSelection(curSel)
        } else {
            setSpinnerSelection(spinner, curSel)
        }
    }

    private fun setLangSpinnerSelection(sel: String) {
        val adapter = mLangSpinner!!.adapter as LangsArrayAdapter
        val pos = adapter.getPosForLang(sel)
        if (0 <= pos) {
            mLangSpinner!!.setSelection(pos, true)
        }
    }

    private fun setSpinnerSelection(spinner: Spinner?, sel: String?) {
        if (null != sel && null != spinner) {
            val adapter = spinner.adapter
            val count = adapter.count
            for (ii in 0 until count) {
                if (sel == adapter.getItem(ii)) {
                    spinner.setSelection(ii, true)
                    break
                }
            }
        }
    }

    private fun setSmartnessSpinner() {
        val smartness = mGi!!.robotSmartness
        val setting = when (smartness) {
            1 -> 0
            50 -> 1
            99, 100 -> 2
            else -> {
                Log.w(TAG,"setSmartnessSpinner got smartness $smartness")
                Assert.failDbg()
                -1
            }
        }
        mSmartnessSpinner!!.setSelection(setting)
    }

    private fun positionToSize(position: Int): Int {
        var result = 15
        val sizes = getStringArray(R.array.board_sizes)
        Assert.assertTrueNR(position < sizes.size)
        if (position < sizes.size) {
            val sizeStr = sizes[position]
            result = sizeStr.substring(0, 2).toInt()
        }
        return result
    }

    private fun setBoardsizeSpinner() {
        var selection = 0
        val sizeStr = String.format("%d", mGi!!.boardSize)
        val sizes = getStringArray(R.array.board_sizes)
        for (ii in sizes.indices) {
            if (sizes[ii].startsWith(sizeStr)) {
                selection = ii
                break
            }
        }
        Assert.assertTrue(mGi!!.boardSize == positionToSize(selection))
        mBoardsizeSpinner!!.setSelection(selection)
    }

    private fun buildDisabledsMap(gamePtr: GamePtr?) {
        if (BuildConfig.DEBUG && !localOnlyGame()) {
            if (null == mDisabMap) {
                mDisabMap = HashMap()
                for (typ in CommsConnType.entries) {
                    val bools = booleanArrayOf(
                        XwJNI.comms_getAddrDisabled(gamePtr, typ, false),
                        XwJNI.comms_getAddrDisabled(gamePtr, typ, true)
                    )
                    mDisabMap!![typ] = bools
                }
            }
        }
    }

    private fun setDisableds() {
        if (BuildConfig.DEBUG && null != mDisabMap && !localOnlyGame()) {
            val disableds = findViewById(R.id.disableds) as LinearLayout
            disableds.visibility = View.VISIBLE
            for (ii in disableds.childCount - 1 downTo 0) {
                val child = disableds.getChildAt(ii)
                if (child is DisablesItem) {
                    disableds.removeView(child)
                }
            }
            for (typ in mConTypes!!) {
                val bools = mDisabMap!![typ]
                val item = inflate(R.layout.disables_item) as DisablesItem
                item.init(typ, bools)
                disableds.addView(item)
            }
        }
    }

    private fun adjustPlayersLabel() {
        val label = getString(R.string.players_label_standalone)
        (findViewById(R.id.players_label) as TextView).text = label
    }

    // User's toggling whether everything's locked.  That should mean
    // we enable/disable a bunch of widgits.  And if we're going from
    // unlocked to locked we need to confirm that everything can be
    // reverted.
    private fun handleLockedChange() {
        val locking = m_gameLockedCheck!!.isChecked
        mIsLocked = locking
        for (id in sDisabledWhenLocked) {
            val view = findViewById(id)
            view.setEnabled(!locking)
        }
        val nChildren = mPlayerLayout!!.childCount
        for (ii in 0 until nChildren) {
            val child = mPlayerLayout!!.getChildAt(ii)
            if (child is XWListItem) {
                child.setEnabled(!locking)
            }
        }
    }

    private fun layoutForDlg(dlgID: DlgID): Int {
        val result = when (dlgID) {
            DlgID.FORCE_REMOTE -> R.layout.force_remote
            else -> 0
        }
        Assert.assertTrueNR(result != 0)
        return result
    }

    private fun saveChanges() {
        val gi = mGi!!
        if (!localOnlyGame()) {
            val dictSpinner = findViewById(R.id.dict_spinner) as Spinner
            val name = dictSpinner.getSelectedItem() as String
            if (mBrowseText != name) {
                gi.dictName = name
            }
        }
        gi.inDuplicateMode = getChecked(R.id.duplicate_check)
        gi.hintsNotAllowed = !getChecked(R.id.hints_allowed)
        gi.tradeSub7 = getChecked(R.id.trade_sub_seven)
        gi.allowPickTiles = getChecked(R.id.pick_faceup)
        gi.timerEnabled = getChecked(R.id.use_timer)

        // Get timer value. It's per-move minutes in duplicate mode, otherwise
        // it's for the whole game.
        var seconds = 60 * getInt(R.id.timer_minutes_edit)
        if (!gi.inDuplicateMode) {
            seconds *= gi.nPlayers
        }
        gi.gameSeconds = seconds
        var position = mPhoniesSpinner!!.selectedItemPosition
        gi.phoniesAction = XWPhoniesChoice.entries.toTypedArray().get(position)
        position = mSmartnessSpinner!!.selectedItemPosition
        gi.robotSmartness = position * 49 + 1
        position = mBoardsizeSpinner!!.selectedItemPosition
        gi.boardSize = positionToSize(position)
        gi.traySize = mTraysizeSpinner!!.getSelectedItem().toString().toInt()
        mCar = CommsAddrRec(mConTypes!!)
            .populate(mActivity)
    } // saveChanges

    private fun applyChanges(lock: GameLock, forceNew: Boolean) {
        if (!mIsNewGame) {
            GameUtils.applyChanges1(
                mActivity, mGi!!, mCar, mDisabMap,
                lock, forceNew
            )
            DBUtils.saveThumbnail(mActivity, lock, null) // clear it
        }
    }

    private fun applyChanges(forceNew: Boolean) {
        if (!mIsNewGame && !isFinishing()) {
            if (null != mJniThread) {
                applyChanges(mJniThread!!.getLock(), forceNew)
            } else {
                try {
                    GameLock.lock(mRowid, 100L).use { lock -> applyChanges(lock, forceNew) }
                } catch (gle: GameLockedException) {
                    Log.e(TAG, "applyChanges(): failed to get lock")
                }
            }
        }
    }

    override fun setTitle() {
        val title: String
        if (mIsNewGame) {
            val strID = if (mNewGameIsSolo) R.string.new_game else R.string.new_game_networked
            title = getString(strID)
        } else {
            val strID: Int
            strID = if (null != mConTypes && 0 < mConTypes!!.size) {
                R.string.title_gamenet_config_fmt
            } else {
                R.string.title_game_config_fmt
            }
            val name = GameUtils.getName(mActivity, mRowid)
            title = getString(strID, name)
        }
        setTitle(title)
    }

    private fun localOnlyGame(): Boolean {
        // Log.d( TAG, "localOnlyGame() => %b", result );
        return DeviceRole.SERVER_STANDALONE == mGi!!.serverRole
    }

    private fun setConnLabel() {
        if (localOnlyGame()) {
            mConnLabel!!.visibility = View.GONE
            mChangeConnButton!!.visibility = View.GONE
        } else {
            val connString = mConTypes!!.toString(mActivity, true)
            mConnLabel!!.text = getString(R.string.connect_label_fmt, connString)
            // hide pick-face-up button for networked games
            findViewById(R.id.pick_faceup).visibility = View.GONE
        }
    }

    companion object {
        private val TAG = GameConfigDelegate::class.java.getSimpleName()
        private const val INTENT_FORRESULT_SOLO = "solo"
        private const val INTENT_FORRESULT_NAME = "name"
        private const val WHICH_PLAYER = "WHICH_PLAYER"
        private const val LOCAL_GI = "LOCAL_GI"
        private const val LOCAL_TYPES = "LOCAL_TYPES"
        private const val DIS_MAP = "DIS_MAP"
        const val INTENT_KEY_GI = "key_gi"
        const val INTENT_KEY_SADDR = "key_saddr"
        const val INTENT_KEY_NAME = "key_name"
        private val sDisabledWhenLocked = intArrayOf(
            R.id.juggle_players,
            R.id.add_player,
            R.id.lang_spinner,
            R.id.dict_spinner,
            R.id.hints_allowed,
            R.id.trade_sub_seven,
            R.id.duplicate_check,
            R.id.pick_faceup,
            R.id.boardsize_spinner,
            R.id.traysize_spinner,
            R.id.use_timer,
            R.id.timer_minutes_edit,
            R.id.smart_robot,
            R.id.phonies_spinner,
            R.id.change_connection
        )

        @JvmStatic
        fun editForResult(
            delegator: Delegator,
            requestCode: RequestCode?,
            rowID: Long
        ) {
            val bundle = Bundle()
            bundle.putLong(GameUtils.INTENT_KEY_ROWID, rowID)
            delegator
                .addFragmentForResult(
                    GameConfigFrag.newInstance(delegator),
                    bundle, requestCode
                )
        }

        @JvmStatic
        fun configNewForResult(
            delegator: Delegator,
            requestCode: RequestCode?,
            name: String?, solo: Boolean
        ) {
            val bundle = Bundle()
                .putSerializableAnd(INTENT_FORRESULT_NAME, name)
                .putBooleanAnd(INTENT_FORRESULT_SOLO, solo)
            delegator
                .addFragmentForResult(
                    GameConfigFrag.newInstance(delegator),
                    bundle, requestCode
                )
        }
    }
}

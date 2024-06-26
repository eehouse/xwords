/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2014 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.view.ContextMenu
import android.view.ContextMenu.ContextMenuInfo
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView

import java.lang.ref.WeakReference

import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.GameUtils.GameWrapper
import org.eehouse.android.xw4.MultiService.MultiEvent
import org.eehouse.android.xw4.MultiService.MultiEventListener
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.GamePtr
import org.eehouse.android.xw4.loc.LocUtils


private val TAG: String = DelegateBase::class.java.simpleName

abstract class DelegateBase @JvmOverloads constructor(
    private val mDelegator: Delegator,
    val layoutID: Int,
    private val m_optionsMenuID: Int = R.menu.empty
) : DlgClickNotify, HasDlgDelegate, MultiEventListener
{
    private val mActivity = mDelegator.getActivity()!!
    private val m_dlgDelegate = DlgDelegate(mActivity, this)
    private var m_finishCalled = false
    private var m_rootView: View? = null
    protected var isVisible: Boolean = false
        private set
    private val m_visibleProcs = ArrayList<Runnable>()

    init {
        Assert.assertTrue(0 < m_optionsMenuID)
        LocUtils.xlateTitle(mActivity)
    }

    fun getActivity(): Activity { return mActivity }

    // Does nothing unless overridden. These belong in an interface.
    abstract fun init(savedInstanceState: Bundle?)
    open fun onSaveInstanceState(outState: Bundle) {}
    open fun onPrepareOptionsMenu(menu: Menu): Boolean {
        return false
    }

    open fun onOptionsItemSelected(item: MenuItem): Boolean {
        return false
    }

    open fun onCreateContextMenu(
        menu: ContextMenu, view: View,
        menuInfo: ContextMenuInfo
    ) {}

    open fun onContextItemSelected(item: MenuItem): Boolean {
        return false
    }

    open fun onStop() {}
    open fun onDestroy() {}
    open fun onWindowFocusChanged(hasFocus: Boolean) {}
    open fun handleBackPressed(): Boolean {
        return false
    }

    protected fun requestWindowFeature(feature: Int) {}

    protected fun tryGetPerms(
        perm: Perm, rationale: Int,
        action: DlgDelegate.Action, vararg params: Any?
    ) {
        Perms23.tryGetPerms(this, perm, rationale, action, *params)
    }

    // Fragments only
    fun inflateView(inflater: LayoutInflater,
                    container: ViewGroup?): View?
    {
        // val layoutID = layoutID
        val view =
            if (0 < layoutID) {
                val tmp = inflater.inflate(layoutID, container, false)
                LocUtils.xlateView(mActivity, tmp)
                contentView = tmp
                tmp
            } else {
                null
            }
        return view
    }

    open fun onActivityResult(
        requestCode: RequestCode, resultCode: Int,
        data: Intent
    ) {
        Log.i(TAG, "onActivityResult(): subclass responsibility!!!")
    }

    open fun onStart() {
        synchronized(s_instances) {
            val clazz: Class<*> = javaClass
            if (s_instances.containsKey(clazz)) {
                Log.d(TAG, "onStart(): replacing curThis")
            }
            s_instances.put(clazz, WeakReference(this))
        }
    }

    open fun onResume() {
        isVisible = true
        XWServiceHelper.setListener(this)
        runIfVisible()
        BTUtils.setAmForeground()
    }

    open fun onPause() {
        isVisible = false
        XWServiceHelper.clearListener(this)
        m_dlgDelegate.onPausing()
    }

    protected open fun curThis(): DelegateBase? {
        var result: DelegateBase? = null
        var ref: WeakReference<DelegateBase>?
        synchronized(s_instances) {
            ref = s_instances.get(javaClass)
        }
        if (null != ref) {
            result = ref!!.get()
        }
        if (this !== result) {
            Log.d(TAG, "%s.curThis() => $result", this.toString())
            Assert.failDbg()
        }
        return result
    }

    fun onCreateOptionsMenu(menu: Menu?, inflater: MenuInflater): Boolean {
        val handled = 0 < m_optionsMenuID
        if (handled) {
            inflater.inflate(m_optionsMenuID, menu)
            LocUtils.xlateMenu(mActivity, menu)
        } else {
            Assert.failDbg()
        }

        return handled
    }

    fun onCreateOptionsMenu(menu: Menu?): Boolean {
        val inflater = mActivity.menuInflater
        return onCreateOptionsMenu(menu, inflater)
    }

    protected fun isFinishing(): Boolean
    {
        val result = mActivity.isFinishing
        // Log.d( TAG, "%s.isFinishing() => %b", getClass().getSimpleName(), result );
        return result
    }

    protected val intent: Intent
        get() = mActivity.intent

    protected fun getDelegator(): Delegator { return mDelegator }

    protected val arguments: Bundle?
        get() = mDelegator.getArguments()

    protected var contentView: View?
        get() = m_rootView
        protected set(view) {
            LocUtils.xlateView(mActivity, view)
            m_rootView = view
        }

    fun setContentView(resID: Int) {
        mActivity.setContentView(resID)
        m_rootView = Utils.getContentView(mActivity)
        LocUtils.xlateView(mActivity, m_rootView)
    }

    fun findViewById(resID: Int): View? {
        val result = m_rootView!!.findViewById<View>(resID)
        return result
    }

    fun requireViewById(resID: Int): View {
        val view = findViewById(resID)
        return view!!
    }

    protected fun setVisibility(id: Int, visibility: Int) {
        findViewById(id)?.visibility = visibility
    }

    open fun setTitle() {}

    protected fun setTitle( title: String )
    {
        mActivity.title = title
    }

    protected fun getTitle(): String
    {
        return mActivity.title.toString()
    }

    protected fun startActivityForResult(
        intent: Intent?,
        requestCode: RequestCode
    ) {
        mActivity.startActivityForResult(intent, requestCode.ordinal)
    }

    protected fun setResult(result: Int, intent: Intent?) {
        if (mActivity is MainActivity) {
            val fragment = getDelegator() as XWFragment
            mActivity.setFragmentResult(fragment, result, intent)
        } else {
            mActivity.setResult(result, intent)
        }
    }

    protected fun setResult(result: Int) {
        mActivity.setResult(result)
    }

    protected fun onContentChanged() {
        mActivity.onContentChanged()
    }

    protected fun startActivity(intent: Intent?) {
        mActivity.startActivity(intent)
    }

    protected fun finish() {
        var handled = false
        if (mActivity is MainActivity) {
            if (!m_finishCalled) {
                m_finishCalled = true
                mActivity.finishFragment(mDelegator as XWFragment)
            }
            handled = true
        }

        if (!handled) {
            mActivity.finish()
        }
    }

    private fun runIfVisible() {
        if (isVisible) {
            for (proc: Runnable in m_visibleProcs) {
                post(proc)
            }
            m_visibleProcs.clear()
        }
    }

    fun showFaq(params: Array<String>) {
        val uri = getString(R.string.faq_uri_fmt, params[0], params[1])
        NetUtils.launchWebBrowserWith(mActivity, uri)
    }

    fun getString(resID: Int, vararg params: Any?): String {
        return LocUtils.getString(mActivity, resID, *params)
    }

    fun getQuantityString(
        resID: Int, quantity: Int,
        vararg params: Any?
    ): String {
        return LocUtils.getQuantityString(
            mActivity, resID, quantity,
            *params
        )
    }

    protected fun getStringArray(resID: Int): Array<String?> {
        return LocUtils.getStringArray(mActivity, resID)
    }

    protected fun inflate(resID: Int): View {
        return LocUtils.inflate(mActivity, resID)
    }

    open fun invalidateOptionsMenuIf() {
        mActivity.invalidateOptionsMenu()
    }

    fun showToast(msg: Int) {
        Utils.showToast(mActivity, msg)
    }

    fun showToast(msg: String) {
        Utils.showToast(mActivity, msg)
    }

    fun getSystemService(name: String?): Any {
        return mActivity.getSystemService((name)!!)
    }

    fun runOnUiThread(runnable: Runnable?) {
        mActivity.runOnUiThread(runnable)
    }

    fun setText(parent: View?, id: Int, value: String?) {
        val editText: EditText? = parent!!.findViewById<View>(id) as EditText
        editText?.setText(value, TextView.BufferType.EDITABLE)
    }

    fun setText(id: Int, value: String?) {
        setText(m_rootView, id, value)
    }

    fun getText(parent: View?, id: Int): String {
        val editText = parent!!.findViewById<View>(id) as EditText
        return editText.text.toString()
    }

    fun getText(id: Int): String {
        return getText(m_rootView, id)
    }

    fun setInt(id: Int, value: Int) {
        val str = value.toString()
        setText(id, str)
    }

    fun getInt(id: Int): Int {
        var result = 0
        val str = getText(id)
        try {
            result = str.toInt()
        } catch (nfe: NumberFormatException) {
        }
        return result
    }


    fun setChecked(id: Int, value: Boolean) {
        val cbx = findViewById(id) as CheckBox
        cbx.isChecked = value
    }

    fun getChecked(id: Int): Boolean {
        val cbx = findViewById(id) as CheckBox
        return cbx.isChecked
    }

    open fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog? {
        var dialog: Dialog? = null
        val dlgID = alert.dlgID
        when (dlgID) {
            DlgID.DLG_CONNSTAT -> {
                val ab = makeAlertBuilder()
                val summary = params[0] as GameSummary
                val msg = params[1] as String
                val conTypes = summary.conTypes
                ab.setMessage(msg)
                    .setPositiveButton(android.R.string.ok, null)

                val showDbg = (BuildConfig.NON_RELEASE
                        || XWPrefs.getDebugEnabled(mActivity))
                if (showDbg && null != conTypes) {
                    if (conTypes.contains(CommsConnType.COMMS_CONN_P2P)) {
                        val lstnr: DialogInterface.OnClickListener =
                            object : DialogInterface.OnClickListener {
                                override fun onClick(
                                    dlg: DialogInterface,
                                    buttn: Int
                                ) {
                                    NetStateCache.reset(mActivity)
                                    if (conTypes.contains(
                                            CommsConnType
                                                .COMMS_CONN_P2P
                                        )
                                    ) {
                                        WiDirService.reset(mActivity)
                                    }
                                }
                            }
                        ab.setNegativeButton(R.string.button_reconnect, lstnr)
                    }
                }
                dialog = ab.create()
            }

            else -> Log.d(
                TAG, "%s.makeDialog(): not handling %s", javaClass.simpleName,
                dlgID.toString()
            )
        }
        return dialog
    }

    fun showDialogFragment(dlgID: DlgID, vararg params: Any?)
    {
        runOnUiThread(object : Runnable {
            override fun run() {
                if (isFinishing()) {
                    Log.e(
                        TAG, "not posting dlgID %s b/c %s finishing",
                        dlgID, this
                    )
                    DbgUtils.printStack(TAG)
                } else {
                    show(DBAlert.newInstance(dlgID, arrayOf(*params)))
                }
            }
        })
    }

    fun show(state: DlgState) {
        val df =
            when (state.mID) {
                DlgID.CONFIRM_THEN, DlgID.DIALOG_OKONLY, DlgID.DIALOG_NOTAGAIN ->
                    DlgDelegateAlert.newInstance(state)
                DlgID.DIALOG_ENABLESMS -> EnableSMSAlert.newInstance(state)
                DlgID.INVITE_CHOICES_THEN -> InviteChoicesAlert.newInstance(state)
                else -> {Assert.failDbg(); null}
            }
        show(df)
    }

    fun show(df: XWDialogFragment?) {
        DbgUtils.assertOnUIThread()
        if (null != df && mActivity is XWActivity) {
            mActivity.show(df!!)
        } else {
            Assert.failDbg()
        }
    }

    protected fun buildNamerDlg(
        namer: Renamer?, lstnr1: DialogInterface.OnClickListener?,
        lstnr2: DialogInterface.OnClickListener?, dlgID: DlgID?
    ): Dialog {
        val dialog: Dialog = makeAlertBuilder()
            .setPositiveButton(android.R.string.ok, lstnr1)
            .setNegativeButton(android.R.string.cancel, lstnr2)
            .setView(namer)
            .create()
        return dialog
    }

    fun makeAlertBuilder(): AlertDialog.Builder {
        return LocUtils.makeAlertBuilder(mActivity)
    }

    fun makeNotAgainBuilder(
        key: Int,
        action: DlgDelegate.Action,
        msg: String
    ): DlgDelegate.Builder {
        return m_dlgDelegate.makeNotAgainBuilder(key, action, msg)
    }

    override fun makeNotAgainBuilder(
        key: Int,
        action: DlgDelegate.Action,
        msgId: Int,
        vararg params: Any?
    ): DlgDelegate.Builder {
        return m_dlgDelegate.makeNotAgainBuilder(key, action, msgId, *params)
    }

    fun makeNotAgainBuilder(key: Int, msg: String): DlgDelegate.Builder {
        return m_dlgDelegate.makeNotAgainBuilder(key, msg)
    }

    override fun makeNotAgainBuilder(
        key: Int,
        msgID: Int,
        vararg params: Any?
    ): DlgDelegate.Builder {
        return m_dlgDelegate.makeNotAgainBuilder(key, msgID, *params)
    }

    fun makeConfirmThenBuilder(action: DlgDelegate.Action, msg: String): DlgDelegate.Builder {
        return m_dlgDelegate.makeConfirmThenBuilder(action, msg)
    }

    fun makeConfirmThenBuilder(
        action: DlgDelegate.Action,
        msgId: Int,
        vararg params: Any?
    ): DlgDelegate.Builder {
        return m_dlgDelegate.makeConfirmThenBuilder(action, msgId, *params)
    }

    open fun post(runnable: Runnable): Boolean {
        return m_dlgDelegate.post(runnable)
    }

    protected fun launchLookup(words: Array<String>, isoCode: ISOCode?, noStudy: Boolean) {
        m_dlgDelegate.launchLookup(words, isoCode, noStudy)
    }

    protected fun launchLookup(words: Array<String>, isoCode: ISOCode?) {
        val studyOn = XWPrefs.getStudyEnabled(mActivity)
        m_dlgDelegate.launchLookup(words, isoCode, !studyOn)
    }

    protected fun showInviteChoicesThen(
        action: DlgDelegate.Action, nli: NetLaunchInfo,
        nMissing: Int, nInvited: Int
    ) {
        m_dlgDelegate.showInviteChoicesThen(action, nli, nMissing, nInvited)
    }

    override fun makeOkOnlyBuilder(msgID: Int,
                                   vararg params: Any?): DlgDelegate.Builder
    {
        return m_dlgDelegate.makeOkOnlyBuilder(msgID, *params)
    }

    override fun makeOkOnlyBuilder(msg: String): DlgDelegate.Builder {
        return m_dlgDelegate.makeOkOnlyBuilder(msg)
    }

    protected fun startProgress(titleID: Int, msgID: Int) {
        m_dlgDelegate.startProgress(titleID, msgID, null)
    }

    protected fun startProgress(titleID: Int, msg: String?) {
        m_dlgDelegate.startProgress(titleID, msg, null)
    }

    protected fun startProgress(title: String?, msg: String?) {
        m_dlgDelegate.startProgress(title, msg, null)
    }

    protected fun startProgress(
        titleID: Int, msgID: Int,
        lstnr: DialogInterface.OnCancelListener?
    ) {
        m_dlgDelegate.startProgress(titleID, msgID, lstnr)
    }

    protected fun startProgress(
        titleID: Int, msg: String?,
        lstnr: DialogInterface.OnCancelListener?
    ) {
        m_dlgDelegate.startProgress(titleID, msg, lstnr)
    }

    protected fun setProgressMsg(id: Int) {
        m_dlgDelegate.setProgressMsg(id)
    }

    protected fun stopProgress() {
        m_dlgDelegate.stopProgress()
    }

    fun showSMSEnableDialog(action: DlgDelegate.Action) {
        m_dlgDelegate.showSMSEnableDialog(action)
    }

    open fun canHandleNewIntent(intent: Intent): Boolean {
        Log.d(TAG, "canHandleNewIntent() => false")
        return false
    }

    open fun handleNewIntent(intent: Intent) {
        Log.d(TAG, "handleNewIntent(%s): not handling", intent.toString())
    }

    protected fun runWhenActive(proc: Runnable) {
        m_visibleProcs.add(proc)
        runIfVisible()
    }

    fun onStatusClicked(gamePtr: GamePtr?) {
        val addrs = XwJNI.comms_getAddrs(gamePtr)
        val addr = if (null != addrs && 0 < addrs.size) addrs[0] else null
        val summary = GameUtils.getSummary(mActivity, gamePtr!!.rowid, 1)
        if (null != summary) {
            val msg = ConnStatusHandler.getStatusText(
                mActivity, (gamePtr), summary.gameID,
                summary.conTypes, addr
            )

            post(Runnable {
                if (null == msg) {
                    askNoAddrsDelete()
                } else {
                    showDialogFragment(
                        DlgID.DLG_CONNSTAT, summary,
                        msg
                    )
                }
            })
        }
    }

    fun onStatusClicked(rowid: Long) {
        Log.d(TAG, "onStatusClicked(%d)", rowid)

        GameWrapper.make(mActivity, rowid).use { gw ->
            if (null != gw) {
                onStatusClicked(gw.gamePtr())
            }
        }
    }

    protected fun askNoAddrsDelete() {
        makeConfirmThenBuilder(
            DlgDelegate.Action.DELETE_AND_EXIT,
            R.string.connstat_net_noaddr
        )
            .setPosButton(R.string.list_item_delete)
            .setNegButton(R.string.button_close_game)
            .show()
    }

    //////////////////////////////////////////////////
    // MultiService.MultiEventListener interface
    //////////////////////////////////////////////////
    override fun eventOccurred(event: MultiEvent, vararg args: Any?) {
        var fmtId = 0
        var notAgainKey = 0
        when (event) {
            MultiEvent.BAD_PROTO_BT -> {
                fmtId = R.string.bt_bad_proto_fmt
                notAgainKey = R.string.key_na_bt_badproto
            }

            MultiEvent.BAD_PROTO_SMS -> fmtId = R.string.sms_bad_proto_fmt
            MultiEvent.APP_NOT_FOUND_BT -> fmtId = R.string.app_not_found_fmt
            else -> Log.d(TAG, "eventOccurred(event=%s) (DROPPED)", event.toString())
        }
        if (0 != fmtId) {
            val msg = getString(fmtId, args[0] as String)
            val key = notAgainKey
            runOnUiThread(object : Runnable {
                override fun run() {
                    val builder =
                        if (0 == key) makeOkOnlyBuilder(msg)
                        else makeNotAgainBuilder(key, msg)

                    builder.show()
                }
            })
        }
    }

    //////////////////////////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////////////////////////
    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any?): Boolean {
        var handled = true
        when (action) {
            DlgDelegate.Action.ENABLE_NBS_ASK -> showSMSEnableDialog(DlgDelegate.Action.ENABLE_NBS_DO)
            DlgDelegate.Action.ENABLE_NBS_DO -> XWPrefs.setNBSEnabled(mActivity, true)
            DlgDelegate.Action.ENABLE_BT_DO -> BTUtils.enable(mActivity)
            DlgDelegate.Action.ENABLE_MQTT_DO -> {
                XWPrefs.setMQTTEnabled(mActivity, true)
                MQTTUtils.setEnabled(mActivity, true)
            }

            DlgDelegate.Action.PERMS_QUERY ->
                Perms23.onGotPermsAction(this, true, params[0] as Perms23.GotPermsState)
            DlgDelegate.Action.SHOW_FAQ -> showFaq(params[0] as Array<String>)
            else -> {
                Log.d(TAG, "onPosButton(): unhandled action %s", action.toString())
                // Assert.assertTrue( !BuildConfig.DEBUG );
                handled = false
            }
        }
        return handled
    }

    override fun onNegButton(action: DlgDelegate.Action,
                             vararg params: Any?): Boolean
    {
        var handled = true
        when (action) {
            DlgDelegate.Action.PERMS_QUERY ->
                Perms23.onGotPermsAction(
                    this, false, params[0] as Perms23.GotPermsState)
            else -> {
                Log.d(TAG, "onNegButton: unhandled action %s", action.toString())
                handled = false
            }
        }
        return handled
    }

    override fun onDismissed(action: DlgDelegate.Action,
                             vararg params: Any?): Boolean
    {
        var handled = false
        Log.d(
            TAG, "%s.onDismissed(%s)", javaClass.simpleName,
            action.toString()
        )

        when (action) {
            DlgDelegate.Action.PERMS_QUERY -> {
                handled = true
                Perms23.onGotPermsAction(
                    this, false, params[0] as Perms23.GotPermsState)
            }

            DlgDelegate.Action.SKIP_CALLBACK -> {}
            else -> Log.e(TAG, "onDismissed(): not handling action %s", action)
        }
        return handled
    }

    override fun inviteChoiceMade(
        action: DlgDelegate.Action,
        means: InviteMeans,
        vararg params: Any?
    ) {
        // Assert.fail();
        Log.d(TAG, "inviteChoiceMade($action) not implemented")
    }

    companion object {
        private val s_instances
                : MutableMap<Class<*>, WeakReference<DelegateBase>> = HashMap()

        val hasLooper: Activity?
            get() {
                var result: Activity? = null
                synchronized(s_instances) {
                    for (ref: WeakReference<DelegateBase> in s_instances.values) {
                        val base: DelegateBase? = ref.get()
                        if (null != base) {
                            result = base.mActivity
                            break
                        }
                    }
                }
                return result
            }
    }
}

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

import android.app.Activity
import android.app.AlertDialog
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.AsyncTask
import android.os.Bundle
import android.text.TextUtils
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemLongClickListener
import android.widget.Button
import android.widget.CheckBox
import android.widget.LinearLayout
import android.widget.PopupMenu
import android.widget.TextView
import androidx.preference.PreferenceManager
import org.eehouse.android.xw4.DBUtils.countGamesUsingISOCode
import org.eehouse.android.xw4.DBUtils.dictsMoveInfo
import org.eehouse.android.xw4.DBUtils.getStringFor
import org.eehouse.android.xw4.DBUtils.setStringFor
import org.eehouse.android.xw4.DbgUtils.showf
import org.eehouse.android.xw4.DictUtils.DictAndLoc
import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.DictUtils.ON_SERVER
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener
import org.eehouse.android.xw4.DwnldDelegate.OnGotLcDictListener
import org.eehouse.android.xw4.MountEventReceiver.SDCardNotifiee
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.SelectableItem.LongClickHandler
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.XWExpListAdapter.GroupTest
import org.eehouse.android.xw4.XWListItem.ExpandedListener
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.loc.LocUtils
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.Serializable
import java.text.Collator
import java.util.Arrays

class DictsDelegate(delegator: Delegator) :
    ListDelegateBase(delegator, R.layout.dicts_browse, R.menu.dicts_menu),
    View.OnClickListener, OnItemLongClickListener, SelectableItem, SDCardNotifiee,
    DlgClickNotify, GroupStateListener, DownloadFinishedListener, ExpandedListener
{
    private val mActivity: Activity
    private val mClosedLangs: MutableSet<String> = HashSet()
    private var mExpandedItems: MutableSet<AvailDictInfo>? = null
    private var mAdapter: DictListAdapter? = null
    private var mQuickFetchMode = false
    private var mLangs: Array<String>? = null
    private var mCheckbox: CheckBox? = null
    private var mLocNames: Array<String>? = null
    private var mFinishOnName: String? = null
    private var mSelViews: MutableMap<String?, XWListItem>? = null
    private var mSelDicts: MutableMap<String, Any>? = HashMap()
    private var mOrigTitle: String? = null
    private var mShowRemote = false
    private var mFilterLang: String? = null
    private var mOnServerStr: String? = null
    private var mLastLang: ISOCode? = null
    private var mLastDict: String? = null

    private class AvailDictInfo(
        var m_name: String, // what needs to be in URL
        var mISOCode: ISOCode?, // what we display to user, i.e. translated
        var m_localLang: String?,
        var m_nWords: Int, var m_nBytes: Long, var m_note: String?
    ) : Comparable<Any?>, Serializable {
        override fun compareTo(obj: Any?): Int {
            val other = obj as AvailDictInfo?
            return m_name.compareTo(other!!.m_name)
        }
    }

    private class LangInfo(var m_posn: Int, objs: Collection<Any>) {
        var m_numDictsInst = 0
        var m_numDictsAvail = 0

        init {
            for (obj in objs) {
                if (obj is AvailDictInfo) {
                    ++m_numDictsAvail
                } else if (obj is DictAndLoc) {
                    ++m_numDictsInst
                } else {
                    Assert.failDbg()
                }
            }
        }
    }

    private var mRemoteInfo: HashMap<String, Array<AvailDictInfo>>? = null
    private var m_launchedForMissing = false

    private inner class DictListAdapter(private val m_context: Context) :
        XWExpListAdapter(arrayOf(LangInfo::class.java,
                                 DictAndLoc::class.java,
                                 AvailDictInfo::class.java))
    {
        public override fun makeListData(): Array<Any> {
            val alist = ArrayList<Any>()
            val nLangs = mLangs!!.size
            for (ii in 0 until nLangs) {
                val langName = mLangs!![ii]
                if (null != mFilterLang && mFilterLang != langName) {
                    continue
                }
                val items = makeLangItems(langName)
                Assert.assertTrueNR(0 < items.size)
                alist.add(LangInfo(ii, items))
                if (!mClosedLangs.contains(langName)) {
                    alist.addAll(items)
                }
            }
            return alist.toTypedArray<Any>()
        } // makeListData

        public override fun getView(dataObj: Any, convertView: View?): View {
            var result: View? = null
            if (dataObj is LangInfo) {
                val info = dataObj
                val groupPos = info.m_posn
                val langName = mLangs!![groupPos]
                val expanded = !mClosedLangs.contains(langName)
                var details: String? = null
                if (0 < info.m_numDictsInst && 0 < info.m_numDictsAvail) {
                    details = getString(
                        R.string.dict_lang_inst_and_avail,
                        info.m_numDictsInst, info.m_numDictsAvail
                    )
                } else if (0 < info.m_numDictsAvail) {
                    details = getString(
                        R.string.dict_lang_avail,
                        info.m_numDictsAvail
                    )
                } else if (0 < info.m_numDictsInst) {
                    details = getString(R.string.dict_lang_inst, info.m_numDictsInst)
                } else {
                    Assert.failDbg()
                }
                val title = Utils.capitalize(langName) + " " + details
                result = ListGroup.make(
                    m_context, convertView,
                    this@DictsDelegate, groupPos, title,
                    expanded
                )
            } else {
                val item: XWListItem
                item =
                    if (null != convertView && convertView is XWListItem) {
                        convertView
                    } else {
                        XWListItem.inflate(mActivity, this@DictsDelegate)
                    }
                result = item
                var name: String? = null
                if (dataObj is DictAndLoc) {
                    val dal = dataObj
                    name = dal.name
                    if (ON_SERVER.NO == DictLangCache.getOnServer(m_context, dal)) {
                        item.setIsCustom(true)
                    }
                    val loc = dal.loc
                    item.setComment(mLocNames!![loc.ordinal])
                    item.setCached(loc)
                    item.setOnClickListener(this@DictsDelegate)
                    item.setExpandedListener(null) // item might be reused
                } else if (dataObj is AvailDictInfo) {
                    val info = dataObj
                    name = info.m_name
                    item.setCached(info)
                    item.setExpandedListener(this@DictsDelegate)
                    item.setExpanded(mExpandedItems!!.contains(info))
                    item.setComment(mOnServerStr)
                } else {
                    Assert.failDbg()
                }
                item.setText(name!!)
                val selected = mSelDicts!!.containsKey(name)
                if (selected) {
                    mSelViews!![name] = item
                }
                item.isSelected = selected
            }
            return result!!
        }

        private fun makeTestFor(langName: String): GroupTest {
            return object : GroupTest {
                override fun isTheGroup(item: Any): Boolean {
                    val info = item as LangInfo
                    return mLangs!![info.m_posn] == langName
                }
            }
        }

        fun removeLangItems(langName: String) {
            val indx = findGroupItem(makeTestFor(langName))
            removeChildrenOf(indx)
        }

        fun addLangItems(langName: String) {
            val indx = findGroupItem(makeTestFor(langName))
            addChildrenOf(indx, makeLangItems(langName))
        }

        private fun makeLangItems(langName: String): ArrayList<Any> {
            val result = ArrayList<Any>()
            val locals = HashSet<String>()
            val isoCode = DictLangCache.getLangIsoCode(m_context, langName)
            val dals = DictLangCache.getDALsHaveLang(m_context, isoCode!!)
            dals.map{locals.add(it.name)}

            if (mShowRemote && null != mRemoteInfo) {
                val infos = mRemoteInfo!![langName]
                if (null != infos) {
                    for (info in infos) {
                        if (!locals.contains(info.m_name)) {
                            result.add(info)
                        }
                    }
                } else {
                    Log.w(TAG, "No remote info for lang %s", langName)
                }
            }

            // Now append locals
            if (null != dals) {
                result.addAll(Arrays.asList(*dals))
            }
            return result
        }
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog {
        val lstnr: DialogInterface.OnClickListener
        val lstnr2: DialogInterface.OnClickListener

        val message: String
        val doRemove = true
        val dialog =
            when (alert.dlgID) {
                DlgID.MOVE_DICT -> {
                val selNames = getSelNames()
                val moveTo = intArrayOf(-1)
                message = getString(
                    R.string.move_dict_fmt,
                    getJoinedSelNames(getSelNames())
                )
                val newSelLstnr = DialogInterface.OnClickListener { dlgi, item ->
                    moveTo[0] = item
                    val dlg = dlgi as AlertDialog
                    val btn = dlg.getButton(AlertDialog.BUTTON_POSITIVE)
                    btn.setEnabled(true)

                    // Ask for STORAGE (but do nothing if not granted)
                    if (DictLoc.DOWNLOAD == itemToRealLoc(item)) {
                        Perms23.Builder(Perm.STORAGE)
                            .asyncQuery(mActivity)
                    }
                }
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    val toLoc = itemToRealLoc(moveTo[0])
                    moveDicts(selNames, toLoc)
                }
                AlertDialog.Builder(mActivity)
                    .setTitle(message)
                    .setSingleChoiceItems(
                        makeDictDirItems(), moveTo[0],
                        newSelLstnr
                    )
                    .setPositiveButton(R.string.button_move, lstnr)
                    .setNegativeButton(android.R.string.cancel, null)
                    .create()
            }

            DlgID.SET_DEFAULT -> {
                val dictName = mSelDicts!!.keys.iterator().next()
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    if (DialogInterface.BUTTON_NEGATIVE == item
                        || DialogInterface.BUTTON_POSITIVE == item
                    ) {
                        setDefault(
                            dictName, R.string.key_default_dict,
                            R.string.key_default_robodict
                        )
                    }
                    if (DialogInterface.BUTTON_NEGATIVE == item
                        || DialogInterface.BUTTON_NEUTRAL == item
                    ) {
                        setDefault(
                            dictName, R.string.key_default_robodict,
                            R.string.key_default_dict
                        )
                    }
                }
                val lang = DictLangCache.getDictLangName(
                    mActivity,
                    dictName
                )
                message = getString(
                    R.string.set_default_message_fmt,
                    dictName, lang
                )
                makeAlertBuilder()
                    .setTitle(R.string.query_title)
                    .setMessage(message)
                    .setPositiveButton(R.string.button_default_human, lstnr)
                    .setNeutralButton(R.string.button_default_robot, lstnr)
                    .setNegativeButton(R.string.button_default_both, lstnr)
                    .create()
            }

            DlgID.DICT_OR_DECLINE -> {
                lstnr = DialogInterface.OnClickListener { dlg, item ->
                    val intent = intent
                    val isoCode = ISOCode
                        .newIf(intent.getStringExtra(MultiService.ISO))
                    val name = intent.getStringExtra(MultiService.DICT)
                    m_launchedForMissing = true
                    DwnldDelegate
                        .downloadDictInBack(
                            mActivity, isoCode, name!!,
                            this@DictsDelegate
                        )
                }
                lstnr2 = DialogInterface.OnClickListener { dlg, item -> curThis().finish() }
                MultiService.missingDictDialog(
                    mActivity, intent,
                    lstnr, lstnr2
                )
            }

            else -> super.makeDialog(alert, *params)
        }
        return dialog!!
    } // makeDialog

    override fun init(savedInstanceState: Bundle?) {
        mOnServerStr = getString(R.string.dict_on_server)
        val closed = XWPrefs.getClosedLangs(mActivity)
        if (null != closed) {
            mClosedLangs.addAll(Arrays.asList(*closed))
        }
        mExpandedItems = HashSet()
        mLocNames = getStringArray(R.array.loc_names) as Array<String>
        listView!!.setOnItemLongClickListener(this)
        mCheckbox = findViewById(R.id.show_remote) as CheckBox
        mCheckbox!!.setOnClickListener(this)
        getBundledData(savedInstanceState)
        mCheckbox!!.setSelected(mShowRemote)
        val args = arguments
        if (null != args) {
            if (MultiService.isMissingDictBundle(args)) {
                showDialogFragment(DlgID.DICT_OR_DECLINE)
            } else {
                val showRemote = args.getBoolean(DICT_SHOWREMOTE, false)
                if (showRemote) {
                    mQuickFetchMode = true
                    mShowRemote = true
                    mCheckbox!!.visibility = View.GONE
                    val isoCode = ISOCode.newIf(args.getString(DICT_LANG_EXTRA))
                    if (null != isoCode) {
                        mFilterLang = DictLangCache.getLangNameForISOCode(mActivity, isoCode)
                        mClosedLangs.remove(mFilterLang)
                    }
                    val name = args.getString(DICT_NAME_EXTRA)
                    if (null == name) {
                        FetchListTask(mActivity).execute()
                    } else {
                        mFinishOnName = name
                        startDownload(isoCode, name)
                    }
                }
                downloadNewDict(args)
            }
        }
        mOrigTitle = getTitle()
        makeNotAgainBuilder(R.string.key_na_dicts, R.string.not_again_dicts)
            .show()
        Perms23.tryGetPermsNA(
            this, Perm.STORAGE, R.string.dicts_storage_rationale,
            R.string.key_na_perms_storage_dicts,
            DlgDelegate.Action.STORAGE_CONFIRMED
        )
    } // init

    override fun onResume() {
        super.onResume()
        MountEventReceiver.register(this)
        setTitleBar()
    }

    override fun onStop() {
        MountEventReceiver.unregister(this)
        super.onStop()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putBoolean(REMOTE_SHOW_KEY, mShowRemote)
        outState.putSerializable(REMOTE_INFO_KEY, mRemoteInfo)
        outState.putSerializable(SEL_DICTS_KEY, mSelDicts as? HashMap<*, *>?)
    }

    private fun getBundledData(sis: Bundle?) {
        if (null != sis) {
            mShowRemote = sis.getBoolean(REMOTE_SHOW_KEY, false)
            mRemoteInfo = sis.getSerializable(REMOTE_INFO_KEY) as? HashMap<String, Array<AvailDictInfo>>
            mSelDicts = sis.getSerializable(SEL_DICTS_KEY) as? HashMap<String, Any>
        }
    }

    override fun onClick(view: View) {
        if (view === mCheckbox) {
            switchShowingRemote(mCheckbox!!.isChecked)
        } else {
            val item = view as XWListItem
            DictBrowseDelegate.launch(
                getDelegator(), item.getText(),
                (item.getCached() as DictLoc),
                item.getIsCustom())
        }
    }

    override fun handleBackPressed(): Boolean {
        val handled = 0 < mSelDicts!!.size
        if (handled) {
            clearSelections()
        } else {
            if (null != mLastLang && null != mLastDict) {
                val intent = Intent()
                intent.putExtra(RESULT_LAST_LANG, mLastLang.toString())
                intent.putExtra(RESULT_LAST_DICT, mLastDict)
                setResult(Activity.RESULT_OK, intent)
            } else {
                setResult(Activity.RESULT_CANCELED)
            }
        }
        return handled
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        // int nSel = m_selDicts.size();
        val nSels = countSelDicts()
        Utils.setItemVisible(
            menu, R.id.dicts_select,
            1 == nSels[SEL_LOCAL] && 0 == nSels[SEL_REMOTE]
        )

        // NO -- test if any downloadable selected
        Utils.setItemVisible(
            menu, R.id.dicts_download,
            0 == nSels[SEL_LOCAL] && 0 < nSels[SEL_REMOTE]
        )
        Utils.setItemVisible(
            menu, R.id.dicts_deselect_all,
            0 < nSels[SEL_LOCAL] || 0 < nSels[SEL_REMOTE]
        )
        val allVolatile = 0 == nSels[SEL_REMOTE] && selItemsVolatile()
        Utils.setItemVisible(
            menu, R.id.dicts_move,
            allVolatile && DictUtils.haveWriteableSD()
        )
        Utils.setItemVisible(menu, R.id.dicts_delete, allVolatile)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        var handled = true
        when (item.itemId) {
            R.id.dicts_delete -> deleteSelected()
            R.id.dicts_move -> showDialogFragment(DlgID.MOVE_DICT)
            R.id.dicts_select -> showDialogFragment(DlgID.SET_DEFAULT)
            R.id.dicts_deselect_all -> clearSelections()
            R.id.dicts_download -> {
                val uris = ArrayList<Uri>()
                val names = ArrayList<String>()
                var count = 0
                for ((name, cached) in mSelDicts!!) {
                    if (cached is AvailDictInfo) {
                        val uri = Utils.makeDictUriFromCode(
                            mActivity,
                            cached.mISOCode, name
                        )
                        uris.add(uri)
                        names.add(name)
                    }
                }
                DwnldDelegate.downloadDictsInBack(mActivity,
                                                  uris.toTypedArray(),
                                                  names.toTypedArray(),
                                                  this)
            }

            else -> handled = false
        }
        return handled
    }

    private fun moveDicts(selNames: Array<String>, toLoc: DictLoc) {
        if (toLoc.needsStoragePermission()) {
            tryGetPerms(
                Perm.STORAGE, R.string.move_dict_rationale,
                DlgDelegate.Action.MOVE_CONFIRMED, selNames, toLoc
            )
        } else {
            moveDictsWithPermission(selNames, toLoc)
        }
    }

    private fun moveDictsWithPermission(selNames: Array<String>, toLoc: DictLoc) {
        for (name in selNames) {
            val fromLoc = mSelDicts!![name] as DictLoc
            if (fromLoc == toLoc) {
                Log.w(TAG, "not moving %s: same loc", name)
            } else if (DictUtils.moveDict(
                    mActivity,
                    name, fromLoc,
                    toLoc
                )
            ) {
                if (mSelViews!!.containsKey(name)) {
                    val selItem = mSelViews!![name]
                    selItem!!.setComment(mLocNames!![toLoc.ordinal])
                    selItem.setCached(toLoc)
                    selItem.invalidate()
                }
                dictsMoveInfo(
                    mActivity, name,
                    fromLoc, toLoc
                )
            } else {
                showf(mActivity, R.string.toast_no_permission)
                Log.w(TAG, "moveDict(%s) failed", name)
            }
        }
    }

    private fun switchShowingRemote(showRemote: Boolean) {
        // if showing for the first time, download remote info and let the
        // completion routine finish (or clear the checkbox if cancelled.)
        // Otherwise just toggle boolean and redraw.
        if (mShowRemote != showRemote) {
            mShowRemote = showRemote
            if (showRemote && null == mRemoteInfo) {
                FetchListTask(mActivity).execute()
            } else {
                mkListAdapter()
            }
        }
    }

    private fun countNeedDownload(): Int {
        var result = 0
        val iter: Iterator<Any> = mSelDicts!!.values.iterator()
        while (iter.hasNext()) {
            val obj = iter.next()
            if (obj is AvailDictInfo) {
                ++result
            }
        }
        return result
    }

    private fun downloadNewDict(args: Bundle) {
        val loci = args.getInt(UpdateCheckReceiver.NEW_DICT_LOC, 0)
        if (0 < loci) {
            val name = args.getString(UpdateCheckReceiver.NEW_DICT_NAME)
            val url = args.getString(UpdateCheckReceiver.NEW_DICT_URL)
            val uri = Uri.parse(url)
            DwnldDelegate.downloadDictInBack(mActivity, uri, name!!, null)
            finish()
        }
    }

    private fun setDefault(name: String?, keyId: Int, otherKey: Int) {
        val isoCode = DictLangCache.getDictISOCode(mActivity, name)
        val curLangName = XWPrefs.getPrefsString(mActivity, R.string.key_default_language)
        val curISOCode = DictLangCache.getLangIsoCode(mActivity, curLangName!!)
        val changeLang = isoCode != curISOCode
        val sp = PreferenceManager.getDefaultSharedPreferences(mActivity)
        val editor = sp.edit()
        var key = getString(keyId)
        editor.putString(key, name)
        if (changeLang) {
            // change other dict too
            key = getString(otherKey)
            editor.putString(key, name)

            // and change language
            val langName = DictLangCache.getLangNameForISOCode(mActivity, isoCode)
            key = getString(R.string.key_default_language)
            editor.putString(key, langName)
        }
        editor.commit()
    }

    //////////////////////////////////////////////////////////////////////
    // GroupStateListener interface
    //////////////////////////////////////////////////////////////////////
    override fun onGroupExpandedChanged(groupObj: Any, expanded: Boolean) {
        val lg = groupObj as ListGroup
        val langName = mLangs!![lg.position]
        if (expanded) {
            mClosedLangs.remove(langName)
            mAdapter!!.addLangItems(langName)
        } else {
            mClosedLangs.add(langName)
            mAdapter!!.removeLangItems(langName)
        }
        saveClosed()
    }

    //////////////////////////////////////////////////////////////////////
    // OnItemLongClickListener interface
    //////////////////////////////////////////////////////////////////////
    override fun onItemLongClick(
        parent: AdapterView<*>?, view: View,
        position: Int, id: Long
    ): Boolean {
        val success = view is LongClickHandler
        if (success) {
            (view as LongClickHandler).longClicked()
        }
        return success
    }

    private fun selItemsVolatile(): Boolean {
        var result = 0 < mSelDicts!!.size
        val iter: Iterator<Any> = mSelDicts!!.values.iterator()
        while (result && iter.hasNext()) {
            val obj = iter.next()
            if (obj is DictLoc) {
                if (obj == DictLoc.BUILT_IN) {
                    result = false
                }
            } else {
                result = false
            }
        }
        return result
    }

    private fun deleteSelected() {
        val names = getSelNames()
        var msg = getQuantityString(
            R.plurals.confirm_delete_dict_fmt,
            names.size, getJoinedSelNames(names)
        )

        // Confirm.  And for each dict, warn if (after ALL are deleted) any
        // game will no longer be openable without downloading.  For now
        // anyway skip warning for the case where user will have to switch to
        // a different same-lang wordlist to open a game.
        class LangDelData(isoCode: ISOCode?) {
            fun dictsStr(): String? {
                if (null == m_asArray) {
                    val arr = delDicts.toTypedArray<String?>()
                    m_asArray = TextUtils.join(", ", arr)
                }
                return m_asArray
            }

            var delDicts: MutableSet<String?>
            private var m_asArray: String? = null
            var langName: String
            var nDicts: Int

            init {
                delDicts = HashSet()
                langName = DictLangCache.getLangNameForISOCode(mActivity, isoCode!!)!!
                nDicts = DictLangCache.getDALsHaveLang(mActivity, isoCode).size
            }
        }

        val dels: MutableMap<ISOCode, LangDelData> = HashMap()
        val skipLangs: MutableSet<ISOCode> = HashSet()
        for (dict in mSelDicts!!.keys) {
            val isoCode = DictLangCache.getDictISOCode(mActivity, dict)
            if (skipLangs.contains(isoCode)) {
                continue
            }
            val nUsingLang = countGamesUsingISOCode(mActivity, isoCode)
            if (0 == nUsingLang) {
                // remember, since countGamesUsingLang is expensive
                skipLangs.add(isoCode)
            } else {
                var data = dels[isoCode]
                if (null == data) {
                    data = LangDelData(isoCode)
                    dels[isoCode] = data
                }
                data.delDicts.add(dict)
            }
        }
        val iter: Iterator<LangDelData> = dels.values.iterator()
        while (iter.hasNext()) {
            val data = iter.next()
            val nLeftAfter = data.nDicts - data.delDicts.size
            if (0 == nLeftAfter) { // last in this language?
                val newMsg = getString(
                    R.string.confirm_deleteonly_dicts_fmt,
                    data.dictsStr(), data.langName
                )
                msg += """
                    
                    
                    $newMsg
                    """.trimIndent()
            }
        }
        makeConfirmThenBuilder(DlgDelegate.Action.DELETE_DICT_ACTION, msg)
            .setPosButton(R.string.button_delete)
            .setParams(names as Any)
            .show()
    } // deleteSelected

    //////////////////////////////////////////////////////////////////////
    // MountEventReceiver.SDCardNotifiee interface
    //////////////////////////////////////////////////////////////////////
    override fun cardMounted(nowMounted: Boolean) {
        Log.i(TAG, "cardMounted(%b)", nowMounted)
        // post so other SDCardNotifiee implementations get a chance
        // to process first: avoid race conditions
        post { mkListAdapter() }
    }

    //////////////////////////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////////////////////////
    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any?): Boolean {
        var handled = true
        when (action) {
            DlgDelegate.Action.DELETE_DICT_ACTION -> {
                val names = params[0] as Array<String>
                for (name in names) {
                    val loc = mSelDicts!![name] as DictLoc
                    deleteDict(name, loc)
                }
                clearSelections()
                mkListAdapter()
            }

            DlgDelegate.Action.UPDATE_DICTS_ACTION -> {
                val needUpdates = params[0] as? MutableMap<String, Uri>
                val uris = ArrayList<Uri>()
                val names = ArrayList<String>()
                needUpdates?.map{ entry ->
                    names.add(entry.key)
                    uris.add(entry.value)
                }
                DwnldDelegate.downloadDictsInBack(mActivity, uris.toTypedArray(),
                                                  names.toTypedArray(), this)
            }

            DlgDelegate.Action.MOVE_CONFIRMED -> {
                val selNames = params[0] as Array<String>
                val toLoc = params[1] as DictLoc
                moveDictsWithPermission(selNames, toLoc)
            }
            DlgDelegate.Action.STORAGE_CONFIRMED -> mkListAdapter()
            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    override fun onNegButton(action: DlgDelegate.Action,
                             vararg params: Any?): Boolean
    {
        var handled = true
        when (action) {
            DlgDelegate.Action.STORAGE_CONFIRMED -> mkListAdapter()
            else -> handled = super.onNegButton(action, *params)
        }
        return handled
    }

    private fun itemToRealLoc(item: Int): DictLoc {
        var item = item
        item += DictLoc.INTERNAL.ordinal
        return DictLoc.entries[item]
    }

    private fun deleteDict(dict: String, loc: DictLoc) {
        DictUtils.deleteDict(mActivity, dict, loc)
        DictLangCache.inval(mActivity, dict, loc, false)
    }

    private fun startDownload(isoCode: ISOCode?, name: String) {
        DwnldDelegate.downloadDictInBack(mActivity, isoCode, name, this)
    }

    private fun resetLangs() {
        val langs: MutableSet<String> = HashSet()
        langs.addAll(Arrays.asList(*DictLangCache.listLangs(mActivity)))
        if (mShowRemote && null != mRemoteInfo) {
            langs.addAll(mRemoteInfo!!.keys)
        }
        mLangs = langs.toTypedArray<String>()
        Arrays.sort(mLangs, Collator.getInstance())
    }

    private fun mkListAdapter() {
        resetLangs()
        mAdapter = DictListAdapter(mActivity)
        setListAdapterKeepScroll(mAdapter!!)
        mSelViews = HashMap()
    }

    private fun saveClosed() {
        val asArray = mClosedLangs.toTypedArray<String?>()
        XWPrefs.setClosedLangs(mActivity, asArray)
    }

    private fun clearSelections() {
        if (0 < mSelDicts!!.size) {
            for (name in getSelNames()) {
                if (mSelViews!!.containsKey(name)) {
                    val item = mSelViews!![name]
                    item!!.isSelected = false
                }
            }
            mSelDicts!!.clear()
            mSelViews!!.clear()
        }
    }

    private fun getSelNames(): Array<String>
    {
        return mSelDicts!!.keys.toTypedArray()
    }

    private fun getJoinedSelNames(names: Array<String>): String
    {
        return TextUtils.join(", ", names)
    }

    private fun countSelDicts(): IntArray {
        val results = intArrayOf(0, 0)
        for (obj in mSelDicts!!.values) {
            if (obj is DictLoc) {
                ++results[SEL_LOCAL]
            } else if (obj is AvailDictInfo) {
                ++results[SEL_REMOTE]
            } else {
                Log.d(TAG, "obj is a: $obj")
                Assert.failDbg()
            }
        }
        Log.i(
            TAG, "countSelDicts() => {loc: %d; remote: %d}",
            results[SEL_LOCAL], results[SEL_REMOTE]
        )
        return results
    }

    private fun setTitleBar() {
        val nSels = mSelDicts!!.size
        setTitle( if (0 < nSels) {
                      getString(R.string.sel_items_fmt, nSels)
                  } else {
                      mOrigTitle
                  }!!
        )
    }

    private fun makeDictDirItems(): Array<String?> {
        val showDownload = DictUtils.haveDownloadDir(mActivity)
        val nItems = if (showDownload) 3 else 2
        var nextI = 0
        val items = arrayOfNulls<String>(nItems)
        for (ii in 0..2) {
            val loc = itemToRealLoc(ii)
            if (!showDownload && DictLoc.DOWNLOAD == loc) {
                continue
            }
            items[nextI++] = mLocNames!![loc.ordinal]
        }
        return items
    }

    init {
        mActivity = delegator.getActivity()!!
    }

    override fun curThis(): DictsDelegate {
        return super.curThis() as DictsDelegate
    }

    //////////////////////////////////////////////////////////////////////
    // XWListItem.ExpandedListener interface
    //////////////////////////////////////////////////////////////////////
    override fun expanded(me: XWListItem, expanded: Boolean) {
        val info = me.getCached() as AvailDictInfo
        if (expanded) {
            mExpandedItems!!.add(info) // may already be there
            val view = inflate(R.layout.remote_dict_details) as LinearLayout
            val button = view.findViewById<View>(R.id.download_button) as Button
            button.setOnClickListener {
                DwnldDelegate.downloadDictInBack(
                    mActivity, info.mISOCode,
                    info.m_name,
                    this@DictsDelegate
                )
            }
            val kBytes = (info.m_nBytes + 999) / 1000
            var msg = getString(
                R.string.dict_info_fmt, info.m_nWords,
                kBytes
            )
            if (!TextUtils.isEmpty(info.m_note)) {
                msg += """
                    
                    ${getString(R.string.dict_info_note_fmt, info.m_note)}
                    """.trimIndent()
            }
            val summary = view.findViewById<View>(R.id.details) as TextView
            summary.text = msg
            me.addExpandedView(view)
        } else {
            me.removeExpandedView()
            mExpandedItems!!.remove(info)
        }
    }

    //////////////////////////////////////////////////////////////////////
    // DwnldActivity.DownloadFinishedListener interface
    //////////////////////////////////////////////////////////////////////
    override fun downloadFinished(
        isoCode: ISOCode, name: String,
        success: Boolean
    ) {
        if (success && mShowRemote) {
            mLastLang = isoCode
            mLastDict = name
        }
        if (m_launchedForMissing) {
            post {
                if (success) {
                    val intent = intent
                    if (MultiService.returnOnDownload(
                            mActivity,
                            intent
                        )
                    ) {
                        finish()
                    } else if (mFinishOnName?.equals(name) ?: false) {
                        finish()
                    }
                } else {
                    showToast(R.string.download_failed)
                }
            }
        } else {
            mkListAdapter()
        }
    }

    //////////////////////////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////////////////////////
    override fun itemClicked(clicked: LongClickHandler )
        = Log.i(TAG, "itemClicked not implemented")

    override fun itemToggled(
        toggled: LongClickHandler,
        selected: Boolean
    ) {
        val dictView = toggled as XWListItem
        val lang = dictView.getText()
        if (selected) {
            mSelViews!![lang] = dictView
            mSelDicts!![lang] = dictView.getCached()!!
        } else {
            mSelViews!!.remove(lang)
            mSelDicts!!.remove(lang)
        }
        invalidateOptionsMenuIf()
        setTitleBar()
    }

    override fun getSelected(obj: LongClickHandler): Boolean {
        val dictView = obj as XWListItem
        return mSelDicts!!.containsKey(dictView.getText())
    }

    private class GetDefaultDictTask(
        private val m_context: Context,
        private val m_lc: ISOCode,
        private val m_lstnr: OnGotLcDictListener
    ) : AsyncTask<Void?, Void?, String?>() {
        private var m_langName: String? = null
        override fun doInBackground(vararg unused: Void?): String? {
            // FIXME: this should pass up the language code to retrieve and
            // parse less data
            var name: String? = null
            val proc = listDictsProc(m_lc)
            val conn = NetUtils.makeHttpUpdateConn(m_context, proc)
            if (null != conn) {
                var theOne: JSONObject? = null
                val langName: String? = null
                val json = NetUtils.runConn(conn, JSONObject())
                if (null != json) {
                    try {
                        val obj = JSONObject(json)
                        val langs = obj.optJSONArray("langs")
                        val nLangs = langs.length()
                        for (ii in 0 until nLangs) {
                            val langObj = langs.getJSONObject(ii)
                            val langCode = ISOCode.newIf(langObj.getString("lc"))
                            if (langCode != m_lc) {
                                continue
                            }
                            // we have our language; look for one marked default;
                            // otherwise take the largest.
                            m_langName = langObj.getString("lang")
                            val dicts = langObj.getJSONArray("dicts")
                            val nDicts = dicts.length()
                            var theOneNWords = 0
                            for (jj in 0 until nDicts) {
                                val dict = dicts.getJSONObject(jj)
                                if (dict.optBoolean("isDflt", false)) {
                                    theOne = dict
                                    break
                                } else {
                                    val nWords = dict.getInt("nWords")
                                    if (null == theOne
                                        || nWords > theOneNWords
                                    ) {
                                        theOne = dict
                                        theOneNWords = nWords
                                    }
                                }
                            }
                        }

                        // If we got here and theOne isn't set, there is
                        // no wordlist available for this language. Set
                        // the flag so we don't try again, even though
                        // we've failed.
                        if (null == theOne) {
                            XWPrefs.setPrefsBoolean(
                                m_context,
                                R.string.key_got_langdict,
                                true
                            )
                        }
                    } catch (ex: JSONException) {
                        Log.ex(TAG, ex)
                        theOne = null
                    }
                }
                if (null != theOne) {
                    name = theOne.optString("xwd")
                }
            }
            return name
        }

        override fun onPostExecute(name: String?) {
            m_lstnr.gotDictInfo(null != name, m_lc, name)
        }
    }

    private inner class FetchListTask(context: Context) : AsyncTask<Void?, Void?, Boolean>(),
        DialogInterface.OnCancelListener { // class FetchListTask
        private val mContext: Context
        private val mNeedUpdates:MutableMap<String, Uri> = HashMap()

        init {
            if (null == mLangs) {
                resetLangs()
            }
            mContext = context
            startProgress(R.string.progress_title, R.string.remote_empty, this)
        }

        override fun doInBackground(vararg unused: Void?): Boolean {
            var success = false
            val proc = listDictsProc(null)
            val conn = NetUtils.makeHttpUpdateConn(mContext, proc)
            if (null != conn) {
                val json = NetUtils.runConn(conn, JSONObject())
                if (!isCancelled) {
                    if (null != json) {
                        post { setProgressMsg(R.string.remote_digesting) }
                    }
                    success = digestData(json)
                }
            }
            return success
        }

        override fun onCancelled() {
            mRemoteInfo = null
            mShowRemote = false
        }

        override fun onCancelled(success: Boolean) {
            onCancelled()
        }

        override fun onPostExecute(success: Boolean) {
            if (success) {
                mkListAdapter()
                if (0 < mNeedUpdates.size) {
                    val names = mNeedUpdates.keys
                        .toTypedArray<String>()
                    val joined = TextUtils.join(", ", names)
                    makeConfirmThenBuilder(
                        DlgDelegate.Action.UPDATE_DICTS_ACTION,
                        R.string.update_dicts_fmt, joined
                    )
                        .setPosButton(R.string.button_download)
                        .setParams(mNeedUpdates)
                        .show()
                }
            } else {
                makeOkOnlyBuilder(R.string.remote_no_net).show()
                mCheckbox!!.setChecked(false)
            }
            stopProgress()
        }

        @Throws(JSONException::class)
        private fun parseLangs(langs: JSONArray): MutableSet<String> {
            val closedLangs: MutableSet<String> = HashSet()
            val curLangs: Set<String> = HashSet(Arrays.asList(*mLangs!!))
            val nLangs = langs.length()
            mRemoteInfo = HashMap()
            var ii = 0
            while (!isCancelled && ii < nLangs) {
                val langObj = langs.getJSONObject(ii)
                val isoCode = ISOCode.newIf(langObj.optString("lc", null))
                val urlLangName = langObj.getString("lang")
                var localLangName: String? = null
                if (null != isoCode) {
                    localLangName = DictLangCache.getLangNameForISOCode(mActivity, isoCode)
                }
                if (null == localLangName) {
                    localLangName = urlLangName
                    DictLangCache.setLangNameForISOCode(
                        mContext,
                        isoCode,
                        urlLangName
                    )
                }
                if (null != mFilterLang &&
                    mFilterLang != localLangName
                ) {
                    ++ii
                    continue
                }
                if (!curLangs.contains(localLangName)) {
                    closedLangs.add(localLangName)
                }
                val dicts = langObj.getJSONArray("dicts")
                val nDicts = dicts.length()
                val dictNames = ArrayList<AvailDictInfo>()
                var jj = 0
                while (!isCancelled && jj < nDicts) {
                    val dict = dicts.getJSONObject(jj)
                    var name = dict.getString("xwd")
                    name = DictUtils.removeDictExtn(name)
                    val nBytes = dict.optLong("nBytes", -1)
                    val nWords = dict.optInt("nWords", -1)
                    var note = dict.optString("note")
                    if (0 == note.length) {
                        note = null
                    }
                    val info = AvailDictInfo(
                        name, isoCode, localLangName,
                        nWords, nBytes, note
                    )
                    if (!mQuickFetchMode) {
                        // Check if we have it and it needs an update
                        if (DictLangCache.haveDict(mActivity, isoCode, name)
                            && !DictUtils.dictIsBuiltin(mActivity, name)
                        ) {
                            var matches = true
                            val sums = dict.optJSONArray("md5sums")
                            if (null != sums) {
                                matches = false
                                val curSums = DictLangCache.getDictMD5Sums(mActivity, name)
                                for (curSum in curSums) {
                                    var kk = 0
                                    while (!matches && kk < sums.length()) {
                                        val sum = sums.getString(kk)
                                        matches = sum == curSum
                                        ++kk
                                    }
                                }
                            }
                            if (!matches) {
                                val uri = Utils.makeDictUriFromName(
                                    mActivity,
                                    urlLangName, name
                                )
                                mNeedUpdates[name] = uri
                            }
                        }
                    }
                    dictNames.add(info)
                    ++jj
                }
                if (0 < dictNames.size) {
                    val asArray = dictNames
                        .toTypedArray<AvailDictInfo>()
                    Arrays.sort(asArray)
                    mRemoteInfo!![localLangName] = asArray
                }
                ++ii
            }
            return closedLangs
        }

        private fun digestData(jsonData: String?): Boolean {
            var success = false
            if (null != jsonData) {
                // DictLangCache hits the DB hundreds of times below. Fix!
                Log.w(TAG, "Fix me I'm stupid")
                try {
                    Log.d(TAG, "digestData(%s)", jsonData)
                    val obj = JSONObject(jsonData)
                    val langs = obj.optJSONArray("langs")
                    if (null != langs) {
                        val closedLangs = parseLangs(langs)
                        closedLangs.remove(mFilterLang)
                        mClosedLangs.addAll(closedLangs)
                        success = true
                    }
                } catch (ex: JSONException) {
                    Log.ex(TAG, ex)
                }
            }
            return success
        } // digestData

        /////////////////////////////////////////////////////////////////
        // DialogInterface.OnCancelListener interface
        /////////////////////////////////////////////////////////////////
        override fun onCancel(dialog: DialogInterface) {
            mCheckbox!!.setChecked(false)
            cancel(true)
        }
    }

    companion object {
        private val TAG = DictsDelegate::class.java.getSimpleName()
        private const val REMOTE_SHOW_KEY = "REMOTE_SHOW_KEY"
        private const val REMOTE_INFO_KEY = "REMOTE_INFO_KEY"
        private const val SEL_DICTS_KEY = "SEL_DICTS_KEY"
        private const val DICT_SHOWREMOTE = "do_launch"
        private const val DICT_LANG_EXTRA = "use_lang"
        private const val DICT_NAME_EXTRA = "use_dict"
        const val RESULT_LAST_LANG = "last_lang"
        const val RESULT_LAST_DICT = "last_dict"
        private const val SEL_LOCAL = 0
        private const val SEL_REMOTE = 1
        fun downloadDefaultDict(
            context: Context, isoCode: ISOCode,
            lstnr: OnGotLcDictListener
        ) {
            GetDefaultDictTask(context, isoCode, lstnr).execute()
        }

        private const val FAKE_GROUP = 101
        private fun addItem(menu: Menu, name: String): MenuItem {
            return menu.add(FAKE_GROUP, Menu.NONE, Menu.NONE, name)
        }

        private fun doPopup(
            dlgtor: Delegator, button: View,
            curDict: String, isoCode: ISOCode
        ) {
            val itemData = HashMap<MenuItem, DictAndLoc>()
            val context: Context = dlgtor.getActivity()!!
            val listener = MenuItem.OnMenuItemClickListener { item ->
                val dal = itemData[item]
                val prevKey = keyForLang(isoCode)
                setStringFor(context, prevKey, dal!!.name)
                DictBrowseDelegate.launch(
                    dlgtor, dal.name,
                    dal.loc
                )
                true
            }
            var prevSel = prevSelFor(context, isoCode)
            if (null == prevSel) {
                prevSel = curDict
            }
            val popup = PopupMenu(context, button)
            val menu = popup.menu

            // Add at top but save until have dal info
            val curItem = addItem(
                menu,
                LocUtils.getString(
                    context,
                    R.string.cur_menu_marker_fmt,
                    curDict
                )
            )
            val dals = DictLangCache.getDALsHaveLang(context, isoCode)
            for (dal in dals) {
                val isCur = dal.name == curDict
                val item = if (isCur) curItem else addItem(menu, dal.name)
                item.setOnMenuItemClickListener(listener)
                itemData[item] = dal
                item.setChecked(dal.name == prevSel)
            }
            menu.setGroupCheckable(FAKE_GROUP, true, true)
            popup.show()
        }

        fun handleDictsPopup(
            delegator: Delegator, button: View,
            curDict: String, isoCode: ISOCode
        ): Boolean {
            val nDicts = DictLangCache.getLangCount(delegator.getActivity()!!, isoCode)
            val canHandle = 1 < nDicts
            if (canHandle) {
                doPopup(delegator, button, curDict, isoCode)
            }
            return canHandle
        }

        private fun keyForLang(isoCode: ISOCode): String {
            return String.format("%s:lang=%s", TAG, isoCode)
        }

        fun prevSelFor(context: Context, isoCode: ISOCode): String? {
            val key = keyForLang(isoCode)
            return getStringFor(context!!, key)
        }

        private fun listDictsProc(lc: ISOCode?): String {
            var proc = String.format(
                "listDicts?vc=%d",
                BuildConfig.VERSION_CODE
            )
            if (null != lc) {
                proc += String.format("&lc=%s", lc)
            }
            return proc
        }

        fun start(delegator: Delegator) {
            delegator.addFragment(DictsFrag.newInstance(delegator), null)
        }

        fun downloadForResult(delegator: Delegator,
                              requestCode: RequestCode,
                              lang: Utils.ISOCode) {
            downloadForResult( delegator, requestCode, lang, null )
        }

        fun downloadForResult(delegator: Delegator,
                              requestCode: RequestCode) {
            downloadForResult( delegator, requestCode, null, null )
        }

        @JvmOverloads
        fun downloadForResult(
            delegator: Delegator,
            requestCode: RequestCode,
            isoCode: ISOCode?, name: String?
        ) {
            val bundle = Bundle()
            bundle.putBoolean(DICT_SHOWREMOTE, true)
            if (null != isoCode) {
                bundle.putString(DICT_LANG_EXTRA, isoCode.toString())
            }
            if (null != name) {
                Assert.assertTrue(null != isoCode)
                bundle.putString(DICT_NAME_EXTRA, name)
            }
            delegator.addFragmentForResult(
                DictsFrag.newInstance(delegator),
                bundle, requestCode
            )
        }
    }
}

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
import android.app.Dialog
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.TextUtils
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.View.OnLongClickListener
import android.view.ViewGroup
import android.view.inputmethod.InputMethodManager
import android.widget.ArrayAdapter
import android.widget.BaseAdapter
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ListView
import android.widget.SectionIndexer
import android.widget.Spinner
import android.widget.TableLayout
import android.widget.TextView

import java.io.Serializable
import java.util.Arrays

import org.eehouse.android.xw4.DBUtils.getSerializableFor
import org.eehouse.android.xw4.DBUtils.setSerializableFor
import org.eehouse.android.xw4.DbgUtils.assertOnUIThread
import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener
import org.eehouse.android.xw4.PatTableRow.EnterPressed
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.DictInfo
import org.eehouse.android.xw4.jni.TmpDict
import org.eehouse.android.xw4.jni.TmpDict.PatDesc
import org.eehouse.android.xw4.loc.LocUtils

class DictBrowseDelegate constructor(delegator: Delegator) : DelegateBase(
    delegator, R.layout.dict_browser,
    R.menu.dict_browse_menu
), View.OnClickListener, OnLongClickListener, EnterPressed {
    // Struct to show both what user's configuring AND what's been
    // successfully fed to create the current iterator. The config setting
    // become the filter params when the user presses the Apply Filter button
    // and corrects any tile problems.
    private class DictBrowseState : Serializable {
        var mChosenMin: Int
        var mChosenMax: Int
        var mPassedMin = 0
        var mPassedMax = 0
        var mPos = 0
        var mTop = 0
        val mPats = (0..2).map{PatDesc()}.toTypedArray()
        var mExpanded = false
        var mDelim: String? = null

        init {
            mChosenMin = MIN_LEN
            mChosenMax = MAX_LEN
        }

        fun onFilterAccepted(dict: TmpDict.DictWrapper, delim: String?) {
            mPassedMin = mChosenMin
            mPassedMax = mChosenMax
            mPats.map{it.strPat = dict.tilesToStr(it.tilePat, delim)}
        }

        override fun toString(): String {
            val sb = StringBuilder("{pats:[")
            mPats.map{sb.append("${it},")}
            sb.append("],")
                .append("passedMin: $mPassedMin, ")
                .append("passedMax: $mPassedMax, ")
                .append("chosenMin: $mChosenMin, ")
                .append("chosenMax: $mChosenMax")
            return sb.append("}").toString()
        }
    }

    private val mActivity: Activity
    private var mLang: ISOCode? = null
    private var mName: String? = null
    private var mAboutStr: String? = null
    private var mLoc: DictLoc? = null
    private var mBrowseState: DictBrowseState? = null
    private val mMinAvail = 0
    private var mList: ListView? = null
    private var mDiClosure: TmpDict.IterWrapper? = null
    private var mDict: TmpDict.DictWrapper? = null
    private var mDictInfo: DictInfo? = null
    private val mRows = arrayOf<PatTableRow?>(null, null, null)
    private var mSpinnerMin: Spinner? = null
    private var mSpinnerMax: Spinner? = null
    private var mFilterAlertShown = false
    private var mResetChecker: Runnable? = null

    private inner class DictListAdapter : BaseAdapter(), SectionIndexer {
        private var mPrefixes: Array<String>? = null
        private var mIndices: IntArray? = null
        private val mNWords: Int

        init {
            mNWords = mDiClosure!!.wordCount()
            // Log.d( TAG, "making DictListAdapter; have %d words", m_nWords );
        }

        override fun getItem(position: Int): Any {
            val text = inflate(android.R.layout.simple_list_item_1) as TextView
            text.setOnClickListener(this@DictBrowseDelegate)
            text.setOnLongClickListener(this@DictBrowseDelegate)
            var str = mDiClosure!!.nthWord(position, mBrowseState!!.mDelim)
            if (null != str) {
                if (SHOW_NUM) {
                    str = String.format("%1\$d %2\$s", 1 + position, str)
                }
            } else if (SHOW_NUM) {
                str = String.format("%1\$d <null>", 1 + position)
            }
            if (null != str) {
                text.text = str
            }
            return text
        }

        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            return getItem(position) as View
        }

        override fun getItemId(position: Int): Long {
            return position.toLong()
        }

        override fun getCount(): Int {
            Assert.assertTrueNR(mNWords == mDiClosure!!.wordCount())
            return mNWords
        }

        // SectionIndexer
        override fun getPositionForSection(section: Int): Int {
            var section = section
            if (section >= mIndices!!.size) {
                section = mIndices!!.size - 1
            }
            return mIndices!![section]
        }

        override fun getSectionForPosition(position: Int): Int {
            var section = Arrays.binarySearch(mIndices, position)
            if (section < 0) {
                section *= -1
            }
            if (section >= mIndices!!.size) {
                section = mIndices!!.size - 1
            }
            return section
        }

        override fun getSections(): Array<Any> {
            mPrefixes = mDiClosure!!.getPrefixes()
            mIndices = mDiClosure!!.getIndices()

            return if ( null == mPrefixes ) {
                arrayOf()
            } else {
                mPrefixes as Array<Any>
            }
        }
    }

    override fun init(savedInstanceState: Bundle?) {
        val args = arguments
        val name = args?.getString(DICT_NAME)
        val isCustom = null != args && args.getBoolean(DICT_CUSTOM, false)
        Assert.assertNotNull(name)
        if (null == name) {
            finish()
        } else {
            mAboutStr = getString(R.string.show_note_menu_fmt, name)
            mName = name
            mLoc = DictLoc.entries.toTypedArray().get(args.getInt(DICT_LOC, 0))
            mLang = DictLangCache.getDictISOCode(mActivity, name)
            findTableRows()
            mSpinnerMin = (findViewById(R.id.spinner_min) as LabeledSpinner)
                .getSpinner()
            mSpinnerMax = (findViewById(R.id.spinner_max) as LabeledSpinner)
                .getSpinner()
            loadBrowseState()
            val names = arrayOf(mName)
            val pairs = DictUtils.openDicts(mActivity, names)
            Assert.assertNotNull(mBrowseState)
            mDict = TmpDict.makeDict(pairs.m_bytes[0], mName, pairs.m_paths[0])
            mDictInfo = mDict!!.getInfo(false)
            setTitle(getString(R.string.dict_browse_title_fmt, mName, mDictInfo!!.wordCount))

            val ecl: ExpandChangeListener = object: ExpandChangeListener {
                override fun expandedChanged(nowExpanded: Boolean) {
                    mBrowseState!!.mExpanded = nowExpanded
                    setShowConfig()
                    if (!nowExpanded) {
                        hideSoftKeyboard()
                    }
                }
            }
            val eib = (findViewById(R.id.expander) as ExpandImageButton)
                .setOnExpandChangedListener(ecl)
                .setExpanded(mBrowseState!!.mExpanded)
            val ids = intArrayOf(
                R.id.button_useconfig, R.id.button_addBlank,
                R.id.button_clear
            )
            ids.map{findViewById(it)?.setOnClickListener(this)}

            setShowConfig()
            replaceIter(true)
            if (isCustom) {
                val msg = LocUtils
                    .getString(mActivity, R.string.notagain_custom_xwd_fmt, name)
                makeNotAgainBuilder(R.string.key_na_customXWD, msg)
                    .show()
            }
        }
    } // init

    override fun onPause() {
        scrapeBrowseState()
        storeBrowseState()
        enableResetChecker(false)
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        loadBrowseState()
        setFindPats(mBrowseState!!.mPats)
    }

    override fun makeDialog(alert: DBAlert, vararg params: Any?): Dialog?
    {
        var dialog: Dialog? = null
        val dlgID = alert.dlgID
        when (dlgID) {
            DlgID.CHOOSE_TILES -> {
                val choices = params[0] as Array<ByteArray>
                val indx = params[1] as Int
                val strs = arrayOfNulls<String>(choices.size)
                var ii = 0
                while (ii < choices.size) {
                    strs[ii] = mDict!!.tilesToStr(choices[ii], DELIM)
                    ++ii
                }
                val title = getString(
                    R.string.pick_tiles_title_fmt,
                    mRows[indx]!!.getFieldName()
                )
                val chosen = intArrayOf(0)
                dialog = makeAlertBuilder()
                    .setSingleChoiceItems(strs, chosen[0]) { dialog, which -> chosen[0] = which }
                    .setPositiveButton(
                        android.R.string.ok
                    ) { dialog, which ->
                        if (0 <= chosen[0]) {
                            val sel = chosen[0]
                            useButtonClicked(indx, choices[sel])
                        }
                    }
                    .setTitle(title)
                    .create()
            }

            DlgID.SHOW_TILES -> {
                val info = params[0] as String
                val tilesView = inflate(R.layout.tiles_table)
                addTileRows(tilesView, info)
                val langName = DictLangCache.getLangNameForISOCode(mActivity, mLang!!)
                val title = getString(R.string.show_tiles_title_fmt, langName)
                dialog = makeAlertBuilder()
                    .setView(tilesView)
                    .setPositiveButton(android.R.string.ok, null)
                    .setTitle(title)
                    .create()
            }

            else -> dialog = super.makeDialog(alert, *params)
        }
        return dialog
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        Utils.setItemVisible(
            menu, R.id.dicts_shownote,
            null != desc
        )
        menu.findItem(R.id.dicts_shownote)
            .setTitle(mAboutStr)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        var handled = true
        when (item.itemId) {
            R.id.dicts_showtiles -> showTiles()
            R.id.dicts_showfaq -> showFaq(FAQ_PARAMS)
            R.id.dicts_shownote -> makeOkOnlyBuilder(desc!!)
                .setTitle(mAboutStr!!)
                .show()

            else -> handled = false
        }
        return handled
    }

    //////////////////////////////////////////////////
    // View.OnLongClickListener interface
    //////////////////////////////////////////////////
    override fun onLongClick(view: View): Boolean {
        val success = view is TextView
        if (success) {
            val text = view as TextView
            val word = text.getText().toString()
            Utils.stringToClip(mActivity, word)
            val msg = LocUtils
                .getString(mActivity, R.string.word_to_clip_fmt, word)
            showToast(msg)
        }
        return success
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    override fun onClick(view: View) {
        when (view.id) {
            R.id.button_useconfig -> useButtonClicked()
            R.id.button_addBlank -> addBlankButtonClicked()
            R.id.button_clear -> resetClicked()
            else -> if (view is TextView) {
                val words = arrayOf(view.getText().toString())
                launchLookup(words, mLang, true)
            } else {
                Assert.failDbg()
            }
        }
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    override fun onPosButton(action: Action,
                             vararg params: Any?): Boolean
    {
        var handled = false
        when (action) {
            Action.FINISH_ACTION -> {
                handled = true
                finish()
            }

            Action.SHOW_TILES -> showTiles()
            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    //////////////////////////////////////////////////
    // PatTableRow.EnterPressed
    //////////////////////////////////////////////////
    override fun enterPressed(): Boolean {
        useButtonClicked()
        return true
    }

    private fun scrapeBrowseState() {
        Assert.assertTrueNR(null != mBrowseState)
        mBrowseState!!.mChosenMin = MIN_LEN + mSpinnerMin!!.selectedItemPosition
        mBrowseState!!.mChosenMax = MIN_LEN + mSpinnerMax!!.selectedItemPosition
        if (null != mList) { // there are words? (don't NPE on empty dict)
            mBrowseState!!.mPos = mList!!.firstVisiblePosition
            val view = mList!!.getChildAt(0)
            mBrowseState!!.mTop = view?.top ?: 0
        }

        // Get the strings (not bytes) from the rows
        for (ii in mRows.indices) {
            mRows[ii]!!.getToDesc(mBrowseState!!.mPats[ii])
            // .updateFrom( desc );
        }
    }

    private fun addTileRows(view: View, info: String) {
        val table = view.findViewById<ViewGroup>(R.id.table)
        if (null != table) {
            val tiles = TextUtils.split(info, "\n")
            for (row in tiles) {
                val fields = TextUtils.split(row, "\t")
                if (3 == fields.size) {
                    val rowView = inflate(R.layout.tiles_row) as ViewGroup
                    for (ii in sTileRowIDs.indices) {
                        val tv = rowView.findViewById<View>(sTileRowIDs[ii]) as TextView
                        tv.text = fields[ii]
                    }
                    table.addView(rowView)
                }
            }
        }
    }

    private fun showTiles() {
        val info = mDict!!.getTilesInfo()
        showDialogFragment(DlgID.SHOW_TILES, info)
    }

    private var m_stateKey: String? = null
    private val stateKey: String
        private get() {
            if (null == m_stateKey) {
                m_stateKey = String.format("KEY_%s_%d", mName, mLoc!!.ordinal)
            }
            return m_stateKey!!
        }

    // We'll enable the button as soon as any row gets focus, since once one
    // of them has focus one always will.
    private var mBlankButtonEnabled = false
    private val mFocusGainedProc = Runnable {
        if (!mBlankButtonEnabled) {
            mBlankButtonEnabled = true
            requireViewById(R.id.button_addBlank).setEnabled(true)
        }
    }

    private fun findTableRows() {
        val table = findViewById(R.id.table) as TableLayout
        val count = table.childCount
        var nFound = 0
        var ii = 0
        while (ii < count && nFound < mRows.size) {
            val child = table.getChildAt(ii)
            if (child is PatTableRow) {
                val row = child
                mRows[nFound++] = row
                row.setOnFocusGained(mFocusGainedProc)
                row.setOnEnterPressed(this)
            }
            ++ii
        }
        Assert.assertTrueNR(nFound == mRows.size)
    }

    private fun loadBrowseState() {
        val newState = false
        if (null == mBrowseState) {
            val obj = getSerializableFor(mActivity, stateKey)
            if (null != obj && obj is DictBrowseState) {
                mBrowseState = obj
            }
            if (null == mBrowseState) {
                mBrowseState = DictBrowseState()
            }
        }
        // Log.d( TAG, "loadBrowseState() => %s", m_browseState );
    }

    private fun storeBrowseState() {
        if (null != mBrowseState) {
            setSerializableFor(mActivity, stateKey, mBrowseState)
        }
    }

    private fun useButtonClicked(justFixed: Int = -1, fixedTiles: ByteArray? = null) {
        if (-1 == justFixed) {
            // Hungarian fix: when we're called via button, clear state so we
            // can know later when we have a tile pattern that it came from
            // the user making a choice and we needn't offer it
            // again. Otherwise if more than one of the lines is ambiguous
            // (results in CHOOSE_TILES call) we loop forever.
            scrapeBrowseState()
            for (desc in mBrowseState!!.mPats) {
                desc!!.tilePat = null
            }
        }
        var pending = false
        if (mBrowseState!!.mChosenMin > mBrowseState!!.mChosenMax) {
            pending = true
            makeOkOnlyBuilder(R.string.error_min_gt_max).show()
        }
        val pats = mBrowseState!!.mPats
        var ii = 0
        while (ii < pats!!.size && !pending) {
            val thisPats = pats[ii]
            if (justFixed == ii) {
                Assert.assertTrueNR(null != fixedTiles)
                thisPats!!.tilePat = fixedTiles
            } else if (null == thisPats!!.tilePat) {
                val strPat = thisPats.strPat
                if (null != strPat && 0 < strPat.length) {
                    val choices = mDict!!.strToTiles(strPat)
                    if (null == choices || 0 == choices.size) {
                        val langName = DictLangCache.getLangNameForISOCode(mActivity, mLang!!)
                        makeOkOnlyBuilder(R.string.no_tiles_exist, strPat, langName)
                            .setActionPair(
                                Action.SHOW_TILES,
                                R.string.show_tiles_button
                            )
                            .show()
                        pending = true
                    } else if (1 == choices.size || !mDict!!.hasDuplicates()) {
                        // Pick the shortest option, i.e. when there's a
                        // choice between using one or several tiles to spell
                        // something choose one.
                        thisPats.tilePat = choices[0]
                        for (jj in 1 until choices.size) {
                            val tilePat = choices[jj]
                            if (tilePat.size < thisPats.tilePat!!.size) {
                                thisPats.tilePat = tilePat
                            }
                        }
                    } else {
                        mBrowseState!!.mDelim = DELIM
                        showDialogFragment(DlgID.CHOOSE_TILES, choices as Any, ii)
                        pending = true
                    }
                }
            }
            ++ii
        }
        if (!pending) {
            storeBrowseState()
            replaceIter(false)
            hideSoftKeyboard()
        }
    }

    private fun addBlankButtonClicked() {
        var handled = false
        for (row in mRows) {
            handled = handled || row!!.addBlankToFocussed("_")
        }
    }

    private fun resetClicked() {
        mBrowseState = DictBrowseState()
        storeBrowseState()
        loadBrowseState()
        setFindPats(mBrowseState!!.mPats)
    }

    private fun setShowConfig() {
        val expanded = mBrowseState!!.mExpanded
        requireViewById(R.id.config).visibility =
            if (expanded) View.VISIBLE else View.GONE
        enableResetChecker(expanded)
    }

    private fun setFindPats(descs: Array<PatDesc>?) {
        if (null != descs && descs.size == mRows.size) {
            for (ii in mRows.indices) {
                mRows[ii]!!.setFromDesc(descs[ii])
            }
        }
        setUpSpinners()
    }

    private fun formatPats(pats: Array<PatDesc>?, delim: String?): String {
        Assert.assertTrueNR(null != mDiClosure)
        val strs: MutableList<String?> =
            ArrayList()
        for (ii in pats!!.indices) {
            val desc = pats[ii]
            var str = desc!!.strPat
            if (null == str && (ii == 0 || ii == pats.size - 1)) {
                str = ""
            }
            if (null != str) {
                strs.add(str)
            }
        }
        // Log.d( TAG, "formatPats() => %s", result );
        return TextUtils.join("…", strs)
    }

    private var m_nums: Array<String>? = null
    private fun makeSpinnerAdapter(spinner: Spinner?, curVal: Int) {
        val adapter = ArrayAdapter(
            mActivity,
            android.R.layout.simple_spinner_item,
            m_nums!!
        )
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinner!!.setAdapter(adapter)
        spinner.setSelection(curVal - MIN_LEN)
    }

    private fun setUpSpinners() {
        if (null == m_nums) {
            m_nums = (MIN_LEN .. MAX_LEN).map{"$it"}.toTypedArray()
        }
        makeSpinnerAdapter(mSpinnerMin, mBrowseState!!.mChosenMin)
        makeSpinnerAdapter(mSpinnerMax, mBrowseState!!.mChosenMax)
    }

    private var mDescWrap: Array<String?>? = null
    private val desc: String?
        private get() {
            if (null == mDescWrap) {
                var desc = mDict!!.getDesc()
                if (BuildConfig.NON_RELEASE) {
                    val sums = DictLangCache.getDictMD5Sums(mActivity, mName)
                    if (null != desc) {
                        desc += "\n\n"
                    } else {
                        desc = ""
                    }
                    desc += """
                    md5s: ${sums[0]}
                    ${sums[1]}
                    """.trimIndent()
                }
                mDescWrap = arrayOf(desc)
            }
            return mDescWrap!![0]
        }

    private fun removeList(): FrameLayout {
        mList = null
        val parent = findViewById(R.id.list_container) as FrameLayout
        parent.removeAllViews()
        return parent
    }

    private fun replaceIter(useOldVals: Boolean) {
        Assert.assertNotNull(mBrowseState)
        Assert.assertNotNull(mDict)
        val min =
            if (useOldVals) mBrowseState!!.mPassedMin
            else mBrowseState!!.mChosenMin
        val max =
            if (useOldVals) mBrowseState!!.mPassedMax
            else mBrowseState!!.mChosenMax
        val title = getString(R.string.filter_title_fmt, mName)
        val msg = getString(R.string.filter_progress_fmt,
                            mDictInfo!!.wordCount)

        startProgress(title, msg)
        launch {
            val wrapper = mDict!!.makeDI(mBrowseState!!.mPats, min, max)
            stopProgress()
            wrapper?.let {
                mBrowseState!!.onFilterAccepted(mDict!!, null)
                initList(it)
                setFindPats(mBrowseState!!.mPats)
            } ?: run {
                makeOkOnlyBuilder(R.string.alrt_bad_filter).show()
            }
            newFeatureAlert()
        }
    }

    private fun newFeatureAlert() {
        if (!mFilterAlertShown) {
            mFilterAlertShown = true
            makeNotAgainBuilder(
                R.string.key_na_newFeatureFilter,
                R.string.new_feature_filter
            )
                .setActionPair(Action.SHOW_FAQ, R.string.button_faq)
                .setParams(FAQ_PARAMS as Any)
                .show()
        }
    }

    private fun initList(newIter: TmpDict.IterWrapper) {
        val parent = removeList()
        mList = inflate(R.layout.dict_browser_list) as ListView
        Assert.assertNotNull(mBrowseState)
        Assert.assertNotNull(mDict)
        mDiClosure = newIter
        val dla: DictListAdapter = DictListAdapter()
        mList!!.setAdapter(dla)
        mList!!.isFastScrollEnabled = true
        mList!!.setSelectionFromTop(mBrowseState!!.mPos, mBrowseState!!.mTop)
        parent.addView(mList)
        updateFilterString()
    }

    private fun updateFilterString() {
        val pats = mBrowseState!!.mPats
        Assert.assertNotNull(pats)
        val summary: String
        val pat = formatPats(pats, null)
        val nWords = mDiClosure!!.wordCount()
        val minMax = mDiClosure!!.getMinMax()
        summary = getString(
            R.string.filter_sum_pat_fmt, pat,
            minMax[0], minMax[1],
            nWords
        )
        val tv = findViewById(R.id.filter_summary) as TextView
        tv.text = summary
    }

    private fun hideSoftKeyboard() {
        val hasFocus = mActivity.currentFocus
        if (null != hasFocus) {
            val imm =
                mActivity.getSystemService(Activity.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.hideSoftInputFromWindow(hasFocus.windowToken, 0)
        }
    }

    init {
        mActivity = delegator.getActivity()!!
    }

    private fun enableResetChecker(enable: Boolean) {
        assertOnUIThread()
        if (!enable) {
            mResetChecker = null
        } else if (null == mResetChecker) {
            val handler = Handler(Looper.getMainLooper())
            val resetButton = findViewById(R.id.button_clear) as Button
            mResetChecker = Runnable {
                mResetChecker?.let {
                    val curMin = MIN_LEN + mSpinnerMin!!.selectedItemPosition
                    val curMax = MIN_LEN + mSpinnerMax!!.selectedItemPosition
                    var hasState = curMin != MIN_LEN || curMax != MAX_LEN
                    var ii = 0
                    while (!hasState && ii < mRows.size) {
                        hasState = mRows[ii]!!.hasState()
                        ++ii
                    }
                    resetButton.setEnabled(hasState)
                    handler.postDelayed(it, sResetCheckMS)
                }
            }
            handler.postDelayed(mResetChecker!!, sResetCheckMS)
        }
    }

    companion object {
        private val TAG = DictBrowseDelegate::class.java.getSimpleName()
        private const val DELIM = "."
        private const val SHOW_NUM = false
        private val FAQ_PARAMS = arrayOf("filters", "intro")
        private const val DICT_NAME = "DICT_NAME"
        private const val DICT_LOC = "DICT_LOC"
        private const val DICT_CUSTOM = "DICT_CUSTOM"
        private const val MIN_LEN = 2
        private const val MAX_LEN = 15
        private val sTileRowIDs = intArrayOf(R.id.face, R.id.count, R.id.value)
        private const val sResetCheckMS = 500L
        private fun launch(delegator: Delegator, bundle: Bundle) {
            delegator.addFragment(
                DictBrowseFrag.newInstance(delegator),
                bundle
            )
        }

        @JvmOverloads
        fun launch(
            delegator: Delegator, name: String,
            loc: DictLoc, isCustom: Boolean = false
        ) {
            val bundle = Bundle()
            bundle.putString(DICT_NAME, name)
            bundle.putInt(DICT_LOC, loc.ordinal)
            if (isCustom) {
                bundle.putBoolean(DICT_CUSTOM, true)
            }
            launch(delegator, bundle)
        }

        fun launch(delegator: Delegator, name: String) {
            val loc = DictUtils.getDictLoc(delegator.getActivity()!!, name)
            if (null == loc) {
                Log.w(TAG, "launch(): DictLoc null; try again?")
            } else {
                launch(delegator, name, loc)
            }
        }
    }
}

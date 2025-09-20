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
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.os.Handler
import android.util.AttributeSet
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.view.doOnAttach
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

import java.text.DateFormat
import java.util.Date

import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener
import org.eehouse.android.xw4.SelectableItem.LongClickHandler
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = GameListItem::class.java.simpleName

class GameListItem(private val mContext: Context, aset: AttributeSet?) :
    LinearLayout(mContext, aset),
    View.OnClickListener, LongClickHandler, ExpandChangeListener
{
    private var m_loaded: Boolean
    private var mGR: GameRef? = null
    private var m_hideable: View? = null
    private var mThumbView: ImageView? = null
    private var mThumb: Bitmap? = null
    private var m_name: ExpiringTextView? = null
    private var m_viewUnloaded: TextView? = null
    private var m_viewLoaded: View? = null
    private var m_list: LinearLayout? = null
    private var m_state: TextView? = null
    private var m_modTime: TextView? = null
    private var m_gameTypeImage: ImageView? = null
    private var m_role: TextView? = null

    private var m_haveTurn = false
    private var m_haveTurnLocal = false
    private var m_lastMoveTime: Long
    private var m_expandButton: ExpandImageButton? = null
    private var mHandler: Handler? = null

    private var mSummary: GameSummary? = null
    private var mGi: CurGameInfo? = null

    private var mCb: SelectableItem? = null
    private var mFieldID = 0
    private var mSelected = false
    private val m_dsdel: DrawSelDelegate

    fun getSummary(): GameSummary?
    {
        return mSummary
    }

    fun getGI(): CurGameInfo { return mGi!! }

    fun gr(): GameRef {
        return mGR!!
    }

    fun setField(fieldID: Int) {
        mFieldID = fieldID
        setName()
    }

    fun load(gr: GameRef, cb: SelectableItem, field: Int,
             handler: Handler, selected: Boolean) {
        mGR = gr
        mCb = cb
        mFieldID = field
        mHandler = handler
        setSelected(selected)
        forceReload()
    }

    fun forceReload() {
        setLoaded(false)

        mGR?.let { gr ->
            launch {
                findViewsOnce()
                mGi = gr.getGI()
                // Log.d(TAG, "forceReload(): gi: $mGi")
                gr.getSummary()?.let { summary ->
                    mSummary = summary
                    makeThumbnailIf(summary)
                    summary.setGI(mGi!!)
                    // Log.d(TAG, "got summary: $summary")
                    setData(summary)
                    setLoaded(true)
                }
            }
        }
    }

    fun invalName() {
        setName()
    }

    override fun setSelected(selected: Boolean) {
        // If new value and state not in sync, force change in state
        if (selected != mSelected) {
            toggleSelected()
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        mGR?.let {
            synchronized(s_invalRows) {
                if (s_invalRows.contains(it.gr) ) {
                    forceReload()
                }
            }
        }
    }

    private fun update(
        lastMoveTime: Long, haveTurn: Boolean,
        haveTurnLocal: Boolean
    ) {
        m_lastMoveTime = lastMoveTime
        m_haveTurn = haveTurn
        m_haveTurnLocal = haveTurnLocal
        showHide()
    }

    // View.OnClickListener interface
    override fun onClick(view: View) {
        val id = view.id
        Log.d(TAG, "onClick($id)")
        when (id) {
            R.id.game_view_container -> toggleSelected()
            R.id.right_side, R.id.thumbnail ->
                mCb!!.itemClicked(this@GameListItem)

            else -> Assert.failDbg()
        }
    }

    fun showHaveChat(haveChat: Boolean = true) {
        val resID =
            if ( haveChat) R.drawable.green_chat__gen
            else 0
        findViewById<ImageView>(R.id.has_chat_marker)
            .setImageResource(resID)
    }

    // ExpandImageButton.ExpandChangeListener
    override fun expandedChanged(nowExpanded: Boolean) {
        // Log.d(TAG, "expandedChanged(nowExpanded: $nowExpanded)")
        mGR!!.setCollapsed(!nowExpanded)
        launch {
            mSummary = mGR!!.getSummary()
            showHide()
        }
    }

    private fun findViewsOnce() {
        if ( null == m_hideable ) {
            m_hideable = findViewById<LinearLayout>(R.id.hideable)
            m_name = findViewById<ExpiringTextView>(R.id.game_name)
            m_expandButton = findViewById<ExpandImageButton>(R.id.expander)
            m_expandButton!!.setOnExpandChangedListener(this)
            m_viewUnloaded = findViewById<TextView>(R.id.view_unloaded)
            m_viewLoaded = findViewById(R.id.view_loaded)
            findViewById<View>(R.id.game_view_container).setOnClickListener(this)
            m_list = findViewById<LinearLayout>(R.id.player_list)
            m_state = findViewById<TextView>(R.id.state)
            m_modTime = findViewById<TextView>(R.id.modtime)
            m_gameTypeImage = findViewById<View>(R.id.game_type_marker) as ImageView
            mThumbView = findViewById<ImageView>(R.id.thumbnail)
            mThumbView!!.setOnClickListener(this)
            m_role = findViewById<TextView>(R.id.role)

            findViewById<View>(R.id.right_side).setOnClickListener(this)
        }
    }

    private fun setLoaded(loaded: Boolean) {
        if (loaded != m_loaded) {
            m_loaded = loaded

            if (loaded) {
                // This should be enough to invalidate
                m_viewUnloaded!!.visibility = INVISIBLE
                m_viewLoaded!!.visibility = VISIBLE
            } else {
                m_viewLoaded!!.invalidate()
            }
        }
    }

    private fun showHide() {
        mSummary?.let {
            val expanded = !it.collapsed
            Log.d(TAG, "showHide(): expanded: $expanded")

            m_expandButton!!.setExpanded(expanded)
            m_hideable!!.visibility = if (expanded) VISIBLE else GONE

            val showThumb = (null != mThumb && XWPrefs.getThumbEnabled(mContext)
                                 && expanded)
            Log.d(TAG, "showHide: showThumb: $showThumb")
            if (showThumb) {
                mThumbView!!.visibility = VISIBLE
                mThumbView!!.setImageBitmap(mThumb)
            } else {
                mThumbView!!.visibility = GONE
            }
            m_name!!.setBackgroundColor(android.R.color.transparent)
            mHandler?.let {
                m_name?.setPct(
                    it, m_haveTurn && !expanded,
                    m_haveTurnLocal, m_lastMoveTime
                )
            }
        }
    }

    private fun setName(): String? {
        var state: String? = null // hack to avoid calling summarizeState twice
        mSummary?.let { summary ->
            state = summary.summarizeState(mContext)
            var value = when (mFieldID) {
                R.string.game_summary_field_empty -> null
                R.string.game_summary_field_gameid ->
                    String.format("ID:%X", summary.gameID)

                R.string.game_summary_field_npackets ->
                    String.format("%d", summary.nPacketsPending)

                R.string.game_summary_field_language -> dictLang
                R.string.game_summary_field_opponents -> summary.playerNames(mContext)
                R.string.game_summary_field_state -> state
                R.string.title_addrs_pref -> mGi!!.conTypes?.toString(mContext, false)
                R.string.game_summary_field_created ->
                    sDF.format(Date(summary.created))
                else -> {
                    // Log.d(TAG, "unexpected fieldID $mFieldID)")
                    // Assert.failDbg();
                    null
                } // here
            }
            val name = mGi!!.gameName
            value = if (null != value) {
                LocUtils.getString(
                    mContext, R.string.str_game_name_fmt,
                    name, value
                )
            } else {
                name
            }

            m_name!!.text = value
        } ?: run {Log.d(TAG, "setName(): summary not set")}
        // Log.d(TAG, "setName() => $state")
        return state
    }

    private val dictLang: String
        get() {
            val isoCode = mGi!!.isoCode()!!
            val langName = DictLangCache
                .getLangNameForISOCode(mContext, isoCode)
                ?: LocUtils.getString(
                    mContext, R.string.langUnknownFmt, isoCode
                )
            return langName
        }

    private suspend fun setData(summary: GameSummary) {
        val state = setName()

        m_list!!.removeAllViews()
        var haveATurn = false
        var haveALocalTurn = false
        val isLocal = BooleanArray(1)
        for (ii in 0 until summary.nPlayers) {
            val tmp = LocUtils.inflate(mContext, R.layout.player_list_elem)
                as ExpiringLinearLayout
            var tview = tmp.findViewById<TextView>(R.id.item_name)
            tview.text = summary.summarizePlayer(mContext, ii)
            tview = tmp.findViewById<TextView>(R.id.item_score)
            tview.text = String.format("%d", summary.scores!![ii])
            val thisHasTurn = summary.isNextToPlay(ii, isLocal)
            if (thisHasTurn) {
                haveATurn = true
                if (isLocal[0]) {
                    haveALocalTurn = true
                }
            }
            tmp.setPct(mHandler!!, thisHasTurn, isLocal[0],
                       summary.lastMoveTime.toLong())
            m_list!!.addView(tmp, ii)
        }

        m_state!!.text = state

        var lastMoveTime = summary.lastMoveTime.toLong()
        lastMoveTime *= 1000
        m_modTime!!.text = sDF.format(Date(lastMoveTime))

        setTypeIcon()

        // Let's use the chat-icon space for an ALERT icon when we're
        // quarantined. Not ready for non-debug use though, as it shows up
        // periodically as a false positive. Chat icon wins if both should
        // be displayed, mostly because of the false positives.
        // val resID =
        //     if (summary.hasChat) {
        //         R.drawable.green_chat__gen
        //     } else if (BuildConfig.NON_RELEASE && !mGR!!.safeToOpen()) {
        //         android.R.drawable.stat_sys_warning
        //     } else 0
        // findViewById<ImageView>(R.id.has_chat_marker).setImageResource(resID)
        showHaveChat(summary.hasChat)

        if (BuildConfig.NON_RELEASE) {
            val quarCount = mGR!!.failedOpenCount()
            findViewById<TextView>(R.id.corrupt_count_marker).text =
                // 1 is normal: means the game's open.
                if (quarCount <= 1) "" else "$quarCount"
        }

        if (XWPrefs.moveCountEnabled(mContext)) {
            val tv = findViewById<View>(R.id.n_pending) as TextView
            val nPending = summary.nPacketsPending
            val str = if (nPending == 0) "" else String.format("%d", nPending)
            tv.text = str
        }

        val roleSummary = summary.summarizeRole(mContext, mGR!!)
        m_role!!.visibility = if (null == roleSummary) GONE else VISIBLE
        if (null != roleSummary) {
            m_role!!.text = roleSummary
        }

        findViewById<View>(R.id.dup_tag).visibility =
            if (summary.inDuplicateMode()) VISIBLE else GONE

        Log.d(TAG, "setData(): collapsed: ${summary.collapsed}")
        update(
            summary.lastMoveTime.toLong(), haveATurn, haveALocalTurn
        )
    }

    private fun setTypeIcon() {
        mSummary?.let { // to be safe
            val iconID = if (mSummary!!.isMultiGame
            ) R.drawable.ic_multigame
            else R.drawable.ic_sologame
            m_gameTypeImage!!.setImageResource(iconID)
        }
    }

    private fun toggleSelected() {
        mSelected = !mSelected
        m_dsdel.showSelected(mSelected)
        mCb!!.itemToggled(this, mSelected)

        findViewById<View>(R.id.game_checked).visibility =
            if (mSelected) VISIBLE else GONE
    }

    private suspend fun makeThumbnailIf(summary: GameSummary)
    {
        // Log.d(TAG, "makeThumbnailIf()")
        if (!summary.collapsed && XWPrefs.getThumbEnabled(mContext)) {
            Log.d(TAG, "makeThumbnailIf(): calling getThumbData()")
            mGR!!.getThumbData()?.let {
                BitmapFactory.decodeByteArray(it, 0, it!!.size)?.let {
                    withContext(Dispatchers.Main) {
                        mThumb = it
                        Log.d(TAG, "makeThumbnailIf: created it!!!")
                        showHide()
                    }
                }
            }
        }
    }

    // GameListAdapter.ClickHandler interface
    override fun longClicked() {
        toggleSelected()
    }

    init {
        m_loaded = false
        m_lastMoveTime = 0
        m_dsdel = DrawSelDelegate(this)
        this.doOnAttach {
            forceReload()
        }
    }

    companion object {
        private const val SUMMARY_WAIT_MSECS = 1000

        private val s_invalRows = HashSet<Long>()

        private val sDF: DateFormat = DateFormat
            .getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT)

        fun inval(gr: GameRef) {
            synchronized(s_invalRows) {
                s_invalRows.add(gr.gr)
            }
        }
    }
}

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
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.os.Handler
import android.util.AttributeSet
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView

import java.text.DateFormat
import java.util.Date
import java.util.concurrent.LinkedBlockingQueue

import org.eehouse.android.xw4.ExpandImageButton.ExpandChangeListener
import org.eehouse.android.xw4.SelectableItem.LongClickHandler
import org.eehouse.android.xw4.jni.GameSummary
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = GameListItem::class.java.simpleName

class GameListItem(private val m_context: Context, aset: AttributeSet?) :
    LinearLayout(m_context, aset),
    View.OnClickListener, LongClickHandler, ExpandChangeListener
{
    private var m_activity: Activity? = null
    private var m_loaded: Boolean
    var rowID: Long
        private set
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

    private var m_expanded = false
    private var m_haveTurn = false
    private var m_haveTurnLocal = false
    private var m_lastMoveTime: Long
    private var m_expandButton: ExpandImageButton? = null
    private var m_handler: Handler? = null

    private var mSummary: GameSummary? = null

    private var m_cb: SelectableItem? = null
    private var m_fieldID = 0
    private var m_loadingCount: Int
    private var m_selected = false
    private val m_dsdel: DrawSelDelegate

    fun getSummary(): GameSummary?
    {
        return mSummary;
    }

    private fun init(
        handler: Handler, rowid: Long, fieldID: Int,
        cb: SelectableItem
    ) {
        m_handler = handler
        rowID = rowid
        m_fieldID = fieldID
        m_cb = cb

        forceReload()
    }

    fun forceReload() {
        // DbgUtils.logf( "GameListItem.forceReload: rowid=%d", m_rowid );
        setLoaded(false)
        // Apparently it's impossible to reliably cancel an existing
        // AsyncTask, so let it complete, but drop the results as soon
        // as we're back on the UI thread.
        ++m_loadingCount

        LoadItemTask().start()
    }

    fun invalName() {
        setName()
    }

    override fun setSelected(selected: Boolean) {
        // If new value and state not in sync, force change in state
        if (selected != m_selected) {
            toggleSelected()
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (DBUtils.ROWID_NOTFOUND != rowID) {
            synchronized(s_invalRows) {
                if (s_invalRows.contains(
                        rowID
                    )
                ) {
                    forceReload()
                }
            }
        }
    }

    private fun update(
        expanded: Boolean, lastMoveTime: Long, haveTurn: Boolean,
        haveTurnLocal: Boolean
    ) {
        m_expanded = expanded
        m_lastMoveTime = lastMoveTime
        m_haveTurn = haveTurn
        m_haveTurnLocal = haveTurnLocal
        showHide()
    }

    // View.OnClickListener interface
    override fun onClick(view: View) {
        val id = view.id
        when (id) {
            R.id.game_view_container -> toggleSelected()
            R.id.right_side, R.id.thumbnail -> if (null != mSummary) {
                m_cb!!.itemClicked(this@GameListItem, mSummary)
            }

            else -> Assert.failDbg()
        }
    }

    // ExpandImageButton.ExpandChangeListener
    override fun expandedChanged(nowExpanded: Boolean) {
        m_expanded = nowExpanded
        DBUtils.setExpanded(m_context, rowID, m_expanded)

        makeThumbnailIf(m_expanded)

        showHide()
    }

    private fun findViews() {
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
        m_expandButton!!.setExpanded(m_expanded)
        m_hideable!!.visibility = if (m_expanded) VISIBLE else GONE

        val showThumb = (null != mThumb && XWPrefs.getThumbEnabled(m_context)
                && m_expanded)
        if (showThumb) {
            mThumbView!!.visibility = VISIBLE
            mThumbView!!.setImageBitmap(mThumb)
        } else {
            mThumbView!!.visibility = GONE
        }

        m_name!!.setBackgroundColor(android.R.color.transparent)
        m_name!!.setPct(
            m_handler!!, m_haveTurn && !m_expanded,
            m_haveTurnLocal, m_lastMoveTime
        )
    }

    private fun setName(): String? {
        var state: String? = null // hack to avoid calling summarizeState twice
        if (null != mSummary) {
            val summary = mSummary!!
            state = summary.summarizeState(m_context)
            var value = when (m_fieldID) {
                R.string.game_summary_field_empty -> null
                R.string.game_summary_field_gameid ->
                    String.format("ID:%X", summary.gameID)

                R.string.game_summary_field_rowid -> String.format("%d", rowID)
                R.string.game_summary_field_npackets ->
                    String.format("%d", summary.nPacketsPending)

                R.string.game_summary_field_language -> dictLang
                R.string.game_summary_field_opponents -> summary.playerNames(m_context)
                R.string.game_summary_field_state -> state
                R.string.title_addrs_pref -> summary.conTypes!!.toString(m_context, false)
                R.string.game_summary_field_created ->
                    sDF.format(Date(summary.created))
                else -> {Assert.failDbg(); null}
            }
            val name = GameUtils.getName(m_context, rowID)
            value = if (null != value) {
                LocUtils.getString(
                    m_context, R.string.str_game_name_fmt,
                    name, value
                )
            } else {
                name
            }

            m_name!!.text = value
        }
        return state
    }

    private val dictLang: String
        get() {
            var langName = DictLangCache
                .getLangNameForISOCode(m_context, mSummary!!.isoCode)
            if (null == langName) {
                langName = LocUtils.getString(
                    m_context, R.string.langUnknownFmt,
                    mSummary!!.isoCode
                )
            }
            return langName
        }

    private fun setData(summary: GameSummary?, expanded: Boolean) {
        if (null != summary) {
            val state = setName()

            m_list!!.removeAllViews()
            var haveATurn = false
            var haveALocalTurn = false
            val isLocal = BooleanArray(1)
            for (ii in 0 until summary.nPlayers) {
                val tmp = LocUtils.inflate(m_context, R.layout.player_list_elem)
                    as ExpiringLinearLayout
                var tview = tmp.findViewById<TextView>(R.id.item_name)
                tview.text = summary.summarizePlayer(m_context, rowID, ii)
                tview = tmp.findViewById<TextView>(R.id.item_score)
                tview.text = String.format("%d", summary.scores!![ii])
                val thisHasTurn = summary.isNextToPlay(ii, isLocal)
                if (thisHasTurn) {
                    haveATurn = true
                    if (isLocal[0]) {
                        haveALocalTurn = true
                    }
                }
                tmp.setPct(m_handler!!, thisHasTurn, isLocal[0],
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
            var resID = 0
            if (summary.isMultiGame) {
                val flags = DBUtils.getMsgFlags(m_context, rowID)
                if (0 != (flags and GameSummary.MSG_FLAGS_CHAT)) {
                    resID = R.drawable.green_chat__gen
                }
            }
            if (0 == resID && BuildConfig.NON_RELEASE
                && !Quarantine.safeToOpen(rowID)
            ) {
                resID = android.R.drawable.stat_sys_warning
            }
            // Setting to 0 clears, which we want
            val iv = findViewById<View>(R.id.has_chat_marker) as ImageView
            iv.setImageResource(resID)
            if (BuildConfig.NON_RELEASE) {
                val quarCount = Quarantine.getCount(rowID)
                (findViewById<View>(R.id.corrupt_count_marker) as TextView).text =
                    if (0 == quarCount) "" else "" + quarCount
            }

            if (XWPrefs.moveCountEnabled(m_context)) {
                val tv = findViewById<View>(R.id.n_pending) as TextView
                val nPending = summary.nPacketsPending
                val str = if (nPending == 0) "" else String.format("%d", nPending)
                tv.text = str
            }

            val roleSummary = summary.summarizeRole(m_context, rowID)
            m_role!!.visibility = if (null == roleSummary) GONE else VISIBLE
            if (null != roleSummary) {
                m_role!!.text = roleSummary
            }

            findViewById<View>(R.id.dup_tag).visibility =
                if (summary.inDuplicateMode()) VISIBLE else GONE

            update(
                expanded, summary.lastMoveTime.toLong(), haveATurn,
                haveALocalTurn
            )
        }
    }

    private fun setTypeIcon() {
        if (null != mSummary) { // to be safe
            val iconID = if (mSummary!!.isMultiGame
            ) R.drawable.ic_multigame
            else R.drawable.ic_sologame
            m_gameTypeImage!!.setImageResource(iconID)
        }
    }

    private fun toggleSelected() {
        m_selected = !m_selected
        m_dsdel.showSelected(m_selected)
        m_cb!!.itemToggled(this, m_selected)

        findViewById<View>(R.id.game_checked).visibility = if (m_selected) VISIBLE else GONE
    }

    private fun makeThumbnailIf(expanded: Boolean) {
        if (expanded && null != m_activity && XWPrefs.getThumbEnabled(m_context)) {
            enqueueGetThumbnail(this, rowID)
        }
    }

    private inner class LoadItemTask() : Thread() {
        override fun run() {
            val summary = GameUtils.getSummary(m_context, rowID, SUMMARY_WAIT_MSECS.toLong())

            if (0 == --m_loadingCount) {
                mSummary = summary

                m_activity!!.runOnUiThread {
                    val expanded = DBUtils.getExpanded(m_context, rowID)
                    makeThumbnailIf(expanded)

                    setData(summary, expanded)
                    setLoaded(null != mSummary)
                    if (null == summary) {
                        m_viewUnloaded!!
                            .setText(LocUtils.getString(m_context, R.string.summary_busy))
                    }
                    synchronized(s_invalRows) {
                        s_invalRows.remove(rowID)
                    }
                }
            }
        }
    } // class LoadItemTask

    // GameListAdapter.ClickHandler interface
    override fun longClicked() {
        toggleSelected()
    }

    private class ThumbQueueElem(var m_item: GameListItem, var m_rowid: Long)

    init {
        if (m_context is Activity) {
            m_activity = m_context
        }
        m_loaded = false
        rowID = DBUtils.ROWID_NOTFOUND
        m_lastMoveTime = 0
        m_loadingCount = 0
        m_dsdel = DrawSelDelegate(this)
    }

    companion object {
        private const val SUMMARY_WAIT_MSECS = 1000

        private val s_invalRows = HashSet<Long>()

        private val sDF: DateFormat = DateFormat
            .getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT)

        @JvmStatic
        fun makeForRow(
            context: Context, convertView: View?,
            rowid: Long, handler: Handler,
            fieldID: Int, cb: SelectableItem
        ): GameListItem {
            val result: GameListItem
            if (null != convertView && convertView is GameListItem) {
                result = convertView
            } else {
                result = LocUtils.inflate(context,R.layout.game_list_item)
                    as GameListItem
                result.findViews()
            }
            result.init(handler, rowid, fieldID, cb)
            return result
        }

        @JvmStatic
        fun inval(rowid: Long) {
            synchronized(s_invalRows) {
                s_invalRows.add(rowid)
            }
            // Log.d( TAG, "GameListItem.inval(rowid=%d)", rowid );
        }

        private val s_queue = LinkedBlockingQueue<ThumbQueueElem>()
        private var s_thumbThread: Thread? = null

        private fun enqueueGetThumbnail(item: GameListItem, rowid: Long) {
            s_queue.add(ThumbQueueElem(item, rowid))

            synchronized(GameListItem::class.java) {
                if (null == s_thumbThread) {
                    s_thumbThread = makeThumbThread()
                    s_thumbThread!!.start()
                }
            }
        }

        private fun makeThumbThread(): Thread {
            return Thread {
                while (true) {
                    val elem: ThumbQueueElem
                    try {
                        elem = s_queue.take()
                    } catch (ie: InterruptedException) {
                        Log.w(TAG, "interrupted; killing s_thumbThread")
                        break
                    }
                    val activity = elem.m_item.m_activity
                    val rowid = elem.m_rowid
                    var thumb = DBUtils.getThumbnail(activity!!, rowid)
                    if (null == thumb) {
                        // loadMakeBitmap puts in DB
                        thumb = GameUtils.loadMakeBitmap(activity, rowid)
                    }

                    if (null != thumb) {
                        val fThumb: Bitmap = thumb
                        activity.runOnUiThread {
                            val item = elem.m_item
                            item.mThumb = fThumb
                            item.showHide()
                        }
                    }
                }
            }
        }
    }
}

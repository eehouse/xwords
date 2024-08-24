/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2011 by Eric House (xwords@eehouse.org).  All rights
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
import android.os.Build
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.text.format.DateUtils
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ScrollView
import android.widget.TableLayout
import android.widget.TableRow
import android.widget.TextView

import java.text.DateFormat

import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.jni.JNIThread

class ChatDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.chat, R.menu.chat_menu) {
    private var m_rowid: Long = 0
    private var m_curPlayer = 0
    private var m_names: Array<String>? = null
    private val m_activity: Activity
    private var m_edit: EditText? = null
    private var m_layout: TableLayout? = null
    private var m_scroll: ScrollView? = null

    init {
        m_activity = delegator.getActivity()!!
    }

    override fun init(savedInstanceState: Bundle?) {
        m_edit = findViewById(R.id.chat_edit) as EditText
        m_edit!!.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(s: Editable) {
                invalidateOptionsMenuIf()
            }

            override fun beforeTextChanged(
                s: CharSequence, st: Int,
                cnt: Int, a: Int
            ) {
            }

            override fun onTextChanged(
                s: CharSequence, start: Int,
                before: Int, count: Int
            ) {
            }
        })
        val args = arguments!!
        m_rowid = args.getLong(GameUtils.INTENT_KEY_ROWID, -1)
        m_curPlayer = args.getInt(INTENT_KEY_PLAYER, -1)
        m_names = args.getStringArray(INTENT_KEY_NAMES)
        val locals = args.getBooleanArray(INTENT_KEY_LOCS)
        m_scroll = findViewById(R.id.scroll) as ScrollView
        m_layout = findViewById(R.id.chat_history) as TableLayout

        m_layout!!.addOnLayoutChangeListener { vv, ll, tt, rr, bb, ol, ot, or, ob -> scrollDown() }
        val pairs: ArrayList<DBUtils.HistoryPair> = DBUtils.getChatHistory(m_activity, m_rowid, locals!!)
        for (pair in pairs) {
                addRow(pair.msg, pair.playerIndx, pair.ts.toLong())
        }
        val title = getString(
            R.string.chat_title_fmt,
            GameUtils.getName(m_activity, m_rowid)
        )
        setTitle(title)
    } // init

    override fun onResume() {
        super.onResume()
        s_visibleThis = this
        val startAndEnd = IntArray(2)
        val curMsg = DBUtils.getCurChat(
            m_activity, m_rowid,
            m_curPlayer, startAndEnd
        )
        if (null != curMsg && 0 < curMsg.length) {
            m_edit!!.setText(curMsg)
            m_edit!!.setSelection(startAndEnd[0], startAndEnd[1])
        }
    }

    override fun onPause() {
        s_visibleThis = null
        val curText = m_edit!!.getText().toString()
        DBUtils.setCurChat(
            m_activity, m_rowid, m_curPlayer, curText,
            m_edit!!.selectionStart,
            m_edit!!.selectionEnd
        )
        super.onPause()
    }

    private fun addRow(msg: String, playerIndx: Int, tsSeconds: Long) {
        val row = inflate(R.layout.chat_row) as TableRow
        if (m_curPlayer == playerIndx) {
            row.setBackgroundColor(-0xdfdfe0)
        }
        var view = row.findViewById<View>(R.id.chat_row_text) as TextView
        view.text = msg
        view = row.findViewById<View>(R.id.chat_row_name) as TextView
        val name =
            if (0 <= playerIndx && playerIndx < m_names!!.size) m_names!![playerIndx]
            else "<???>"
        view.text = getString(R.string.chat_sender_fmt, name)
        if (tsSeconds > 0) {
            val now = 1000L * Utils.getCurSeconds()
            val str = DateUtils
                .formatSameDayTime(
                    1000L * tsSeconds, now, DateFormat.MEDIUM,
                    DateFormat.MEDIUM
                )
                .toString()
            (row.findViewById<View>(R.id.chat_row_time) as TextView).text = str
        }
        m_layout!!.addView(row)
        scrollDown()
    }

    private fun scrollDown()
        = m_scroll!!.post { m_scroll!!.fullScroll(View.FOCUS_DOWN) }

    private fun handleSend() {
        JNIThread.getRetained(m_rowid).use { thread ->
            if (null != thread) {
                val edit = m_edit!!
                val text = edit.getText().toString()
                thread.sendChat(text)
                val ts = Utils.getCurSeconds()
                DBUtils.appendChatHistory(m_activity, m_rowid, text, m_curPlayer, ts)
                addRow(text, m_curPlayer, ts.toInt().toLong())
                edit.setText(null)
            } else {
                Log.e(TAG, "null thread; unable to send chat")
            }
        }
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        val text = m_edit!!.getText().toString()
        val haveText = 0 < text.length
        Utils.setItemVisible(menu, R.id.chat_menu_send, haveText)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        var handled = true
        when (item.itemId) {
            R.id.chat_menu_clear -> makeConfirmThenBuilder(
                Action.CLEAR_ACTION,
                R.string.confirm_clear_chat
            )
                .show()

            R.id.chat_menu_send -> handleSend()
            else -> handled = false
        }
        return handled
    }

    override fun onPosButton(action: Action, vararg params: Any?): Boolean {
        var handled = true
        when (action) {
            Action.CLEAR_ACTION -> {
                DBUtils.clearChatHistory(m_activity, m_rowid)
                val layout = findViewById(R.id.chat_history) as TableLayout
                layout.removeAllViews()
            }

            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    companion object {
        private val TAG = ChatDelegate::class.java.getSimpleName()
        private const val INTENT_KEY_PLAYER = "intent_key_player"
        private const val INTENT_KEY_NAMES = "intent_key_names"
        private const val INTENT_KEY_LOCS = "intent_key_locs"
        private var s_visibleThis: ChatDelegate? = null
        fun append(rowid: Long, msg: String, fromIndx: Int, tsSeconds: Long): Boolean {
            val handled = (null != s_visibleThis
                    && s_visibleThis!!.m_rowid == rowid)
            if (handled) {
                s_visibleThis!!.addRow(msg, fromIndx, tsSeconds)
                Utils.playNotificationSound(s_visibleThis!!.m_activity)
            }
            return handled
        }

        fun start(
            delegator: Delegator,
            rowID: Long, curPlayer: Int,
            names: Array<String>?, locs: BooleanArray?
        ) {
            Assert.assertFalse(-1 == curPlayer)
            val bundle = Bundle()
				.putLongAnd(GameUtils.INTENT_KEY_ROWID, rowID)
				.putIntAnd(INTENT_KEY_PLAYER, curPlayer)
				.putStringArrayAnd(INTENT_KEY_NAMES, names)
				.putBooleanArrayAnd(INTENT_KEY_LOCS, locs)
            delegator.addFragment(ChatFrag.newInstance(delegator), bundle)
        }
    }
}

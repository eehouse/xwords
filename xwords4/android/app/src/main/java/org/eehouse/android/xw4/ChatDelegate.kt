/*
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
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.text.format.DateUtils
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

import java.lang.ref.WeakReference
import java.text.DateFormat

import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.JNIThread

class ChatDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.chat, R.menu.chat_menu) {
    private var mGR: GameRef? = null
    private var mCurPlayer = 0
    private var mNames: Array<String>? = null
    private val mActivity: Activity
    private var mEdit: EditText? = null
    private var mLayout: RecyclerView? = null
    private var mLocals: BooleanArray? = null
    private var mAdapter: ChatViewAdapter? = null

    init {
        mActivity = delegator.getActivity()!!
    }

    override fun init(savedInstanceState: Bundle?) {
        mEdit = findViewById(R.id.chat_edit) as EditText
        mEdit!!.addTextChangedListener(object : TextWatcher {
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
        mGR = GameRef(args.getLong(GameUtils.INTENT_KEY_GAMEREF))
        mCurPlayer = args.getInt(INTENT_KEY_PLAYER, -1)
        mNames = args.getStringArray(INTENT_KEY_NAMES)
        mLocals = args.getBooleanArray(INTENT_KEY_LOCS)

        // val title = getString(
        //     R.string.chat_title_fmt,
        //     GameUtils.getName(mActivity, mRowid)
        // )
        setTitle("fixme")

        initMsgView()
    } // init

    private fun initMsgView()
    {
        if ( null == mLayout ) {
            mLayout = findViewById(R.id.history) as RecyclerView
            mLayout?.let { layout ->
                Log.d(TAG, "initMsgView(): got layout")
                layout.setLayoutManager(GridLayoutManager(mActivity, 1/*3*/))
                Utils.launch(Dispatchers.IO) {
                    val count = mGR!!.getChatCount()
                    withContext(Dispatchers.Main) {
                        Log.d(TAG, "count: $count")
                        mAdapter = ChatViewAdapter(count)
                        layout.setAdapter(mAdapter)
                        layout.addOnLayoutChangeListener { vv, ll, tt, rr, bb, ol, ot, or, ob -> scrollDown() }
                    }
                }
            }
            scrollDown()
        }
    }

    fun reload() {
        mLayout = null
        initMsgView()
    }

    override fun onResume() {
        Log.d(TAG, "onResume()")
        super.onResume()
        s_visibleThis = WeakReference<ChatDelegate>(this)
        initMsgView()
    }

    override fun onPause() {
        s_visibleThis = null
        super.onPause()
    }

    private fun scrollDown() {
        mAdapter?.let { adapter ->
            mLayout?.scrollToPosition(adapter.getItemCount()-1)
        }
    }

    private fun handleSend() {
        mEdit!!.let { edit ->
            val msg = edit.getText().toString()
            mGR!!.sendChat( msg )
            edit.setText(null)
            reload()
        }
    }

    override fun onPrepareOptionsMenu(menu: Menu): Boolean {
        val text = mEdit!!.getText().toString()
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
        val handled =
            when (action) {
                Action.CLEAR_ACTION -> {
                    mGR!!.deleteChats()
                    reload()
                    true
                }
                else -> super.onPosButton(action, *params)
            }
        return handled
    }

    inner class ChatViewHolder(view: View): RecyclerView.ViewHolder(view) {
    }

    inner class ChatViewAdapter(private val mCount: Int)
        : RecyclerView.Adapter<ChatViewHolder>()
    {
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int)
            : ChatViewHolder
        {
            val row = inflate(R.layout.chat_row)
            return ChatViewHolder(row)
        }

        override fun onBindViewHolder(holder: ChatViewHolder,
                                      position: Int)
        {
            val row = holder.itemView as ViewGroup
            Utils.launch(Dispatchers.IO) {
                val from = IntArray(1)
                val ts = IntArray(1)
                val msg = mGR!!.getNthChat( position, from, ts )
                withContext(Dispatchers.Main) {
                    fillRow(row, msg, from[0], ts[0])
                }
            }
        }

        override fun getItemCount(): Int {
            return mCount
        }

        private fun fillRow(row: ViewGroup, msg: String, playerIndx: Int,
                            timestamp: Int )
        {
            row.findViewById<TextView>(R.id.chat_row_text).text = msg

            if (mCurPlayer == playerIndx) {
                row.setBackgroundColor(-0XDFDFE0)
            }
            val name =
                if (0 <= playerIndx && playerIndx < mNames!!.size) mNames!![playerIndx]
                else "<???>"
            row.findViewById<TextView>(R.id.chat_row_name)
                .text = getString(R.string.chat_sender_fmt, name)

            val tsSeconds = timestamp.toLong()
            if (tsSeconds > 0) {
                val now = 1000L * Utils.getCurSeconds()
                val str = DateUtils
                    .formatSameDayTime(
                        1000L * tsSeconds, now, DateFormat.MEDIUM,
                        DateFormat.MEDIUM
                    )
                    .toString()
                row.findViewById<TextView>(R.id.chat_row_time).text = str
            }
        }
    }

    companion object {
        private val TAG = ChatDelegate::class.java.getSimpleName()
        private const val INTENT_KEY_PLAYER = "intent_key_player"
        private const val INTENT_KEY_NAMES = "intent_key_names"
        private const val INTENT_KEY_LOCS = "intent_key_locs"
        private var s_visibleThis: WeakReference<ChatDelegate>? = null

        fun reload(): Boolean {
            return s_visibleThis?.get()?.let {
                Utils.playNotificationSound(it.mActivity)
                it.reload()
                true
            } ?: false
        }

        fun start(
            delegator: Delegator,
            gr: GameRef, curPlayer: Int,
            names: Array<String>?, locs: BooleanArray?
        ) {
            Assert.assertFalse(-1 == curPlayer)
            val bundle = Bundle()
				.putLongAnd(GameUtils.INTENT_KEY_GAMEREF, gr.gr)
				.putIntAnd(INTENT_KEY_PLAYER, curPlayer)
				.putStringArrayAnd(INTENT_KEY_NAMES, names)
				.putBooleanArrayAnd(INTENT_KEY_LOCS, locs)
            delegator.addFragment(ChatFrag.newInstance(delegator), bundle)
        }
    }
}

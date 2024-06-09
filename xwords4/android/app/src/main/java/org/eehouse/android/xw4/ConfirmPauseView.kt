/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2019 - 2024 by Eric House (xwords@eehouse.org).  All
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

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemSelectedListener
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView


// Edit text should start out empty
class ConfirmPauseView(context: Context, aset: AttributeSet?) :
    LinearLayout(context, aset),
    View.OnClickListener, OnItemSelectedListener, EditWClear.TextWatcher
{
    private val TAG = ConfirmPauseView::class.java.simpleName
    private val PAUSE_MSGS_KEY = TAG + "/pause_msgs"
    private val UNPAUSE_MSGS_KEY = TAG + "/unpause_msgs"

    private var mIsPause: Boolean? = null
    private var mInflateFinished = false
    private var mInited = false
    private var mSavedMsgs: HashSet<String>? = null
    private var mForgetButton: Button? = null
    private var mRememberButton: Button? = null
    private var mSpinner: Spinner? = null
    private var mMsgEdit: EditWClear? = null

    override fun onFinishInflate() {
        mInflateFinished = true
        initIfReady()
    }

    private fun initIfReady() {
        if (!mInited && mInflateFinished && null != mIsPause) {
            mInited = true

            val context = context

            val id = if (mIsPause!!) R.string.pause_expl else R.string.unpause_expl
            (findViewById<View>(R.id.confirm_pause_expl) as TextView)
                .setText(id)

            mForgetButton = findViewById<View>(R.id.pause_forget_msg) as Button
            mForgetButton!!.setOnClickListener(this)
            mRememberButton = findViewById<View>(R.id.pause_save_msg) as Button
            mRememberButton!!.setOnClickListener(this)
            mSpinner = findViewById<View>(R.id.saved_msgs) as Spinner
            mMsgEdit = findViewById<View>(R.id.msg_edit) as EditWClear
            mMsgEdit!!.addTextChangedListener(this)

            val key = if (mIsPause!!) PAUSE_MSGS_KEY else UNPAUSE_MSGS_KEY
            mSavedMsgs = DBUtils.getSerializableFor(context, key) as HashSet<String>?
            if (null == mSavedMsgs) {
                mSavedMsgs = HashSet()
            }

            populateSpinner()
            mSpinner!!.onItemSelectedListener = this
            msg = ""
            // onTextChanged( "" );
        }
    }

    private fun populateSpinner() {
        val adapter =
            ArrayAdapter<String>(context, android.R.layout.simple_spinner_item)
        for (msg in mSavedMsgs!!) {
            adapter.add(msg)
        }
        mSpinner!!.adapter = adapter
    }

    override fun onItemSelected(
        parent: AdapterView<*>, spinner: View,
        position: Int, id: Long
    ) {
        val msg = parent.adapter.getItem(position) as String
        this.msg = msg
        onTextChanged(msg)
    }

    override fun onNothingSelected(p: AdapterView<*>?) {}

    override fun onClick(view: View) {
        Log.d(TAG, "onClick() called")
        val msg = msg
        if (view === mRememberButton && 0 < msg.length) {
            mSavedMsgs!!.add(msg)
        } else if (view === mForgetButton) {
            mSavedMsgs!!.remove(msg)
            this.msg = ""
        } else {
            Assert.assertFalse(BuildConfig.DEBUG)
        }
        val key = if (mIsPause!!) PAUSE_MSGS_KEY else UNPAUSE_MSGS_KEY
        DBUtils.setSerializableFor(context, key, mSavedMsgs)
        populateSpinner()
    }

    // from EditWClear.TextWatcher
    override fun onTextChanged(msg: String) {
        Log.d(TAG, "onTextChanged(%s)", msg)
        val hasText = 0 < msg.length
        val matches = mSavedMsgs!!.contains(msg)
        mForgetButton!!.isEnabled = hasText && matches
        mRememberButton!!.isEnabled = hasText && !matches
    }

    fun setIsPause(isPause: Boolean): ConfirmPauseView {
        mIsPause = isPause
        initIfReady()
        return this
    }

    var msg: String
        get() = mMsgEdit!!.text.toString()
        private set(msg) {
            mMsgEdit!!.setText(msg)
        }
}

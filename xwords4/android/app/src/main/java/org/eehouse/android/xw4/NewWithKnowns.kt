/*
 * Copyright 2020 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context
import android.text.TextUtils
import android.util.AttributeSet
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemSelectedListener
import android.widget.ArrayAdapter
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.Spinner
import android.widget.TextView

import org.eehouse.android.xw4.jni.Device
import org.eehouse.android.xw4.jni.Knowns
import org.eehouse.android.xw4.loc.LocUtils

class NewWithKnowns(cx: Context, aset: AttributeSet?) :
    LinearLayout(cx, aset), OnItemSelectedListener,
    RadioGroup.OnCheckedChangeListener
{
    interface ButtonChangeListener {
        fun onNewButtonText(txt: String)
    }

    interface ButtonCallbacks {
        fun onUseKnown(knownName: String, gameName: String)
        fun onStartGame(gameName: String, solo: Boolean, configFirst: Boolean)
    }

    private var mListener: ButtonChangeListener? = null
    private var mCurKnown: String? = null
    private var mCurRadio = 0
    private var mNamesSpinner: Spinner? = null
    private var mStandalone = false
    private var mGameName: String? = null

    fun setCallback(listener: ButtonChangeListener): NewWithKnowns {
        mListener = listener
        return this
    }

    fun configure(standalone: Boolean, gameName: String?) {
        mStandalone = standalone
        mGameName = gameName

        launchWhenStarted {
            val standalone = mStandalone
            val gameName = mGameName
            val context = context
            val hasKnowns = !standalone && Knowns.hasKnownPlayers()
            val toHide: IntArray
            if (hasKnowns) {
                val knowns = Knowns.getPlayers()
                mCurKnown = DBUtils.getStringFor(
                    context, KP_NAME_KEY,
                    knowns!![0]
                )
                val adapter = ArrayAdapter(
                    context,
                    android.R.layout.simple_spinner_item,
                    knowns
                )
                adapter.setDropDownViewResource(
                    android.R.layout
                        .simple_spinner_dropdown_item
                )
                mNamesSpinner = findViewById<Spinner>(R.id.names)
                mNamesSpinner?.let {
                    it.adapter = adapter
                    it.onItemSelectedListener = this@NewWithKnowns
                    Assert.assertTrueNR(!TextUtils.isEmpty(mCurKnown))
                    for (ii in knowns.indices) {
                        if (knowns[ii] == mCurKnown) {
                            it.setSelection(ii)
                            break
                        }
                    }
                }
                toHide = intArrayOf(
                    R.id.radio_default, R.id.choose_expl_default,
                )
            } else {
                toHide = intArrayOf(
                    R.id.radio_unknown,
                    R.id.choose_expl_new,
                    R.id.radio_known,
                    R.id.names,
                    R.id.expl_known,
                )

                val tv = findViewById<View>(R.id.choose_expl_default) as TextView
                val id = if (standalone) R.string.choose_expl_default_solo
                else R.string.choose_expl_default_net
                tv.text = LocUtils.getString(context, id)
            }

            toHide.map{findViewById<View>(it).visibility = GONE}

            findViewById<EditWClear>(R.id.name_edit).setText(gameName)

            val group = findViewById<RadioGroup>(R.id.group)
            group.setOnCheckedChangeListener(this@NewWithKnowns)

            // Get the id of the radio button used last time. Since views' IDs can
            // change from one build to another, double-check that it's in the set
            // of known possible values, and if not don't try to use it.
            val key = if (standalone) KP_PREVSOLO_KEY else KP_PREVNET_KEY
            val legalIds =
                if ( standalone ) {
                    setOf(R.id.radio_default, R.id.radio_configure,)
                } else {
                    setOf(R.id.radio_known, R.id.radio_unknown, R.id.radio_configure,)
                }

            val lastSet = DBUtils.getCheckedIntFor(context, key, legalIds, 0)
            if (lastSet != 0) {
                group.check(lastSet)
            }
        }
    }

    fun onButtonPressed(procs: ButtonCallbacks) {
        if (0 != mCurRadio) {
            val context = context
            val gameName = gameName()
            when (mCurRadio) {
                R.id.radio_known -> {
                    DBUtils.setStringFor(context, KP_NAME_KEY, mCurKnown)
                    procs.onUseKnown(mCurKnown!!, gameName)
                }

                R.id.radio_unknown, R.id.radio_default -> procs.onStartGame(
                    gameName,
                    mStandalone,
                    false
                )

                R.id.radio_configure -> procs.onStartGame(gameName, mStandalone, true)
                else -> Assert.failDbg() // fired
            }
            val key = if (mStandalone) KP_PREVSOLO_KEY else KP_PREVNET_KEY
            DBUtils.setIntFor(context, key, mCurRadio)
        }
    }

    private fun gameName(): String {
        val et = findViewById<View>(R.id.name_edit) as EditWClear
        return et.text.toString()
    }

    override fun onItemSelected(
        parent: AdapterView<*>?, view: View,
        pos: Int, id: Long
    ) {
        if (view is TextView) {
            mCurKnown = view.text.toString()
            onRadioChanged()
        }
    }

    override fun onNothingSelected(parent: AdapterView<*>?) {}

    override fun onCheckedChanged(group: RadioGroup, checkedId: Int) {
        mCurRadio = checkedId
        onRadioChanged()
    }

    private fun onRadioChanged() {
        mNamesSpinner?.let {
            it.visibility = if (mCurRadio == R.id.radio_known) VISIBLE else GONE
        }

        val context = context
        var resId = 0
        var msg: String? = null
        when (mCurRadio) {
            R.id.radio_known -> {
                Assert.assertTrueNR(!mStandalone)
                msg = LocUtils.getString(context, R.string.newgame_invite_fmt, mCurKnown)
            }
            R.id.radio_unknown, R.id.radio_default -> resId = R.string.newgame_open_game
            R.id.radio_configure -> resId = R.string.newgame_configure_game
            else -> Assert.failDbg()
        }
        if (0 != resId) {
            msg = LocUtils.getString(context, resId)
        }
        msg?.let { mListener?.onNewButtonText(it) }
    }

    companion object {
        private val TAG: String = NewWithKnowns::class.java.simpleName
        private val KP_NAME_KEY = TAG + "/kp_last_name"
        private val KP_PREVSOLO_KEY = TAG + "/kp_prev_solo"
        private val KP_PREVNET_KEY = TAG + "/kp_prev_net"
    }
}

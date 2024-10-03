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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
package org.eehouse.android.xw4

import android.app.Activity
import android.view.View
import android.view.View.OnLongClickListener
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.LinearLayout

import java.util.EnumMap

import org.eehouse.android.xw4.BoardContainer.SizeChangeListener
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = Toolbar::class.java.simpleName

class Toolbar(private val mActivity: Activity, private val mDlgDlgt: HasDlgDelegate) :
    SizeChangeListener
{
    enum class Buttons(val resId: Int, val disableId: Int = 0) {
        BUTTON_HINT_PREV(R.id.prevhint_button),
        BUTTON_HINT_NEXT(R.id.nexthint_button),
        BUTTON_CHAT(R.id.chat_button, R.string.key_disable_chat_button),
        BUTTON_JUGGLE(R.id.shuffle_button, R.string.key_disable_shuffle_button),
        BUTTON_UNDO(R.id.undo_button, R.string.key_disable_undo_button),
        BUTTON_BROWSE_DICT(R.id.dictlist_button, R.string.key_disable_dicts_button),
        BUTTON_VALUES(R.id.values_button, R.string.key_disable_values_button),
        BUTTON_FLIP(R.id.flip_button, R.string.key_disable_flip_button),
    }

    private var mLayout: LinearLayout? = null
    private var mVisible = false
    private val mOnclicklisteners: MutableMap<Buttons, Any>
        = EnumMap(Buttons::class.java)
    private val mOnlongclicklisteners: MutableMap<Buttons, Any>
        = EnumMap(Buttons::class.java)
    private val mEnabled: MutableSet<Buttons> = HashSet()

    init {
        BoardContainer.registerSizeChangeListener(this)
    }

    fun setVisible(visible: Boolean) {
        if (mVisible != visible) {
            mVisible = visible
            doShowHide()
        }
    }

    fun getButtonFor(index: Buttons): ImageButton {
        return mActivity.findViewById<View>(index.resId) as ImageButton
    }

    fun setListener(index: Buttons, msgID: Int,
                    prefsKey: Int, action: Action): Toolbar
    {
        mOnclicklisteners[index] =
            View.OnClickListener {
                // Log.i( TAG, "setListener(): click on %s with action %s",
                //        view.toString(), action.toString() );
                mDlgDlgt.makeNotAgainBuilder(prefsKey, action, msgID)
                    .show()
            }
        return this
    }

    fun setLongClickListener(
        index: Buttons, msgID: Int,
        prefsKey: Int, action: Action
    ): Toolbar {
        mOnlongclicklisteners[index] = OnLongClickListener {
            mDlgDlgt.makeNotAgainBuilder(prefsKey, action, msgID)
                .show()
            true
        }
        return this
    }

    fun update(index: Buttons, enable: Boolean): Toolbar {
        var enable = enable
        if (enable) {
            val disableKeyID = index.disableId
            if (0 != disableKeyID) {
                enable = !XWPrefs.getPrefsBoolean(
                    mActivity, disableKeyID,
                    false
                )
            }
        }

        val button = mActivity.findViewById<View>(index.resId) as ImageButton?
        button?.let {
            it.visibility = if (enable) View.VISIBLE else View.GONE
        }

        if (enable) {
            mEnabled.add(index)
        } else {
            mEnabled.remove(index)
        }
        return this
    }

    fun enabledCount(): Int {
        return mEnabled.size
    }

    // SizeChangeListener
    override fun sizeChanged(width: Int, height: Int, isPortrait: Boolean) {
        installListeners()
        doShowHide()
    }

    fun installListeners() {
        tryAddListeners(mOnclicklisteners)
        tryAddListeners(mOnlongclicklisteners)
    }

    private fun tryAddListeners(map: MutableMap<Buttons, Any>) {
        val iter = map.keys.iterator()
        while (iter.hasNext()) {
            val key = iter.next()
            val listener = map[key]
            setListener(key, listener)
            iter.remove()
        }
    }

    private fun setListener(index: Buttons, listener: Any?) {
        val button = getButtonFor(index)
        when (listener) {
            is View.OnClickListener ->
                button.setOnClickListener(listener)
            is OnLongClickListener ->
                button.setOnLongClickListener(listener)
            else -> Assert.failDbg()
        }
    }

    private fun doShowHide() {
        val isPortrait = BoardContainer.getIsPortrait()

        if (null == mLayout) {
            mLayout = LocUtils.inflate(mActivity, R.layout.toolbar) as LinearLayout
        } else {
            (mLayout!!.parent as ViewGroup).removeView(mLayout)
        }
        mLayout!!.orientation = if (isPortrait) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL

        val scrollerId = if (isPortrait) R.id.tbar_parent_hor else R.id.tbar_parent_vert
        val scroller = mActivity.findViewById<ViewGroup>(scrollerId)
        // Google's had reports of a crash adding second view
        scroller.removeAllViews()
        scroller.addView(mLayout)

        scroller.visibility = if (mVisible) View.VISIBLE else View.GONE
    }
}

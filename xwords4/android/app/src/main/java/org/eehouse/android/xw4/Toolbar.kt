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

import org.eehouse.android.xw4.BoardContainer.SizeChangeListener
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = Toolbar::class.java.simpleName

class Toolbar(private val m_activity: Activity, private val m_dlgDlgt: HasDlgDelegate) :
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

    private var m_layout: LinearLayout? = null
    private var m_visible = false
    private val m_onClickListeners: MutableMap<Buttons, Any> = HashMap()
    private val m_onLongClickListeners: MutableMap<Buttons, Any> = HashMap()
    private val m_enabled: MutableSet<Buttons> = HashSet()

    init {
        BoardContainer.registerSizeChangeListener(this)
    }

    fun setVisible(visible: Boolean) {
        if (m_visible != visible) {
            m_visible = visible
            doShowHide()
        }
    }

    fun getButtonFor(index: Buttons): ImageButton {
        return m_activity.findViewById<View>(index.resId) as ImageButton
    }

    fun setListener(index: Buttons, msgID: Int,
                    prefsKey: Int, action: Action): Toolbar
    {
        m_onClickListeners[index] =
            View.OnClickListener {
                // Log.i( TAG, "setListener(): click on %s with action %s",
                //        view.toString(), action.toString() );
                m_dlgDlgt.makeNotAgainBuilder(prefsKey, action, msgID)
                    .show()
            }
        return this
    }

    fun setLongClickListener(
        index: Buttons, msgID: Int,
        prefsKey: Int, action: Action
    ): Toolbar {
        m_onLongClickListeners[index] = OnLongClickListener {
            m_dlgDlgt.makeNotAgainBuilder(prefsKey, action, msgID)
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
                    m_activity, disableKeyID,
                    false
                )
            }
        }

        val id = index.resId
        val button = m_activity.findViewById<View>(id) as ImageButton
        if (null != button) {
            button.visibility = if (enable) View.VISIBLE else View.GONE
        }

        if (enable) {
            m_enabled.add(index)
        } else {
            m_enabled.remove(index)
        }
        return this
    }

    fun enabledCount(): Int {
        return m_enabled.size
    }

    // SizeChangeListener
    override fun sizeChanged(width: Int, height: Int, isPortrait: Boolean) {
        installListeners()
        doShowHide()
    }

    fun installListeners() {
        tryAddListeners(m_onClickListeners)
        tryAddListeners(m_onLongClickListeners)
    }

    private fun tryAddListeners(map: MutableMap<Buttons, Any>) {
        val iter = map.keys.iterator()
        while (iter.hasNext()) {
            val key = iter.next()
            val listener = map[key]
            if (setListener(key, listener)) {
                iter.remove()
            }
        }
    }

    private fun setListener(index: Buttons, listener: Any?): Boolean {
        val button = getButtonFor(index)
        val success = null != button
        if (success) {
            if (listener is View.OnClickListener) {
                button.setOnClickListener(listener as View.OnClickListener?)
            } else if (listener is OnLongClickListener) {
                button.setOnLongClickListener(listener as OnLongClickListener?)
            } else {
                Assert.failDbg()
            }
        }
        return success
    }

    private fun doShowHide() {
        val isPortrait = BoardContainer.getIsPortrait()

        if (null == m_layout) {
            m_layout = LocUtils.inflate(m_activity, R.layout.toolbar) as LinearLayout
        } else {
            (m_layout!!.parent as ViewGroup).removeView(m_layout)
        }
        m_layout!!.orientation = if (isPortrait) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL

        val scrollerId = if (isPortrait) R.id.tbar_parent_hor else R.id.tbar_parent_vert
        val scroller = m_activity.findViewById<View>(scrollerId) as ViewGroup
        if (null != scroller) {
            // Google's had reports of a crash adding second view
            scroller.removeAllViews()
            scroller.addView(m_layout)

            scroller.visibility = if (m_visible) View.VISIBLE else View.GONE
        }
    }
}

/*
 * Copyright 2012 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.util.AttributeSet
import android.view.View
import android.view.View.OnLongClickListener
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView

import org.eehouse.android.xw4.SelectableItem.LongClickHandler
import org.eehouse.android.xw4.jni.GameMgr
import org.eehouse.android.xw4.jni.GameMgr.GroupRef
import org.eehouse.android.xw4.loc.LocUtils

private val TAG: String = GameListGroup::class.java.simpleName

class GameListGroup(cx: Context, aset: AttributeSet?) :
    ExpiringLinearLayout(cx, aset),
    LongClickHandler, View.OnClickListener, OnLongClickListener
{
    private var mGrp: GroupRef? = null
    private var mName: String? = null
    private var m_expanded = false
    private var m_cb: SelectableItem? = null
    private var mGsl: GroupStateListener? = null
    private var m_etv: TextView? = null
    private var m_selected = false
    private var m_nGames = 0
    private var m_dsdel: DrawSelDelegate? = null
    private var mExpandButton: ImageButton? = null
    private var m_check: ImageView? = null

    override fun onFinishInflate() {
        super.onFinishInflate()
        m_etv = findViewById<View>(R.id.game_name) as TextView
        mExpandButton = findViewById<View>(R.id.expander) as ImageButton
        m_check = findViewById<View>(R.id.group_check) as ImageView
        m_check!!.setOnClickListener(this)

        // click on me OR the button expands/contracts...
        setOnClickListener(this)
        mExpandButton!!.setOnClickListener(this)

        m_dsdel = DrawSelDelegate(this)
        setOnLongClickListener(this)

        setButton()
    }

    override fun setSelected(selected: Boolean) {
        // If new value and state not in sync, force change in state
        if (selected != m_selected) {
            toggleSelected()
        }
    }

    fun load(grp: GroupRef, gsl: GroupStateListener ) {
        mGrp = grp
        mGsl = gsl
        reload()
    }

    fun reload() {
        mGrp?.let { grp ->
            launch {
                m_expanded = !grp.getGroupCollapsed()
                m_nGames = grp.getGroupGamesCount()
                mName = grp.getGroupName()
                setButton()
            }
        }
    }

    fun getGrp(): GroupRef? {
        return mGrp
    }

    fun setText(text: String?) {
        m_etv!!.text = text
    }

    // GameListAdapter.ClickHandler interface
    override fun longClicked() {
        toggleSelected()
    }

    protected fun toggleSelected() {
        m_selected = !m_selected
        m_dsdel!!.showSelected(m_selected)
        m_cb!!.itemToggled(this, m_selected)
        m_check!!.setImageResource(if (m_selected) R.drawable.ic_check_circle else 0)
    }

    //////////////////////////////////////////////////
    // View.OnLongClickListener interface
    //////////////////////////////////////////////////
    override fun onLongClick(view: View): Boolean {
        val handled = !XWApp.CONTEXT_MENUS_ENABLED
        if (handled) {
            longClicked()
        }
        return handled
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    override fun onClick(view: View) {
        when (view.id) {
            R.id.group_check -> toggleSelected()
            R.id.expander, this.id -> {
                Log.d(TAG, "onClick(): $m_expanded")
                mGsl!!.onGroupExpandedChanged(this, !m_expanded)
            }
            else -> Assert.failDbg()
        }
    }

    private fun setButton() {
        Log.d(TAG, "setButton()")
        mExpandButton?.let { button ->
            button.visibility = if (0 == m_nGames) GONE else VISIBLE
            val rsrc =
                if (m_expanded) R.drawable.expander_ic_maximized
                else R.drawable.expander_ic_minimized
            button.setImageResource(rsrc)
        }

        val name =
            LocUtils.getQuantityString(
                context,
                R.plurals.group_name_fmt,
                m_nGames, mName, m_nGames
            )
        val grp =
            if ( BuildConfig.DEBUG ) " $mGrp"
            else ""
        setText(name + grp)
    }
}

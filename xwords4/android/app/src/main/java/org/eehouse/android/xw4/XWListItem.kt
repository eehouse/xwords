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

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.CheckBox
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import org.eehouse.android.xw4.SelectableItem.LongClickHandler
import org.eehouse.android.xw4.loc.LocUtils

class XWListItem(private val mContext: Context, aset: AttributeSet?) :
    LinearLayout(mContext, aset),
    LongClickHandler, View.OnClickListener
{
    private val TAG = XWListItem::class.java.getSimpleName()
    private var m_delCb: DeleteCallback? = null
    private var m_selected = false
    private var m_selCb: SelectableItem? = null
    private var m_checkbox: CheckBox? = null
    private val m_dsdel = DrawSelDelegate(this)

    private var m_expListener: ExpandedListener? = null
    private var m_expanded = false
    private var m_expandedView: View? = null

    interface DeleteCallback {
        fun deleteCalled(item: XWListItem)
    }

    interface ExpandedListener {
        fun expanded(me: XWListItem, expanded: Boolean)
    }

    override fun onFinishInflate() {
        super.onFinishInflate()
        m_checkbox = findViewById<View>(R.id.checkbox) as CheckBox
        m_checkbox!!.setOnClickListener(this)
    }

    private var mPosition: Int = 0
    fun getPosition(): Int { return mPosition }
    fun setPosition( indx: Int ) { mPosition = indx }

    fun setExpandedListener(lstnr: ExpandedListener?) {
        m_expListener = lstnr
        if (null != lstnr) {
            setOnClickListener(this)
        }
    }

    fun setExpanded(expanded: Boolean) {
        m_expanded = expanded
        m_expListener?.expanded(this, m_expanded)
    }

    fun addExpandedView(view: View?) {
        if (null != m_expandedView) {
            removeExpandedView()
        }
        m_expandedView = view
        addView(view)
    }

    fun removeExpandedView() {
        removeView(m_expandedView)
        m_expandedView = null
    }

    fun getText(): String
    {
        val view = findViewById<View>(R.id.text_item) as TextView
        return view.text.toString()
    }

    // PENDING: make text non-nullable when all callers are in kotlin
    fun setText(text: String?) {
        val view = findViewById<View>(R.id.text_item) as TextView
        view.text = text!!
    }

    fun setComment(text: String?) {
        text?.let {
            val view = findViewById<View>(R.id.text_item2) as TextView
            view.visibility = VISIBLE
            view.text = it
        }
    }

    // For mCustom is for dists browser only. Maybe that case needs a
    // subclass??? PENDING
    private var mCustom = false
    fun setIsCustom(isCustom: Boolean)
    {
        mCustom = isCustom
        val view = findViewById<View>(R.id.text_item_custom) as TextView
        if (isCustom) {
            view.visibility = VISIBLE
            view.text = LocUtils.getString(mContext, R.string.wordlist_custom_note)
        } else {
            view.visibility = GONE
        }
    }

    fun getIsCustom(): Boolean { return mCustom }

    fun setDeleteCallback(cb: DeleteCallback?) {
        m_delCb = cb
        val button = findViewById<View>(R.id.del) as ImageButton
        button.setOnClickListener { m_delCb!!.deleteCalled(this@XWListItem) }
        button.visibility = VISIBLE
    }

    private fun setSelCB(selCB: SelectableItem?) {
        m_selCb = selCB
        m_checkbox!!.visibility =
            if (null == selCB) GONE else VISIBLE
    }

    override fun setSelected(selected: Boolean) {
        if (selected != m_selected) {
            toggleSelected()
        }
    }

    override fun setEnabled(enabled: Boolean) {
        val button = findViewById<View>(R.id.del) as ImageButton
        button.isEnabled = enabled
        // calling super here means the list item can't be opened for
        // the user to inspect data.  Might want to reconsider this.
        // PENDING
        super.setEnabled(enabled)
    }

    // I can't just extend an object used in layout -- get a class
    // cast exception when inflating it and casting to the subclass.
    // So rather than create a subclass that knows about its purpose
    // I'll extend this with a general mechanism.  Hackery but ok.
    // var cached: Any? = null
    private var mCached: Any? = null
    fun getCached(): Any? { return mCached }
    fun setCached(cached: Any?) { mCached = cached }

    // SelectableItem.LongClickHandler interface
    override fun longClicked() {
        toggleSelected()
    }

    // View.OnClickListener interface
    override fun onClick(view: View) {
        if (m_checkbox === view) {
            isSelected = m_checkbox!!.isChecked
        } else {
            setExpanded(!m_expanded) // toggle
        }
    }

    private fun toggleSelected() {
        m_selected = !m_selected

        m_dsdel.showSelected(m_selected)

        m_checkbox!!.isChecked = m_selected

        m_selCb!!.itemToggled(this, m_selected)
    }

    companion object {
        fun inflate(context: Context,
                    selCB: SelectableItem? = null): XWListItem
        {
            val item = LocUtils.inflate(context, R.layout.list_item) as XWListItem
            item.setSelCB(selCB)
            return item
        }
    }
}

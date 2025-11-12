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

import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.widget.ImageButton

private val TAG: String = ExpandImageButton::class.java.simpleName

class ExpandImageButton(context: Context, aset: AttributeSet?) :
    ImageButton(context, aset), View.OnClickListener
{
    private var m_expanded = false
    private val mListeners: MutableSet<ExpandChangeListener> = HashSet()

    interface ExpandChangeListener {
        fun expandedChanged(nowExpanded: Boolean)
    }

    override fun onFinishInflate() {
        super.onFinishInflate()
        setOnClickListener(this)

        setImageResource()
    }

    override fun onClick(view: View)
    {
        toggle()
    }

    fun setExpanded(expanded: Boolean): ExpandImageButton
    {
        if (m_expanded != expanded) {
            m_expanded = expanded

            setImageResource()

            mListeners.map{it.expandedChanged(m_expanded)}
        }
        return this
    }

    fun setOnExpandChangedListener(listener: ExpandChangeListener): ExpandImageButton {
        mListeners.add(listener)
        return this
    }

    fun toggle() = setExpanded(!m_expanded)

    private fun setImageResource()
        = setImageResource(if (m_expanded) R.drawable.expander_ic_maximized
                           else R.drawable.expander_ic_minimized)
}

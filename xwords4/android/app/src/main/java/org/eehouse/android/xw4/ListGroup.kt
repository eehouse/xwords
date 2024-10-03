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
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import org.eehouse.android.xw4.loc.LocUtils
import org.eehouse.android.xw4.loc.LocUtils.inflate

private val TAG: String = ListGroup::class.java.simpleName

class ListGroup(cx: Context, aset: AttributeSet?) :
    LinearLayout(cx, aset), View.OnClickListener
{
    private var m_expanded = false
    private var m_expandButton: ImageButton? = null
    private var m_text: TextView? = null
    private var m_desc: String? = null
    private var m_listener: GroupStateListener? = null
    var position: Int = 0
        private set

    override fun onFinishInflate() {
        Log.d(TAG, "onFinishInflate()")
        super.onFinishInflate()
        m_expandButton = findViewById<View>(R.id.expander) as ImageButton
        m_text = findViewById<View>(R.id.game_name) as TextView

        m_expandButton!!.setOnClickListener(this)
        setOnClickListener(this)

        setButtonImage()
        setText()
    }

    //////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
    override fun onClick(view: View) {
        m_expanded = !m_expanded
        m_listener?.onGroupExpandedChanged(this, m_expanded)
        setButtonImage()
    }

    private fun setButtonImage() {
        m_expandButton?.setImageResource(
            if (m_expanded) R.drawable.expander_ic_maximized
            else R.drawable.expander_ic_minimized
        )
    }

    private fun setText() {
        m_text?.text = m_desc
    }

    companion object {
        fun make(
            context: Context, convertView: View?,
            lstnr: GroupStateListener?, posn: Int,
            desc: String?, expanded: Boolean
        ): ListGroup {
            val result =
                if (null != convertView && convertView is ListGroup) {
                    convertView
                } else {
                    LocUtils.inflate(context, R.layout.list_group) as ListGroup
                }
            result.position = posn
            result.m_expanded = expanded
            result.m_desc = desc
            result.m_listener = lstnr

            result.setButtonImage() // in case onFinishInflate already called
            result.setText()

            return result
        }
    }
}

/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.graphics.Canvas
import android.os.Handler
import android.util.AttributeSet
import android.widget.TextView

class ExpiringTextView(private val m_context: Context, attrs: AttributeSet?) :
    TextView(m_context, attrs)
{
    private var m_delegate: ExpiringDelegate? = null

    fun setPct(
        handler: Handler, haveTurn: Boolean,
        haveTurnLocal: Boolean, startSecs: Long
    ) {
        val delegate = delegate
        delegate.setHandler(handler)

        setPct(haveTurn, haveTurnLocal, startSecs)
    }

    fun setPct(
        haveTurn: Boolean, haveTurnLocal: Boolean,
        startSecs: Long
    ) =  m_delegate?.configure(haveTurn, haveTurnLocal, startSecs)

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        m_delegate?.onDraw(canvas)
    }

    private val delegate: ExpiringDelegate
        get() {
            if (null == m_delegate) {
                m_delegate = ExpiringDelegate(m_context, this)
            }
            return m_delegate!!
        }
}

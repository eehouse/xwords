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
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.os.Build
import android.os.Handler
import android.view.View
import org.eehouse.android.xw4.Assert.assertTrue
import org.eehouse.android.xw4.Utils.getCurSeconds
import java.lang.ref.WeakReference

private val TAG: String = ExpiringDelegate::class.java.simpleName

class ExpiringDelegate(private val m_context: Context, private val m_view: View) {
    private var m_active = false
    private var m_pct = -1
    private var m_backPct = -1
    private var m_back: Drawable? = null
    private var m_doFrame = false
    private var m_haveTurnLocal = false
    private var m_startSecs: Long = 0
    private val m_runnable: Runnable? = null
    private var m_selected = false
    private val m_dsdel = DrawSelDelegate(m_view)

    // Combine all the timers into one. Since WeakReferences to the same
    // object aren't equal we need a separate set of their hash codes to
    // prevent storing duplicates.
    private class ExpUpdater : Runnable {
        private var m_handler: Handler? = null
        private val m_refs = ArrayList<WeakReference<ExpiringDelegate>>()
        private val m_hashes: MutableSet<Int> = HashSet()

        override fun run() {
            var sizeBefore: Int
            val dlgts = ArrayList<ExpiringDelegate>()
            synchronized(this) {
                sizeBefore = m_refs.size
                m_hashes.clear()
                val iter = m_refs.iterator()
                while (iter.hasNext()) {
                    val ref = iter.next()
                    val dlgt = ref.get()
                    if (null == dlgt /* || dlgts.contains( dlgt )*/) {
                        iter.remove()
                    } else {
                        dlgts.add(dlgt)
                        m_hashes.add(dlgt.hashCode())
                    }
                }
            }

            Log.d(
                TAG, "ref had %d refs, now has %d expiringdelegate views",
                sizeBefore, dlgts.size
            )

            for (dlgt in dlgts) {
                dlgt.timerFired()
            }

            reschedule()
        }

        private fun reschedule() {
            m_handler!!.postDelayed(this, INTERVAL_SECS * 1000 / 100)
        }

        fun add(self: ExpiringDelegate) {
            val hash = self.hashCode()
            synchronized(this) {
                if (!m_hashes.contains(hash)) {
                    m_hashes.add(hash)
                    m_refs.add(WeakReference(self))
                }
            }
        }

        fun setHandler(handler: Handler) {
            if (handler !== m_handler) {
                Log.d(TAG, "handler changing from %H to %H", m_handler, handler)
                m_handler = handler
                reschedule()
            }
        }
    }

    fun setHandler(handler: Handler): ExpiringDelegate {
        s_updater.setHandler(handler)
        return this
    }

    fun configure(
        haveTurn: Boolean, haveTurnLocal: Boolean,
        startSecs: Long
    ) {
        m_active = haveTurn
        m_doFrame = !haveTurnLocal
        if (haveTurn) {
            m_startSecs = startSecs
            m_haveTurnLocal = haveTurnLocal
            figurePct()
            if (haveTurnLocal) {
                setBackground()
            } else {
                m_view.setBackgroundDrawable(null)
                m_view.setWillNotDraw(false)
            }
        }
    }

    fun setSelected(selected: Boolean) {
        m_selected = selected
        m_dsdel.showSelected(m_selected)
    }

    fun onDraw(canvas: Canvas) {
        if (m_selected) {
            // do nothing; the drawable's set already
        } else if (m_active && m_doFrame) {
            assertTrue(0 <= m_pct && m_pct <= 100)
            m_view.getDrawingRect(s_rect)
            val width = s_rect.width()
            val redWidth = width * m_pct / 100
            assertTrue(redWidth <= width)

            if (s_kitkat) {
                ++s_rect.top
                ++s_rect.left
            }

            // left edge
            addPoints(
                0, s_rect.left, s_rect.top,
                s_rect.left, s_rect.bottom - 1
            )

            // left horizontals
            addPoints(
                1, s_rect.left, s_rect.top,
                s_rect.left + redWidth, s_rect.top
            )
            addPoints(
                2, s_rect.left, s_rect.bottom - 1,
                s_rect.left + redWidth,
                s_rect.bottom - 1
            )

            // right horizontals
            addPoints(
                3, s_rect.left + redWidth, s_rect.top,
                s_rect.right - 1, s_rect.top
            )
            addPoints(
                4, s_rect.left + redWidth, s_rect.bottom - 1,
                s_rect.right - 1, s_rect.bottom - 1
            )

            // right edge
            addPoints(
                5, s_rect.right - 1, s_rect.top,
                s_rect.right - 1, s_rect.bottom
            )

            var offset = 0
            var count = s_points.size
            if (0 < redWidth) {
                s_paint.color = XWApp.RED
                canvas.drawLines(s_points, offset, count / 2, s_paint)
                count /= 2
                offset += count
            }
            if (redWidth < width) {
                s_paint.color = XWApp.GREEN
            }
            canvas.drawLines(s_points, offset, count, s_paint)
        }
    }

    private fun addPoints(
        offset: Int, left: Int, top: Int,
        right: Int, bottom: Int
    ) {
        var offset = offset
        offset *= 4
        s_points[offset + 0] = left.toFloat()
        s_points[offset + 1] = top.toFloat()
        s_points[offset + 2] = right.toFloat()
        s_points[offset + 3] = bottom.toFloat()
    }

    private fun setBackground() {
        assertTrue(m_active)
        if (-1 != m_pct && m_backPct != m_pct) {
            m_back = mkBackground(m_pct)
            m_backPct = m_pct
        }
        if (null != m_back) {
            m_view.setBackgroundDrawable(m_back)
        }
    }

    private fun mkBackground(pct: Int): Drawable {
        assertTrue(0 <= pct && pct <= 100)
        val bm = Bitmap.createBitmap(100, 1, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bm)

        val paint = Paint()
        paint.style = Paint.Style.FILL
        paint.color = XWApp.RED
        canvas.drawRect(0f, 0f, pct.toFloat(), 1f, paint)
        paint.color = Utils.TURN_COLOR
        canvas.drawRect(pct.toFloat(), 0f, 100f, 1f, paint)
        return BitmapDrawable(m_context.resources, bm)
    }

    private fun figurePct() {
        if (0L == m_startSecs) {
            m_pct = 0
        } else {
            val now = getCurSeconds()
            val passed = now - m_startSecs
            m_pct = ((100 * passed) / INTERVAL_SECS).toInt()
            if (m_pct > 100) {
                m_pct = 100
            } else if (m_pct < 0) {
                m_pct = 0
            } else {
                s_updater.add(this)
            }
        }
    }

    private fun timerFired() {
        if (m_active) {
            figurePct()
            if (m_haveTurnLocal) {
                m_back = null
                setBackground()
            }
            m_view.invalidate()
        }
    }

    companion object {
        private const val INTERVAL_SECS = (3 * 24 * 60 * 60).toLong()

        // private static final long INTERVAL_SECS = 60 * 10;   // for testing
        private val s_kitkat = 19 <= Build.VERSION.SDK.toInt()

        // these can be static as drawing's all in same thread.
        private var s_rect = Rect()
        private var s_paint = Paint()
        private var s_points: FloatArray
        private var s_updater: ExpUpdater

        init {
            s_paint.style = Paint.Style.STROKE
            s_paint.strokeWidth = 1f
            s_points = FloatArray(4 * 6)
            s_updater = ExpUpdater()
        }
    }
}

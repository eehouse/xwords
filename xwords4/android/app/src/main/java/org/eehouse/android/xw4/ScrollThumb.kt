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
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.os.Handler
import android.os.Looper
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

private val TAG = ScrollThumb::class.java.simpleName
// dark theme
private const val THUMB_COLOR = "#66FFFFFF"
// Light theme
// private const val THUMB_COLOR = "#66000000"
private const val THUMB_HEIGHT = 160

class ScrollThumb(private val mContext: Context, attrs: AttributeSet?) :
    View(mContext, attrs), Runnable {

    interface OnScrollListener {
        fun onFastScroll(thumbPct: Float)
    }

    private var mThumbTop = 0
    private val mThumbPaint = Paint().apply { color = Color.parseColor(THUMB_COLOR) }
    private var mOnScrollListener: OnScrollListener? = null
    private var mDragging: Boolean = false
    private val mHandler = Handler(Looper.getMainLooper())

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val halfWidth = (width / 2).toFloat()
        canvas.drawRect(halfWidth, mThumbTop.toFloat(),
                        halfWidth + (halfWidth/2),
                        (mThumbTop + THUMB_HEIGHT).toFloat(),
                        mThumbPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val result = 
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_MOVE -> {
                    parent.requestDisallowInterceptTouchEvent(true)

                    val halfHeight = THUMB_HEIGHT / 2
                    val yy = event.y.toInt().coerceIn(halfHeight, height-halfHeight)
                    mThumbTop = yy - halfHeight
                    (mThumbTop.toFloat() / (height-THUMB_HEIGHT)).also {
                            mOnScrollListener?.onFastScroll(it)
                    }
                    invalidate()
                    resetTimer()
                    mDragging = true
                    true
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_CANCEL -> {
                    parent.requestDisallowInterceptTouchEvent(false)
                    mDragging = false
                    true
                }
                else -> super.onTouchEvent(event)
            }
        return result
    }

    fun setOnScrollListener(listener: OnScrollListener) {
        mOnScrollListener = listener
    }

    fun updateThumb(rview: RecyclerView) {
        resetTimer()

        visibility = VISIBLE

        if ( !mDragging ) {
            (rview.layoutManager as LinearLayoutManager).also { llm ->
                val total = llm.itemCount
                if (0 < total) {
                    val first = llm.findFirstVisibleItemPosition().toFloat()
                    val nVisible = llm.childCount
                    val fraction = first / (total - nVisible).coerceAtLeast(1)
                    mThumbTop = (fraction * height).toInt() - (THUMB_HEIGHT/2)
                    invalidate()
                }
            }
        }
    }

    override fun run() {
        visibility = GONE
    }

    private fun resetTimer() {
        mHandler.removeCallbacks(this)
        mHandler.postDelayed(this, 1_000)
    }
}

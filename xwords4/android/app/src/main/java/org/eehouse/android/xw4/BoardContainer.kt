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
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import android.view.ViewGroup
import java.lang.ref.WeakReference
import kotlin.math.min

class BoardContainer(context: Context, aset: AttributeSet?) : ViewGroup(context, aset) {
    private var mBoardBounds: Rect? = null
    private var mToolsBounds: Rect? = null

    interface SizeChangeListener {
        fun sizeChanged(width: Int, height: Int, isPortrait: Boolean)
    }

    init {
        sWidth = 0
        sHeight = sWidth
        sScl = null
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)
        if (0 != width || 0 != height) {
            setForPortrait(width, height)

            // Add a margin of half a percent of the lesser of width,height
            val padding = (min(width.toDouble(), height.toDouble()) * 5 / 1000).toInt()
            figureBounds(padding, padding, width - padding * 2, height - padding * 2)

            // Measure any toolbar first so we can take extra space for the
            // board
            val childCount = childCount
            if (1 < childCount) {
                Assert.assertTrue(4 == childCount)

                // Measure the toolbar
                measureChild(if (sIsPortrait) HBAR_INDX else VBAR_INDX, mToolsBounds)
                adjustBounds()
                val child = getChildAt(if (sIsPortrait) HBAR_INDX else VBAR_INDX)
                // Log.i(TAG, "measured %s; passed ht: %d; got back ht: %d",
                //       child.toString(), mToolsBounds!!.height(),
                //       child.measuredHeight)
                if (haveTradeBar()) {
                    // Measure the exchange buttons bar
                    measureChild(EXCH_INDX, mToolsBounds)
                }
            }

            // Measure the board
            measureChild(BOARD_INDX, mBoardBounds)
        }
        setMeasuredDimension(width, height)
    }

    // In portrait mode, board gets BD_PCT of the vertical space. Otherwise it
    // gets it all IFF the trade buttons aren't visible.
    override fun onLayout(
        changed: Boolean, left: Int, top: Int,
        right: Int, bottom: Int
    ) {
        // If this isn't true, need to refigure the rects
        // Assert.assertTrue( 0 == left && 0 == top );

        // layout the board
        layoutChild(BOARD_INDX, mBoardBounds)
        if (1 < childCount) {
            // The trade bar
            if (haveTradeBar()) {
                layoutChild(EXCH_INDX, mToolsBounds)
            }

            // Now one of the toolbars
            layoutChild(if (sIsPortrait) HBAR_INDX else VBAR_INDX, mToolsBounds)
        }
    }

    private fun measureChild(index: Int, rect: Rect?) {
        val childWidthSpec = MeasureSpec.makeMeasureSpec(
            rect!!.width(),
            MeasureSpec.AT_MOST
        )
        val childHeightSpec = MeasureSpec.makeMeasureSpec(
            rect.height(),
            MeasureSpec.AT_MOST
        )
        val view = getChildAt(index)
        measureChild(view, childWidthSpec, childHeightSpec)
    }

    private fun layoutChild(index: Int, rect: Rect?) {
        val child = getChildAt(index)
        if (GONE != child.visibility) {
            child.layout(rect!!.left, rect.top, rect.right, rect.bottom)
        }
    }

    private fun setForPortrait(width: Int, height: Int) {
        if (height != sHeight || width != sWidth) {
            sHeight = height
            sWidth = width
            sIsPortrait = PORTRAIT_THRESHHOLD < height * 100 / width
            findViewById<View>(R.id.tbar_parent_hor).visibility =
                if (sIsPortrait) VISIBLE else GONE
            findViewById<View>(R.id.tbar_parent_vert).visibility =
                if (sIsPortrait) GONE else VISIBLE
            callSCL()
        }
    }

    private fun figureBounds(left: Int, top: Int, width: Int, height: Int) {
        var left = left
        var top = top
        var width = width
        var height = height
        val boardHeight =
            if (haveTradeBar() || sIsPortrait) height * BOARD_PCT_VERT / 100 else height
        val boardWidth = if (sIsPortrait) width else width * BOARD_PCT_HOR / 100

        // board
        mBoardBounds = Rect(
            left, top, left + boardWidth,
            top + boardHeight
        )
        // DbgUtils.logf( "BoardContainer: boardBounds: %s", boardBounds.toString() );
        // toolbar
        if (sIsPortrait) {
            top += boardHeight
            height -= boardHeight
        } else {
            left += boardWidth
            width -= boardWidth
        }
        mToolsBounds = Rect(left, top, left + width, top + height)
    }

    private fun adjustBounds() {
        if (sIsPortrait) {
            val curHeight = mToolsBounds!!.height()
            val newHeight = getChildAt(HBAR_INDX).measuredHeight
            val diff = curHeight - newHeight
            mBoardBounds!!.bottom += diff
            mToolsBounds!!.top += diff
        } else {
            val curWidth = mToolsBounds!!.width()
            val newWidth = getChildAt(VBAR_INDX).measuredWidth
            val diff = curWidth - newWidth
            mBoardBounds!!.right += diff
            mToolsBounds!!.left += diff
        }
    }

    private fun haveTradeBar(): Boolean {
        var result = false
        if (sIsPortrait && 1 < childCount) {
            val bar = getChildAt(1)
            result = null != bar && GONE != bar.visibility
        }
        return result
    }

    companion object {
        private val TAG = BoardContainer::class.java.getSimpleName()

        // If the ratio of height/width exceeds this, use portrait layout
        private const val PORTRAIT_THRESHHOLD = 105
        private const val BOARD_PCT_VERT = 90
        private const val BOARD_PCT_HOR = 85
        private const val BOARD_INDX = 0
        private const val EXCH_INDX = 1
        private const val VBAR_INDX = 2
        private const val HBAR_INDX = 3
        private var sIsPortrait = true

        private var sWidth = 0
        private var sHeight = 0
        private var sScl: WeakReference<SizeChangeListener>? = null

        fun registerSizeChangeListener(scl: SizeChangeListener) {
            sScl = WeakReference(scl)
            callSCL()
        }

        fun getIsPortrait(): Boolean { return sIsPortrait }

        private fun callSCL() {
            if (0 != sWidth || 0 != sHeight) {
                sScl?.get()?.sizeChanged( sWidth, sHeight, sIsPortrait )
            }
        }
    }
}

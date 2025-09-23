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

import android.app.Activity
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.os.Build
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View

import kotlin.math.abs
import kotlin.math.min
import kotlin.math.sqrt

import org.eehouse.android.xw4.DbgUtils.printStack
import org.eehouse.android.xw4.BoardCanvas.DrawProgress
import org.eehouse.android.xw4.jni.BoardDims
import org.eehouse.android.xw4.jni.BoardHandler
import org.eehouse.android.xw4.jni.BoardHandler.DrawDoneProc
import org.eehouse.android.xw4.jni.BoardHandler.NewRecentsProc
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.DrawCtx
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.UtilCtxt

class BoardView(private val mContext: Context, attrs: AttributeSet?) :
    View(mContext, attrs), BoardHandler, DrawProgress
{
    private val mDefaultFontHt: Int
    private val mMediumFontHt: Int
    private var mGi: CurGameInfo? = null
    private var mIsSolo = false
    private var mLayoutWidth = 0
    private var mDimsTossCount = 0 // hack hack hack!!
    private var mLayoutHeight = 0
    private var mCanvas: BoardCanvas? = null // owns the bitmap
    private var mUtils: UtilCtxt? = null
    private var mGR: GameRef? = null
    private var mParent: Activity? = null
    private var mMeasuredFromDims = false
    private var mDims: BoardDims? = null
    private var mConnTypes: CommsConnTypeSet? = null
    private var mNRP: NewRecentsProc? = null
    private var mDDProc: DrawDoneProc? = null
    private var mLastSpacing = MULTI_INACTIVE
    private var mBitmap: Bitmap? = null // the board

    // called when inflating xml
    init {
        val scale = resources.displayMetrics.density
        mDefaultFontHt = (MIN_FONT_DIPS * scale + 0.5f).toInt()
        mMediumFontHt = mDefaultFontHt * 3 / 2
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        var draw = false
        val wantMore = mGR?.let { gr ->
            val action = event.action
            val xx = event.x.toInt()
            val yy = event.y.toInt()
            when (action) {
                MotionEvent.ACTION_DOWN -> {
                    mLastSpacing = MULTI_INACTIVE
                    if (ConnStatusHandler.handleDown(xx, yy)) {
                        // do nothing
                    } else if (gr.containsPt(xx, yy)) {
                        gr.handlePenDown(xx, yy)
                    } else {
                        Log.d(TAG, "onTouchEvent(): in white space")
                    }
                }

                MotionEvent.ACTION_MOVE ->
                    if (ConnStatusHandler.handleMove(xx, yy)) {
                        // do nothing
                    } else if (MULTI_INACTIVE == mLastSpacing) {
                        gr.handlePenMove(xx, yy)
                    } else {
                        figureZoom(event).let{ zoomBy ->
                            if (0 != zoomBy) {
                                gr.zoom(if (zoomBy < 0) -2 else 2)
                            }
                        }
                    }

                MotionEvent.ACTION_UP ->
                    if (ConnStatusHandler.handleUp(xx, yy)) {
                        // do nothing
                    } else {
                        gr.handlePenUp(xx, yy)
                    }

                MotionEvent.ACTION_POINTER_DOWN, MotionEvent.ACTION_POINTER_2_DOWN -> {
                    gr.handlePenUp(xx, yy)
                    mLastSpacing = getSpacing(event)
                }

                MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_POINTER_2_UP ->
                    mLastSpacing = MULTI_INACTIVE

                else -> Log.w(TAG, "onTouchEvent: unknown action: %d", action)
            }
            // invalidate()
            true
        } ?: false

        return wantMore // true required to get subsequent events
    }


    fun draw() {
        mGR?.let {
            launch {
                it.draw()
                invalidate()
            }
        }
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // Log.d( TAG, "onMeasure(width: %s, height: %s)",
        //        MeasureSpec.toString( widthMeasureSpec ),
        //        MeasureSpec.toString( heightMeasureSpec ) );
        mDims?.let { dims ->
            if (BoardContainer.getIsPortrait() != dims.height > dims.width) {
                // square possible; will break above! No. tested by forceing square
                Log.d(TAG, "onMeasure: discarding m_dims")
                if (++mDimsTossCount < 4) {
                    mDims = null
                    mLayoutHeight = 0
                    mLayoutWidth = mLayoutHeight
                } else {
                    Log.d(
                        TAG, "onMeasure(): unexpected width (%d) to height (%d) ratio"
                                + "; proceeding", dims.width, dims.height
                    )
                }
            }
        }
        var width: Int
        var height: Int
        mMeasuredFromDims = null != mDims
        if (mMeasuredFromDims) {
            height = mDims!!.height
            width = mDims!!.width
        } else {
            width = MeasureSpec.getSize(widthMeasureSpec)
            height = MeasureSpec.getSize(heightMeasureSpec)
        }
        val minHeight = suggestedMinimumHeight
        if (height < minHeight) {
            height = minHeight
        }
        val minWidth = suggestedMinimumWidth
        if (width < minWidth) {
            width = minWidth
        }
        setMeasuredDimension(width, height)
        // Log.d( TAG, "onMeasure: calling setMeasuredDimension( width=%d, height=%d )",
        //        width, height );
    }

    // It may make sense to kick off layout from here rather than onDraw().
    override fun onSizeChanged( width: Int, height: Int, oldWidth: Int, oldHeight: Int )
    {
        Log.d(TAG, "onSizeChanged($width, $height, $oldWidth, $oldHeight)")
        super.onSizeChanged( width, height, oldWidth, oldHeight )
    }

    // It's an error to do any drawing outside of onDraw() -- meaning that
    // starting a coroutine here and then drawing is a no-no. So leave that
    // operation in actuallyDraw()
    override fun onDraw(canvas: Canvas) {
        val width = width
        val height = height
        if (width == mLayoutWidth && height == mLayoutHeight) {
            actuallyDraw(canvas)
        } else if (null == mGi) {
            // nothing to do either
            Log.d(TAG, "onDraw(): no m_gi")
        } else if (null == mGR) {
            // nothing to do either
            Log.d(TAG, "onDraw(): no mGR")
        } else if (null == mDims) {
            Log.d( TAG, "onDraw(): null mDims" ); // don't see this!!
            val paint = Paint()
            paint.textSize = mMediumFontHt.toFloat()
            val scratch = Rect()
            val timerTxt = "-00:00"
            paint.getTextBounds(timerTxt, 0, timerTxt.length, scratch)
            val timerWidth = scratch.width()
            val fontWidth =
                min(mDefaultFontHt.toDouble(), (timerWidth / timerTxt.length).toDouble())
                .toInt()
            doLayout( width, height, fontWidth, mDefaultFontHt)
            // We'll be back....
        } else {
            val gr = mGR!!
            // If board size has changed we need a new bitmap
            val bmHeight = 1 + mDims!!.height
            val bmWidth = 1 + mDims!!.width
            Log.d(TAG, "onDraw(): DOING IT/creating mBitmap($bmWidth x $bmHeight)")
            mBitmap = Bitmap.createBitmap(
                bmWidth, bmHeight,
                Bitmap.Config.ARGB_8888
            )
            mCanvas = BoardCanvas(mParent!!, mBitmap!!, gr, mDims, mNRP, this)
            gr.setDraw(mCanvas!!, mUtils!!)

            // set so we know we're done
            mLayoutWidth = width
            mLayoutHeight = height

            gr.draw()    // this is asynchronous: invalidate may come too early
            invalidate() // get onDraw() called again
        }
    }

    private fun actuallyDraw(canvas: Canvas) {
        Log.d(TAG, "actuallyDraw()")
        if (!mMeasuredFromDims) {
            Log.d( TAG, "actuallyDraw(): m_measuredFromDims not set" );
        } else {
            Log.d(TAG, "actuallyDraw(): I'm ready to draw to $canvas")
            val bitmap = 
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    Bitmap.createBitmap(mBitmap!!)
                } else mBitmap!!
            Assert.assertNotNull(bitmap)
            DbgUtils.assertOnUIThread()

            Log.d(TAG, "calling canvas.drawBitmap(); should put bits on screen!!")
            canvas.drawBitmap(bitmap, 0f, 0f, Paint())

            mConnTypes?.let {
                ConnStatusHandler.draw(
                    mContext, canvas, it, mIsSolo
                )
            }
            Log.d(TAG, "done drawing bitmap")
        }
    }

    fun getDraw(): DrawCtx? {
        Assert.assertTrue( null != mCanvas )
        return mCanvas
    }

    fun setUtils(utils: UtilCtxt) {
        mUtils = utils
    }

    private fun doLayout(
        width: Int, height: Int, fontWidth: Int, fontHeight: Int
    ) {
        Log.d(TAG, "doLayout($width, $height)")
        val squareTiles = XWPrefs.getSquareTiles(mContext!!)
        Utils.launch {
            val gr = mGR!!
            val dims = gr.figureLayout(0, 0, width, height,
                                       150,  /*scorePct*/200,  /*trayPct*/
                                       width, fontWidth, fontHeight, squareTiles)

            // Make space for net status icon if appropriate
            if (mGi!!.deviceRole != CurGameInfo.DeviceRole.ROLE_STANDALONE) {
                val statusWidth = dims.boardWidth / 15
                dims.scoreWidth -= statusWidth
                val left = dims.scoreLeft + dims.scoreWidth + dims.timerWidth
                ConnStatusHandler.setRect(
                    left, dims.top, left + statusWidth,
                    dims.top + dims.scoreHt
                )
            } else {
                ConnStatusHandler.clearRect()
            }

            gr.applyLayout(dims)

            dimsChanged(dims)
        }
    }

    // BoardHandler interface implementation
    override fun startHandling(
        parent: Activity, gr: GameRef,
        gi: CurGameInfo, proc: NewRecentsProc?,
        ddProc: DrawDoneProc?
    ) {
        mParent = parent
        mGR = gr
        mGi = gi
        mIsSolo = CurGameInfo.DeviceRole.ROLE_STANDALONE == gi.deviceRole
        mConnTypes = gi.conTypes!!
        mLayoutWidth = 0
        mLayoutHeight = 0
        mNRP = proc
        mDDProc = ddProc
        // sCurGameID = mGi!!.gameID

        // Set the jni layout if we already have one
        mDims?.let {
            Log.d(TAG, "startHandling(): have dims so calling applyLayout()")
            gr.applyLayout(it)
            // Make sure we draw.  Sometimes when we're reloading after
            // an obsuring Activity goes away we otherwise won't.
            invalidate()
        } ?: run{ Log.d(TAG, "startHandling(): mDims not set!!") }
    }

    override fun stopHandling() {
        mGR?.setDraw()
        mGR = null
        mCanvas?.setGR(null)
    }

    // This isn't working yet, but the idea is that we let the common code
    // decide when to draw and use draw_endDraw() to know to transfer bits to
    // the screen.
    override fun drawDone() {
        invalidate()
        mDDProc?.let {it.drawDone()}
    }

    fun doJNIDraw() {
        synchronized(this) {
            mGR?.draw()
        }

        // Force update now that we have bits to copy. I don't know why (yet),
        // but on older versions of Android we need to run this even if
        // XwJNI.board_draw() returned false
        DbgUtils.assertOnUIThread()
        invalidate()
    }

    private fun dimsChanged(dims: BoardDims) {
        mDims = dims
        Log.d(TAG, "dimsChanged(): mDims set; requesting layout")
        mParent!!.runOnUiThread {
            requestLayout()
            invalidate()
        }
    }

    fun orientationChanged() {
        mDims = null
        mLayoutHeight = 0
        mLayoutWidth = mLayoutHeight
        mDimsTossCount = 0
        requestLayout()
    }

    fun setInTrade(inTrade: Boolean) {
        mCanvas?.setInTrade(inTrade)
    }

    val curPlayer: Int
        get() = mCanvas?.curPlayer ?: -1

    fun curPending(): Int {
        return mCanvas?.curPending() ?: 0
    }

    private fun getSpacing(event: MotionEvent): Int {
        val result: Int
        result = if (1 == event.pointerCount) {
            MULTI_INACTIVE
        } else {
            val xx = event.getX(0) - event.getX(1)
            val yy = event.getY(0) - event.getY(1)
            sqrt((xx * xx + yy * yy).toDouble()).toInt()
        }
        return result
    }

    private fun figureZoom(event: MotionEvent): Int {
        var zoomDir = 0
        if (MULTI_INACTIVE != mLastSpacing) {
            val newSpacing = getSpacing(event)
            val diff = abs((newSpacing - mLastSpacing).toDouble()).toInt()
            if (diff > PINCH_THRESHOLD) {
                zoomDir = if (newSpacing < mLastSpacing) -1 else 1
                mLastSpacing = newSpacing
            }
        }
        return zoomDir
    }

    // private fun handle(cmd: JNICmd, vararg args: Any) {
    //     Assert.fail()
    //     // if (null == mGR) {
    //     //     Log.w(TAG, "not calling handle(%s)", cmd.toString())
    //     //     printStack(TAG)
    //     // } else {
    //     //     mGR!!.handle(cmd, *args)
    //     // }
    // }

    companion object {
        private val TAG = BoardView::class.java.getSimpleName()
        private const val MIN_FONT_DIPS = 10.0f
        private const val MULTI_INACTIVE = -1
        private const val PINCH_THRESHOLD = 40
    }
}

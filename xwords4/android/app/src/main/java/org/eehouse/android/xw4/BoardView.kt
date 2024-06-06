/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import org.eehouse.android.xw4.jni.BoardDims
import org.eehouse.android.xw4.jni.BoardHandler
import org.eehouse.android.xw4.jni.BoardHandler.NewRecentsProc
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.JNIThread
import org.eehouse.android.xw4.jni.JNIThread.JNICmd
import org.eehouse.android.xw4.jni.SyncedDraw
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.GamePtr

class BoardView(private val mContext: Context, attrs: AttributeSet?) : View(
    mContext, attrs
), BoardHandler, SyncedDraw {
    private val mDefaultFontHt: Int
    private val mMediumFontHt: Int
    private val mInvalidator: Runnable
    private var mJniGamePtr: GamePtr? = null
    private var mGi: CurGameInfo? = null
    private var mIsSolo = false
    private var mLayoutWidth = 0
    private var mDimsTossCount = 0 // hack hack hack!!
    private var mLayoutHeight = 0
    private var mCanvas: BoardCanvas? = null // owns the bitmap
    private var mJniThread: JNIThread? = null
    private var mParent: Activity? = null
    private var mMeasuredFromDims = false
    private var mDims: BoardDims? = null
    private var mConnTypes: CommsConnTypeSet? = null
    private var mNRP: NewRecentsProc? = null
    private var mLastSpacing = MULTI_INACTIVE

    // called when inflating xml
    init {
        val scale = resources.displayMetrics.density
        mDefaultFontHt = (MIN_FONT_DIPS * scale + 0.5f).toInt()
        mMediumFontHt = mDefaultFontHt * 3 / 2
        mInvalidator = Runnable { invalidate() }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val wantMore = null != mJniThread
        if (wantMore) {
            val action = event.action
            val xx = event.x.toInt()
            val yy = event.y.toInt()
            when (action) {
                MotionEvent.ACTION_DOWN -> {
                    mLastSpacing = MULTI_INACTIVE
                    if (ConnStatusHandler.handleDown(xx, yy)) {
                        // do nothing
                    } else if (XwJNI.board_containsPt(mJniGamePtr, xx, yy)) {
                        handle(JNICmd.CMD_PEN_DOWN, xx, yy)
                    } else {
                        Log.d(TAG, "onTouchEvent(): in white space")
                    }
                }

                MotionEvent.ACTION_MOVE -> if (ConnStatusHandler.handleMove(xx, yy)) {
                    // do nothing
                } else if (MULTI_INACTIVE == mLastSpacing) {
                    handle(JNICmd.CMD_PEN_MOVE, xx, yy)
                } else {
                    val zoomBy = figureZoom(event)
                    if (0 != zoomBy) {
                        handle(
                            JNICmd.CMD_ZOOM,
                            if (zoomBy < 0) -2 else 2
                        )
                    }
                }

                MotionEvent.ACTION_UP -> if (ConnStatusHandler.handleUp(xx, yy)) {
                    // do nothing
                } else {
                    handle(JNICmd.CMD_PEN_UP, xx, yy)
                }

                MotionEvent.ACTION_POINTER_DOWN, MotionEvent.ACTION_POINTER_2_DOWN -> {
                    handle(JNICmd.CMD_PEN_UP, xx, yy)
                    mLastSpacing = getSpacing(event)
                }

                MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_POINTER_2_UP -> mLastSpacing =
                    MULTI_INACTIVE

                else -> Log.w(TAG, "onTouchEvent: unknown action: %d", action)
            }
        }
        return wantMore // true required to get subsequent events
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // Log.d( TAG, "onMeasure(width: %s, height: %s)",
        //        MeasureSpec.toString( widthMeasureSpec ),
        //        MeasureSpec.toString( heightMeasureSpec ) );
        if (null != mDims) {
            if (BoardContainer.getIsPortrait() != mDims!!.height > mDims!!.width) {
                // square possible; will break above! No. tested by forceing square
                Log.d(TAG, "onMeasure: discarding m_dims")
                if (++mDimsTossCount < 4) {
                    mDims = null
                    mLayoutHeight = 0
                    mLayoutWidth = mLayoutHeight
                } else {
                    Log.d(
                        TAG, "onMeasure(): unexpected width (%d) to height (%d) ratio"
                                + "; proceeding", mDims!!.width, mDims!!.height
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

    // @Override
    // public void onSizeChanged( int width, int height, int oldWidth, int oldHeight )
    // {
    //     DbgUtils.logf( "BoardView.onSizeChanged(): width: %d => %d; height: %d => %d",
    //                    oldWidth, width, oldHeight, height );
    //     super.onSizeChanged( width, height, oldWidth, oldHeight );
    // }
    // This will be called from the UI thread
    override fun onDraw(canvas: Canvas) {
        synchronized(this) {
            if (!layoutBoardOnce()) {
                // Log.d( TAG, "onDraw(): layoutBoardOnce() failed" );
            } else if (!mMeasuredFromDims) {
                // Log.d( TAG, "onDraw(): m_measuredFromDims not set" );
            } else {
                var bitmap = sBitmap
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    bitmap = Bitmap.createBitmap(bitmap!!)
                }
                canvas.drawBitmap(bitmap!!, 0f, 0f, Paint())
                ConnStatusHandler.draw(
                    mContext, canvas, resources,
                    mConnTypes, mIsSolo
                )
            }
        }
    }

    private fun layoutBoardOnce(): Boolean {
        val width = width
        val height = height
        var layoutDone = width == mLayoutWidth && height == mLayoutHeight
        if (layoutDone) {
            // Log.d( TAG, "layoutBoardOnce(): layoutDone true" );
        } else if (null == mGi) {
            // nothing to do either
            Log.d(TAG, "layoutBoardOnce(): no m_gi")
        } else if (null == mJniThread) {
            // nothing to do either
            Log.d(TAG, "layoutBoardOnce(): no m_jniThread")
        } else if (null == mDims) {
            // Log.d( TAG, "layoutBoardOnce(): null m_dims" );
            // m_canvas = null;
            // need to synchronize??
            val paint = Paint()
            paint.textSize = mMediumFontHt.toFloat()
            val scratch = Rect()
            val timerTxt = "-00:00"
            paint.getTextBounds(timerTxt, 0, timerTxt.length, scratch)
            val timerWidth = scratch.width()
            val fontWidth =
                min(mDefaultFontHt.toDouble(), (timerWidth / timerTxt.length).toDouble())
                    .toInt()
            Log.d(
                TAG, "layoutBoardOnce(): posting JNICmd.CMD_LAYOUT(w=%d, h=%d)",
                width, height
            )
            handle(
                JNICmd.CMD_LAYOUT, width, height,
                fontWidth, mDefaultFontHt
            )
            // We'll be back....
        } else {
            Log.d(TAG, "layoutBoardOnce(): DOING IT")
            // If board size has changed we need a new bitmap
            val bmHeight = 1 + mDims!!.height
            val bmWidth = 1 + mDims!!.width
            if (null != sBitmap) {
                if (sBitmap!!.getHeight() != bmHeight
                    || sBitmap!!.getWidth() != bmWidth
                ) {
                    sBitmap = null
                    mCanvas = null
                }
            }
            if (null == sBitmap) {
                sBitmap = Bitmap.createBitmap(
                    bmWidth, bmHeight,
                    Bitmap.Config.ARGB_8888
                )
            } else if (sIsFirstDraw) {
                // clear so prev game doesn't seem to appear briefly.  Color
                // doesn't seem to matter....
                sBitmap!!.eraseColor(0)
            }
            if (null == mCanvas) {
                mCanvas = BoardCanvas(
                    mParent!!, sBitmap!!, mJniThread,
                    mDims, mNRP
                )
            } else {
                mCanvas!!.setJNIThread(mJniThread)
            }
            handle(JNICmd.CMD_SETDRAW, mCanvas!!)
            handle(JNICmd.CMD_DRAW)

            // set so we know we're done
            mLayoutWidth = width
            mLayoutHeight = height
            layoutDone = true
        }
        // Log.d( TAG, "layoutBoardOnce()=>%b", layoutDone );
        return layoutDone
    } // layoutBoardOnce

    // BoardHandler interface implementation
    override fun startHandling(
        parent: Activity, thread: JNIThread,
        connTypes: CommsConnTypeSet?,
        nrp: NewRecentsProc?
    ) {
        Log.d(TAG, "startHandling(thread=%H, parent=%s)", thread, parent)
        mParent = parent
        mJniThread = thread
        mJniGamePtr = thread.getGamePtr()
        mGi = thread.getGI()
        mIsSolo = CurGameInfo.DeviceRole.SERVER_STANDALONE == mGi?.serverRole
        mConnTypes = connTypes
        mLayoutWidth = 0
        mLayoutHeight = 0
        mNRP = nrp
        sIsFirstDraw = sCurGameID != mGi!!.gameID
        sCurGameID = mGi!!.gameID

        // Set the jni layout if we already have one
        if (null != mDims) {
            handle(JNICmd.CMD_LAYOUT, mDims!!)
        }

        // Make sure we draw.  Sometimes when we're reloading after
        // an obsuring Activity goes away we otherwise won't.
        invalidate()
    }

    override fun stopHandling() {
        mJniThread = null
        mJniGamePtr = null
        mCanvas?.setJNIThread(null)
    }

    // SyncedDraw interface implementation
    override fun doJNIDraw() {
        var drew = false
        synchronized(this) {
            if (null != mJniGamePtr) {
                drew = XwJNI.board_draw(mJniGamePtr)
            }
        }

        // Force update now that we have bits to copy. I don't know why (yet),
        // but on older versions of Android we need to run this even if drew
        // is false
        mParent?.runOnUiThread(mInvalidator)
    }

    override fun dimsChanged(dims: BoardDims) {
        mDims = dims
        mParent!!.runOnUiThread { requestLayout() }
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

    private fun handle(cmd: JNICmd, vararg args: Any) {
        Assert.assertVarargsNotNullNR(args)
        if (null == mJniThread) {
            Log.w(TAG, "not calling handle(%s)", cmd.toString())
            printStack(TAG)
        } else {
            mJniThread!!.handle(cmd, *args)
        }
    }

    companion object {
        private val TAG = BoardView::class.java.getSimpleName()
        private const val MIN_FONT_DIPS = 10.0f
        private const val MULTI_INACTIVE = -1
        private var sIsFirstDraw = false
        private var sCurGameID = 0
        private var sBitmap: Bitmap? = null // the board
        private const val PINCH_THRESHOLD = 40
    }
}

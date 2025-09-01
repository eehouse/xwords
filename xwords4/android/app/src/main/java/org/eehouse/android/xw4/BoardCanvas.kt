/* Copyright 2009 - 2021 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Paint.Align
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.os.Build
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import org.eehouse.android.xw4.DbgUtils.assertOnUIThread
import org.eehouse.android.xw4.jni.BoardDims
import org.eehouse.android.xw4.jni.BoardHandler.NewRecentsProc
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.CommonPrefs.TileValueType
import org.eehouse.android.xw4.jni.DrawCtx
import org.eehouse.android.xw4.jni.DrawCtx.DrawScoreInfo
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.TmpDict
import org.eehouse.android.xw4.jni.TmpDict.DictWrapper
import org.eehouse.android.xw4.loc.LocUtils
import java.io.ByteArrayOutputStream
import java.lang.reflect.InvocationTargetException
import java.lang.reflect.Method
import kotlin.math.abs
import kotlin.math.max

open class BoardCanvas private constructor(
    private val mContext: Context, private val mActivity: Activity?,
    private val mBitmap: Bitmap, private var mGR: GameRef?, dims: BoardDims?,
    private val mNRP: NewRecentsProc?,
    private val mDrawProgProc: DrawProgress?,
    private var mDT: Int = DrawCtx.DT_SCREEN // DrawTarget
) : Canvas(mBitmap), DrawCtx {
    private val mFillPaint: Paint
    private val mStrokePaint: Paint
    private val mDrawPaint: Paint
    private var mTileStrokePaint: Paint? = null
    private var mScores: Array<Array<String?>?>? = null
    private var mRemText: String? = null
    private val mMediumFontHt: Int
    private val mDefaultFontHt: Int
    private var mMinRemWidth = 0
    private val mBoundsScratch = Rect()
    private var mLetterRect: Rect? = null
    private var mValRect: Rect? = null
    private val mBonusColors: IntArray
    private val mPlayerColors: IntArray
    private val mOtherColors: IntArray
    private val mBonusSummaries: Array<String?>
    private val mPrefs: CommonPrefs
    private var mLastSecsLeft = 0
    private var mLastTimerPlayer = 0
    private var mLastTimerTurnDone = false
    private var mInTrade = false
    private var mDarkOnLight = false
    private val mOrigin: Drawable
    private var mBlackArrow = false
    private var mRightArrow: Drawable? = null
    private var mDownArrow: Drawable? = null
    var curPlayer = -1
        private set
    private var mPendingScore = 0
    private var mDict: DictWrapper? = null
    protected var mDictChars: Array<String>? = null
    private val mHasSmallScreen: Boolean
    private var mBackgroundUsed = 0x00000000
    private var mPendingCount = 0
    private var mSawRecents = false

    interface DrawProgress {
        fun drawDone()
    }

    // FontDims: exists to translate space available to the largest
    // font we can draw within that space taking advantage of our use
    // being limited to a known small subset of glyphs.  We need two
    // numbers from this: the textHeight to pass to Paint.setTextSize,
    // and the descent to use when drawing.  Both can be calculated
    // proportionally.  We know the ht we passed to Paint to get the
    // height we've now measured; that gives a percent to multiply any
    // future wantHt by.  Ditto for the descent
    inner class FontDims internal constructor(
        askedHt: Float,
        topRow: Int,
        bottomRow: Int,
        width: Float
    ) {
        private val mHtProportion: Float
        private val mDescentProportion: Float
        private val mWidthProportion: Float

        init {
            // DbgUtils.logf( "FontDims(): askedHt=" + askedHt );
            // DbgUtils.logf( "FontDims(): topRow=" + topRow );
            // DbgUtils.logf( "FontDims(): bottomRow=" + bottomRow );
            // DbgUtils.logf( "FontDims(): width=" + width );
            val gotHt = (bottomRow - topRow + 1).toFloat()
            mHtProportion = gotHt / askedHt
            Assert.assertTrue(bottomRow + 1 >= askedHt)
            val descent = bottomRow + 1 - askedHt
            // DbgUtils.logf( "descent: " + descent );
            mDescentProportion = descent / askedHt
            Assert.assertTrue(mDescentProportion >= 0)
            mWidthProportion = width / askedHt
        }

        fun heightFor(ht: Int): Int {
            return (ht / mHtProportion).toInt()
        }

        fun descentFor(ht: Int): Int {
            return (ht * mDescentProportion).toInt()
        }

        fun widthFor(width: Int): Int {
            return (width / mWidthProportion).toInt()
        }
    }

    var mFontDims: FontDims? = null

    // Used by ThumbCanvas subclass
    constructor(context: Context, bitmap: Bitmap, dt: Int) :
        this(context, null, bitmap, null, null, null, null, dt)
    constructor(
        activity: Activity, bitmap: Bitmap, gr: GameRef,
        dims: BoardDims?, nrp: NewRecentsProc?, drawProgProc: DrawProgress
    ) : this(activity, activity, bitmap, gr, dims, nrp, drawProgProc)

    fun setGR(gr: GameRef?) {
        assertOnUIThread()
        mGR = gr
        updateDictChars()
    }

    fun curPending(): Int {
        return mPendingScore
    }

    fun setInTrade(inTrade: Boolean) {
        if (mInTrade != inTrade) {
            mInTrade = inTrade
            mGR?.invalAll()
        }
    }

    // DrawCtxt interface implementation

    override fun beginDraw(): Boolean {
        Log.d(TAG, "beginDraw() called (returns true)")
        return true
    }

    override fun endDraw() {
        Log.d(TAG, "endDraw() called")
        mDrawProgProc?.drawDone()
    }

    override fun scoreBegin(
        rect: Rect, numPlayers: Int, scores: IntArray,
        remCount: Int
    ): Boolean {
        fillRectOther(rect, CommonPrefs.COLOR_BACKGRND)
        mScores = arrayOfNulls(numPlayers)
        return true
    }

    override fun measureRemText(
        rect: Rect, nTilesLeft: Int, width: IntArray,
        height: IntArray
    ): Boolean {
        val showREM = 0 <= nTilesLeft
        if (showREM) {
            // should cache a formatter
            mRemText = String.format("%d", nTilesLeft)
            mFillPaint.textSize = mMediumFontHt.toFloat()
            mFillPaint.getTextBounds(mRemText, 0, mRemText!!.length,
                                     mBoundsScratch)
            var minWidth = mBoundsScratch.width()
            if (minWidth < mMinRemWidth) {
                minWidth = mMinRemWidth // it's a button; make it bigger
            }
            width[0] = minWidth
            height[0] = mBoundsScratch.height()
        }
        return showREM
    }

    override fun drawRemText(
        rInner: Rect, rOuter: Rect, nTilesLeft: Int,
        focussed: Boolean
    ) {
        val indx = if (focussed) CommonPrefs.COLOR_FOCUS else CommonPrefs.COLOR_TILE_BACK
        fillRectOther(rOuter, indx)
        mFillPaint.setColor(adjustColor(BLACK))
        drawCentered(mRemText, rInner, null)
    }

    override fun measureScoreText(
        rect: Rect, dsi: DrawScoreInfo,
        width: IntArray, height: IntArray
    ) {
        val scoreInfo = arrayOfNulls<String>(if (dsi.isTurn) 1 else 2)
        var indx = 0
        val sb = StringBuffer()

        // If it's my turn I get one line.  Otherwise squeeze into
        // two.
        if (dsi.isTurn) {
            sb.append(dsi.name)
            sb.append(":")
        } else {
            scoreInfo[indx++] = dsi.name
        }
        sb.append(dsi.totalScore)
        if (dsi.nTilesLeft >= 0) {
            sb.append(":")
            sb.append(dsi.nTilesLeft)
        }
        scoreInfo[indx] = sb.toString()
        mScores!![dsi.playerNum] = scoreInfo
        var rectHt = rect.height()
        if (!dsi.isTurn) {
            rectHt /= 2
        }
        var textHeight = rectHt - SCORE_HT_DROP
        if (textHeight < mDefaultFontHt) {
            textHeight = mDefaultFontHt
        }
        mFillPaint.textSize = textHeight.toFloat()
        var needWidth = 0
        for (ii in scoreInfo.indices) {
            mFillPaint.getTextBounds(
                scoreInfo[ii], 0, scoreInfo[ii]!!.length,
                mBoundsScratch
            )
            if (needWidth < mBoundsScratch.width()) {
                needWidth = mBoundsScratch.width()
            }
        }
        if (needWidth > rect.width()) {
            needWidth = rect.width()
        }
        width[0] = needWidth
        height[0] = rect.height()
    }

    override fun score_drawPlayer(
        rInner: Rect, rOuter: Rect,
        gotPct: Int, dsi: DrawScoreInfo
    ) {
        if (0 != dsi.flags and DrawCtx.CELL_ISCURSOR) {
            fillRectOther(rOuter, CommonPrefs.COLOR_FOCUS)
        } else if (DEBUG_DRAWFRAMES && dsi.selected) {
            fillRectOther(rOuter, CommonPrefs.COLOR_FOCUS)
        }
        val texts = mScores!![dsi.playerNum]!!
        var color = mPlayerColors[dsi.playerNum]
        if (!mPrefs.allowPeek) {
            color = adjustColor(color)
        }
        mFillPaint.setColor(color)
        val height = rOuter.height() / texts.size
        rOuter.bottom = rOuter.top + height
        texts.map { text ->
            drawCentered(text, rOuter, null)
            rOuter.offset(0, height)
        }
        if (DEBUG_DRAWFRAMES) {
            mStrokePaint.setColor(BLACK)
            drawRect(rInner, mStrokePaint)
        }
    }

    override fun drawTimer(
        rect: Rect, player: Int,
        secondsLeft: Int, turnDone: Boolean
    ) {
        mActivity?.let { activity ->
            if (mLastSecsLeft != secondsLeft
                    || mLastTimerPlayer != player
                    || mLastTimerTurnDone != turnDone) {
                val rectCopy = Rect(rect)
                val secondsLeftCopy = secondsLeft
                mGR?.let {
                    Utils.launch {
                        mLastSecsLeft = secondsLeftCopy
                        mLastTimerPlayer = player
                        mLastTimerTurnDone = turnDone
                        val negSign = if (secondsLeftCopy < 0) "-" else ""
                        val secondsLeft = abs(secondsLeftCopy.toDouble()).toInt()
                        val time = String.format(
                            "%s%d:%02d", negSign,
                            secondsLeft / 60, secondsLeft % 60
                        )
                        fillRectOther(rectCopy, CommonPrefs.COLOR_BACKGRND)
                        mFillPaint.setColor(mPlayerColors[player])
                        rectCopy.inset(0, rectCopy.height() / 5)
                        drawCentered(time, rectCopy, null)
                        it.draw()
                    }
                }
            }
        }
    }

    override fun drawCell(
        rect: Rect, text: String?, tile: Int, tileValue: Int,
        owner: Int, bonus: Int, flags: Int, tvType: TileValueType
    ): Boolean {
        var text: String? = text
        var owner = owner
        val canDraw = figureFontDims()
        if (canDraw) {
            val backColor: Int
            val empty = 0 != flags and (DrawCtx.CELL_DRAGSRC or DrawCtx.CELL_ISEMPTY)
            val pending = 0 != flags and DrawCtx.CELL_PENDING
            val recent = 0 != flags and DrawCtx.CELL_RECENT
            mSawRecents = recent || mSawRecents
            var bonusStr: String? = null
            if (mInTrade) {
                fillRectOther(rect, CommonPrefs.COLOR_BACKGRND)
            }
            if (owner < 0) {
                owner = 0
            }
            var foreColor = mPlayerColors[owner]
            if (0 != flags and DrawCtx.CELL_ISCURSOR) {
                backColor = mOtherColors[CommonPrefs.COLOR_FOCUS]
            } else if (empty) {
                if (0 == bonus) {
                    backColor = mOtherColors[CommonPrefs.COLOR_NOTILE]
                } else {
                    backColor = mBonusColors[bonus]
                    bonusStr = mBonusSummaries[bonus]
                }
            } else if (pending) {
                ++mPendingCount
                if (darkOnLight()) {
                    foreColor = WHITE
                    backColor = BLACK
                } else {
                    foreColor = BLACK
                    backColor = WHITE
                }
            } else {
                val indx =
                    if (recent) CommonPrefs.COLOR_TILE_BACK_RECENT else CommonPrefs.COLOR_TILE_BACK
                backColor = mOtherColors[indx]
            }
            fillRect(rect, adjustColor(backColor))
            if (empty) {
                if (DrawCtx.CELL_ISSTAR and flags != 0) {
                    mOrigin.bounds = rect
                    mOrigin.alpha = if (mInTrade) IN_TRADE_ALPHA shr 24 else 255
                    mOrigin.draw(this@BoardCanvas)
                } else if (null != bonusStr) {
                    val color = mOtherColors[CommonPrefs.COLOR_BONUSHINT]
                    mFillPaint.setColor(adjustColor(color))
                    val brect = Rect(rect)
                    val inset = (brect.height() / 3.5).toFloat()
                    brect.inset(0, inset.toInt())
                    drawCentered(bonusStr, brect, mFontDims)
                }
            } else {
                var value: String? = String.format("%d", tileValue)
                when (tvType) {
                    TileValueType.TVT_BOTH -> {}
                    TileValueType.TVT_FACES -> value = null
                    TileValueType.TVT_VALUES -> {
                        text = value
                        value = null
                    }
                }
                mFillPaint.setColor(adjustColor(foreColor))
                if (null == value) {
                    drawCentered(text, rect, mFontDims)
                } else {
                    var smaller = Rect(rect)
                    smaller.bottom -= smaller.height() / 4
                    smaller.right -= smaller.width() / 4
                    drawCentered(text, smaller, mFontDims)
                    smaller = Rect(rect)
                    smaller.left += 2 * smaller.width() / 3
                    smaller.top += 2 * smaller.height() / 3
                    drawCentered(value, smaller, mFontDims)
                }
            }
            if (DrawCtx.CELL_ISBLANK and flags != 0) {
                markBlank(rect, backColor)
            }

            // frame the cell
            val frameColor = mOtherColors[CommonPrefs.COLOR_CELLLINE]
            mStrokePaint.setColor(adjustColor(frameColor))

            // PENDING: fetch/calculate this a lot less frequently!!
            val width = XWPrefs.getPrefsInt(mContext, R.string.key_board_line_width, 1)
            mStrokePaint.strokeWidth = width.toFloat()
            drawRect(rect, mStrokePaint)
            drawCrosshairs(rect, flags)
        }
        return canDraw
    } // drawCell

    override fun drawBoardArrow(
        rect: Rect, bonus: Int, vert: Boolean,
        hintAtts: Int, flags: Int
    ) {
        // figure out if the background is more dark than light
        val useDark = darkOnLight()
        if (mBlackArrow != useDark) {
            mBlackArrow = useDark
            mRightArrow = null
            mDownArrow = mRightArrow
        }
        val arrow =
            if (vert) {
                if (null == mDownArrow) {
                    mDownArrow = loadAndRecolor(R.drawable.ic_downarrow, useDark)
                }
                mDownArrow!!
            } else {
                if (null == mRightArrow) {
                    mRightArrow = loadAndRecolor(R.drawable.ic_rightarrow, useDark)
                }
                mRightArrow!!
            }
        rect.inset(2, 2)
        arrow.bounds = rect
        arrow.draw(this@BoardCanvas)
        postNAHint(R.string.not_again_arrow, R.string.key_notagain_arrow)
    }

    override fun trayBegin(rect: Rect, owner: Int, score: Int): Boolean {
        curPlayer = owner
        mPendingScore = score
        mTileStrokePaint?.let {
            // force new color just in case it's changed
            it.setColor(mOtherColors[CommonPrefs.COLOR_CELLLINE])
        }
        return true
    }

    override fun drawTile(rect: Rect, text: String?, `val`: Int, flags: Int): Boolean {
        return drawTileImpl(rect, text, `val`, flags, true)
    }

    override fun drawTileMidDrag(
        rect: Rect, text: String?, `val`: Int, owner: Int,
        flags: Int
    ): Boolean {
        return drawTileImpl(rect, text, `val`, flags, false)
    }

    override fun drawTileBack(rect: Rect, flags: Int): Boolean {
        return drawTileImpl(rect, "?", -1, flags, true)
    }

    override fun drawTrayDivider(rect: Rect, flags: Int) {
        val isCursor = 0 != flags and DrawCtx.CELL_ISCURSOR
        val selected = 0 != flags and (DrawCtx.CELL_PENDING or DrawCtx.CELL_RECENT)
        val index = if (isCursor) CommonPrefs.COLOR_FOCUS else CommonPrefs.COLOR_BACKGRND
        fillRectOther(rect, index)
        rect.inset(rect.width() / 4, 1)
        if (selected) {
            drawRect(rect, mStrokePaint)
        } else {
            fillRect(rect, mPlayerColors[curPlayer])
        }
    }

    override fun score_pendingScore(
        rect: Rect, score: Int, playerNum: Int,
        curTurn: Boolean, flags: Int
    ) {
        // Log.d( TAG, "score_pendingScore(playerNum=%d, curTurn=%b)",
        //        playerNum, curTurn );
        val otherIndx =
            if (0 == flags and DrawCtx.CELL_ISCURSOR) CommonPrefs.COLOR_BACKGRND else CommonPrefs.COLOR_FOCUS
        ++rect.top
        fillRectOther(rect, otherIndx)
        var playerColor = mPlayerColors[playerNum]
        if (!curTurn) {
            playerColor = playerColor and NOT_TURN_ALPHA
        }
        mFillPaint.setColor(playerColor)
        val text = if (score >= 0) String.format("%d", score) else "??"
        rect.bottom -= rect.height() / 2
        drawCentered(text, rect, null)
        rect.offset(0, rect.height())
        drawCentered(
            LocUtils.getString(mContext, R.string.pts),
            rect, null
        )
    }

    override fun objFinished( /*BoardObjectType*/
                              typ: Int, rect: Rect
    ) {
        if (DrawCtx.OBJ_BOARD == typ) {
            // On squat screens, where I can't use the full width for
            // the board (without scrolling), the right-most cells
            // don't draw their right borders due to clipping, so draw
            // for them.
            val frameColor = mOtherColors[CommonPrefs.COLOR_CELLLINE]
            mStrokePaint.setColor(adjustColor(frameColor))
            val xx = rect.left + rect.width() - 1
            drawLine(
                xx.toFloat(),
                rect.top.toFloat(),
                xx.toFloat(),
                (rect.top + rect.height()).toFloat(),
                mStrokePaint
            )

            // Remove this for now. It comes up at the wrong time for new
            // installs. Need to delay it. PENDING
            if (false && mPendingCount > 0) {
                mPendingCount = 0
                postNAHint(
                    R.string.not_again_longtap_lookup,
                    R.string.key_na_longtap_lookup
                )
            }
            if (mSawRecents) {
                mNRP?.sawNew()
            }
            mSawRecents = false
        }
    }

    override fun getThumbData(): ByteArray {
        Assert.assertTrue( mDT == DrawCtx.DT_THUMB )
        val bas = ByteArrayOutputStream()
        mBitmap.compress(Bitmap.CompressFormat.PNG, 0, bas)
        val result = bas.toByteArray()
        return result
    }

    override fun getThumbSize(): Int {
        Assert.failDbg()        // should never be called
        return 0
    }

    override fun dictChanged(newPtr: Long) {
        // Other Case meant to be overridden!!
        Assert.assertTrue( mDT == DrawCtx.DT_SCREEN )
        val curPtr = mDict?.dictPtr ?: 0L
        var doPost = false
        if (curPtr != newPtr) {
            if (0L == newPtr) {
                mFontDims = null
                mDictChars = null
            } else if (0L == curPtr
                || !TmpDict.dict_tilesAreSame(curPtr, newPtr)
            ) {
                mFontDims = null
                mDictChars = null
                doPost = true
            }
            mDict?.release()
            mDict = DictWrapper(newPtr)
        }

        // If we're on the UI thread this is run inline, so make sure it's
        // after mDict is set above.
        if (doPost) {
            mActivity!!.runOnUiThread { updateDictChars() }
        }
    }

    private fun updateDictChars() {
        if (null == mGR) {
            // Log.d( TAG, "updateDictChars(): mGR still null!!" );
        } else if (null == mDict) {
            // Log.d( TAG, "updateDictChars(): mDict still null!!" );
        } else {
            mDictChars = mDict!!.getChars()
            mGR!!.invalAll()
        }
    }

    private fun saveImpl(rect: Rect) {
        if (Build.VERSION.SDK_INT >= 21) {
            saveLayer(RectF(rect), null)
        } else {
            if (null == sSaveMethod) {
                try {
                    val cls = Class.forName("android.graphics.Canvas")
                    sSaveMethod = cls.getDeclaredMethod(
                        "save", *arrayOf<Class<*>?>(
                            Int::class.javaPrimitiveType
                        )
                    )
                } catch (ex: NoSuchMethodException) {
                    Log.e(TAG, "%s", ex)
                    Assert.failDbg()
                } catch (ex: ClassNotFoundException) {
                    Log.e(TAG, "%s", ex)
                    Assert.failDbg()
                }
            }
            val CLIP_SAVE_FLAG = 0x02
            try {
                sSaveMethod!!.invoke(this, CLIP_SAVE_FLAG)
                // Log.d( TAG, "saveImpl() worked" );
            } catch (ex: InvocationTargetException) {
                Log.e(TAG, "%s", ex)
                Assert.failDbg()
            } catch (ex: IllegalAccessException) {
                Log.e(TAG, "%s", ex)
                Assert.failDbg()
            }
        }
    }

    private fun drawTileImpl(
        rect: Rect, text: String?, `val`: Int,
        flags: Int, clearBack: Boolean
    ): Boolean {
        val canDraw = figureFontDims()
        if (canDraw) {
            val notEmpty = flags and DrawCtx.CELL_ISEMPTY == 0
            val isCursor = flags and DrawCtx.CELL_ISCURSOR != 0
            saveImpl(rect)
            rect.top += 1
            clipRect(rect)
            if (clearBack) {
                fillRectOther(rect, CommonPrefs.COLOR_BACKGRND)
            }
            if (isCursor || notEmpty) {
                var color =
                    mOtherColors[if (isCursor) CommonPrefs.COLOR_FOCUS else CommonPrefs.COLOR_TILE_BACK]
                if (!clearBack) {
                    color = color and 0x7FFFFFFF // translucent if being dragged.
                }
                fillRect(rect, color)
                mFillPaint.setColor(mPlayerColors[curPlayer])
                if (notEmpty) {
                    positionDrawTile(rect, text, `val`)
                    val paint = getTileStrokePaint(rect)
                    drawRect(rect, paint!!) // frame
                    if (0 != flags and (DrawCtx.CELL_PENDING or DrawCtx.CELL_RECENT)) {
                        val width = paint.strokeWidth.toInt()
                        rect.inset(width, width)
                        drawRect(rect, paint) // frame
                    }
                }
            }
            restoreToCount(1) // in case new canvas....
        }
        return canDraw
    } // drawTileImpl

    private fun drawCrosshairs(rect: Rect, flags: Int) {
        val color = mOtherColors[CommonPrefs.COLOR_FOCUS]
        if (0 != flags and DrawCtx.CELL_CROSSHOR) {
            val hairRect = Rect(rect)
            hairRect.inset(0, hairRect.height() / 3)
            fillRect(hairRect, color)
        }
        if (0 != flags and DrawCtx.CELL_CROSSVERT) {
            val hairRect = Rect(rect)
            hairRect.inset(hairRect.width() / 3, 0)
            fillRect(hairRect, color)
        }
    }

    private fun drawCentered(text: String?, rect: Rect, fontDims: FontDims?) {
        drawIn(text, rect, fontDims, Align.CENTER)
    }

    private fun drawIn(
        text: String?, rect: Rect, fontDims: FontDims?,
        align: Align
    ) {
        var descent = -1
        val textSize: Int
        if (null == fontDims) {
            textSize = rect.height() - SCORE_HT_DROP
        } else {
            val height = rect.height() - 4 // borders and padding, 2 each
            descent = fontDims.descentFor(height)
            textSize = fontDims.heightFor(height)
            // DbgUtils.logf( "using descent: " + descent + " and textSize: "
            //             + textSize + " in height " + height );
        }
        mFillPaint.textSize = textSize.toFloat()
        if (descent == -1) {
            descent = mFillPaint.getFontMetricsInt().descent
        }
        descent += 2
        mFillPaint.getTextBounds(text, 0, text!!.length, mBoundsScratch)
        val extra = rect.width() - mBoundsScratch.width()
        if (0 >= extra) {
            mFillPaint.textAlign = Align.LEFT
            drawScaled(text, rect, mBoundsScratch, descent)
        } else {
            val bottom = rect.bottom - descent
            var origin = rect.left
            origin += if (Align.CENTER == align) {
                rect.width() / 2
            } else {
                extra / 5 - mBoundsScratch.left
            }
            mFillPaint.textAlign = align
            drawText(text, origin.toFloat(), bottom.toFloat(), mFillPaint)
        }
    } // drawIn

    private fun drawScaled(
        text: String?, rect: Rect,
        textBounds: Rect, descent: Int
    ) {
        textBounds.bottom = rect.height()
        val bitmap = Bitmap.createBitmap(
            textBounds.width(),
            rect.height(),
            Bitmap.Config.ARGB_8888
        )
        val canvas = Canvas(bitmap)
        val bottom = textBounds.bottom - descent
        canvas.drawText(text!!, -textBounds.left.toFloat(), bottom.toFloat(), mFillPaint)
        drawBitmap(bitmap, null, rect, mDrawPaint)
    }

    private fun positionDrawTile(rect: Rect, text: String?, tileVal: Int) {
        var text: String? = text
        val offset = 2
        text?.let {
            if (null == mLetterRect) {
                mLetterRect = Rect(
                    0, 0, rect.width() - offset,
                    rect.height() * 3 / 4
                )
            }
            val letterRect = mLetterRect!!
            letterRect.offsetTo(rect.left + offset, rect.top + offset)
            drawIn(it, letterRect, mFontDims, Align.LEFT)
            if (FRAME_TRAY_RECTS) {
                drawRect(letterRect, mStrokePaint)
            }
        }
        if (tileVal >= 0) {
            val divisor = if (mHasSmallScreen) 3 else 4
            if (null == mValRect) {
                mValRect = Rect(
                    0, 0, rect.width() / divisor,
                    rect.height() / divisor
                )
                mValRect!!.inset(offset, offset)
            }
            val valRect = mValRect!!
            valRect.offsetTo(
                rect.right - rect.width() / divisor,
                rect.bottom - rect.height() / divisor
            )
            text = String.format("%d", tileVal)
            mFillPaint.textSize = valRect.height().toFloat()
            mFillPaint.textAlign = Align.RIGHT
            drawText(
                text, valRect.right.toFloat(), valRect.bottom.toFloat(),
                mFillPaint
            )
            if (FRAME_TRAY_RECTS) {
                drawRect(valRect, mStrokePaint)
            }
        }
    }

    private fun fillRectOther(rect: Rect, index: Int) {
        fillRect(rect, mOtherColors[index])
    }

    private fun fillRect(rect: Rect, color: Int) {
        mFillPaint.setColor(color)
        drawRect(rect, mFillPaint)
    }

    private fun figureFontDims(): Boolean {
        if (null == mFontDims && null != mDictChars) {
            val ht = 24
            val width = 20
            val paint = Paint() // CommonPrefs.getFontFlags()??
            paint.style = Paint.Style.STROKE
            paint.textAlign = Align.LEFT
            paint.textSize = ht.toFloat()
            val bitmap = Bitmap.createBitmap(
                width, ht * 3 / 2,
                Bitmap.Config.ARGB_8888
            )
            val canvas = Canvas(bitmap)

            // FontMetrics fmi = paint.getFontMetrics();
            // DbgUtils.logf( "ascent: " + fmi.ascent );
            // DbgUtils.logf( "bottom: " + fmi.bottom );
            // DbgUtils.logf( "descent: " + fmi.descent );
            // DbgUtils.logf( "leading: " + fmi.leading );
            // DbgUtils.logf( "top : " + fmi.top );

            // DbgUtils.logf( "using as baseline: " + ht );
            val bounds = Rect()
            var maxWidth = 0
            for (str in mDictChars!!) {
                if (str!!.length == 1 && str[0].code >= 32) {
                    canvas.drawText(str, 0f, ht.toFloat(), paint)
                    paint.getTextBounds(str, 0, 1, bounds)
                    if (maxWidth < bounds.right) {
                        maxWidth = bounds.right
                    }
                }
            }

            // for ( int row = 0; row < bitmap.getHeight(); ++row ) {
            //     StringBuffer sb = new StringBuffer( bitmap.getWidth() );
            //     for ( int col = 0; col < bitmap.getWidth(); ++col ) {
            //         int pixel = bitmap.getPixel( col, row );
            //         sb.append( pixel==0? "." : "X" );
            //     }
            //     DbgUtils.logf( sb.append(row).toString() );
            // }
            var topRow = 0
            findTop@ for (row in 0 until bitmap.getHeight()) {
                for (col in 0 until bitmap.getWidth()) {
                    if (0 != bitmap.getPixel(col, row)) {
                        topRow = row
                        break@findTop
                    }
                }
            }
            var bottomRow = 0
            findBottom@ for (row in bitmap.getHeight() - 1 downTo topRow + 1) {
                for (col in 0 until bitmap.getWidth()) {
                    if (0 != bitmap.getPixel(col, row)) {
                        bottomRow = row
                        break@findBottom
                    }
                }
            }
            mFontDims = FontDims(ht.toFloat(), topRow, bottomRow, maxWidth.toFloat())
        }
        return null != mFontDims
    } // figureFontDims

    private fun adjustColor(color: Int): Int {
        var color = color
        if (mInTrade) {
            color = color and IN_TRADE_ALPHA
        }
        return color
    }

    private fun darkOnLight(): Boolean {
        val background = mOtherColors[CommonPrefs.COLOR_NOTILE]
        if (background != mBackgroundUsed) {
            mBackgroundUsed = background
            mDarkOnLight = isLightColor(background)
        }
        return mDarkOnLight
    }

    private fun markBlank(rect: Rect, backColor: Int) {
        val oval = RectF(
            rect.left.toFloat(),
            rect.top.toFloat(),
            rect.right.toFloat(),
            rect.bottom.toFloat()
        )
        var curColor = 0
        val whiteOnBlack = !isLightColor(backColor)
        if (whiteOnBlack) {
            curColor = mStrokePaint.color
            mStrokePaint.setColor(WHITE)
        }
        drawArc(oval, 0f, 360f, false, mStrokePaint)
        if (whiteOnBlack) {
            mStrokePaint.setColor(curColor)
        }
    }

    private fun loadAndRecolor(resID: Int, useDark: Boolean): Drawable {
        val res = mContext.resources
        var arrow = ContextCompat.getDrawable(mContext, resID)!!
        if (!useDark) {
            val bitmap = Bitmap.createBitmap(
                arrow.intrinsicWidth,
                arrow.intrinsicHeight,
                Bitmap.Config.ARGB_8888
            )
            val canvas = Canvas(bitmap)
            arrow.setBounds(0, 0, canvas.width, canvas.height)
            arrow.draw(canvas)
            for (xx in 0 until bitmap.getWidth()) {
                for (yy in 0 until bitmap.getHeight()) {
                    if (BLACK == bitmap.getPixel(xx, yy)) {
                        bitmap.setPixel(xx, yy, WHITE)
                    }
                }
            }
            arrow = BitmapDrawable(mContext.resources, bitmap)
        }
        return arrow
    }

    private fun isLightColor(color: Int): Boolean {
        var color = color
        var sum = 0
        for (ii in 0..2) {
            sum += color and 0xFF
            color = color shr 8
        }
        return sum > 127 * 3
    }

    private fun getTileStrokePaint(rect: Rect): Paint? {
        if (null == mTileStrokePaint) {
            val paint = Paint()
            paint.style = Paint.Style.STROKE
            paint.strokeWidth = max(2.0, (rect.width() / 20).toDouble()).toFloat()
            paint.setColor(mOtherColors[CommonPrefs.COLOR_CELLLINE])
            mTileStrokePaint = paint
        }
        return mTileStrokePaint
    }

    init {
        mHasSmallScreen = Utils.hasSmallScreen(mContext)
        val res = mContext.resources
        val scale = res.displayMetrics.density
        mDefaultFontHt = (MIN_FONT_DIPS * scale + 0.5f).toInt()
        mMediumFontHt = mDefaultFontHt * 3 / 2
        dims?.let {
            mMinRemWidth = it.cellSize
        }
        mDrawPaint = Paint()
        mFillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
        mStrokePaint = Paint()
        mStrokePaint.style = Paint.Style.STROKE
        mOrigin = ContextCompat.getDrawable(mContext, R.drawable.ic_origin)!!
        mPrefs = CommonPrefs.get(mContext)
        mPlayerColors = mPrefs.playerColors
        mBonusColors = mPrefs.bonusColors
        mOtherColors = mPrefs.otherColors
        val ids = intArrayOf(
            R.string.bonus_l2x_summary,
            R.string.bonus_w2x_summary,
            R.string.bonus_l3x_summary,
            R.string.bonus_w3x_summary,
            R.string.bonus_l4x_summary,
            R.string.bonus_w4x_summary
        )
        mBonusSummaries = arrayOfNulls(1 + ids.size)
        for (ii in ids.indices) {
            mBonusSummaries[ii + 1] = res.getString(ids[ii])
        }
    }

    private fun postNAHint(msgID: Int, keyID: Int) {
        if (!sShown.contains(keyID)) {
            sShown.add(keyID)
            if (mActivity is XWActivity) {
                val activity = mActivity
                activity.runOnUiThread {
                    activity.makeNotAgainBuilder(keyID, msgID)
                        .show()
                }
            }
        }
    }

    companion object {
        private val TAG = BoardCanvas::class.java.getSimpleName()
        private const val BLACK = -0x1000000
        private const val WHITE = -0x1
        private const val SCORE_HT_DROP = 2
        private const val DEBUG_DRAWFRAMES = false
        private const val NOT_TURN_ALPHA = 0x3FFFFFFF
        private const val IN_TRADE_ALPHA = 0x3FFFFFFF
        private const val FRAME_TRAY_RECTS = false // for debugging
        private const val MIN_FONT_DIPS = 14.0f
        private var sSaveMethod: Method? = null
        private val sShown: MutableSet<Int> = HashSet()
    }
}

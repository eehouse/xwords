/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009 - 2021 by Eric House (xwords@eehouse.org).  All rights
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
import org.eehouse.android.xw4.BoardCanvas
import org.eehouse.android.xw4.DbgUtils.assertOnUIThread
import org.eehouse.android.xw4.jni.BoardDims
import org.eehouse.android.xw4.jni.BoardHandler.NewRecentsProc
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.CommonPrefs.TileValueType
import org.eehouse.android.xw4.jni.DrawCtx
import org.eehouse.android.xw4.jni.DrawCtx.DrawScoreInfo
import org.eehouse.android.xw4.jni.JNIThread
import org.eehouse.android.xw4.jni.XwJNI.Companion.dict_getChars
import org.eehouse.android.xw4.jni.XwJNI.Companion.dict_tilesAreSame
import org.eehouse.android.xw4.jni.XwJNI.DictWrapper
import org.eehouse.android.xw4.loc.LocUtils
import java.lang.reflect.InvocationTargetException
import java.lang.reflect.Method
import kotlin.math.abs
import kotlin.math.max

open class BoardCanvas private constructor(
    private val m_context: Context, private val m_activity: Activity?, bitmap: Bitmap,
    private var m_jniThread: JNIThread?, dims: BoardDims?,
    private val mNRP: NewRecentsProc?
) : Canvas(bitmap), DrawCtx {
    private val m_fillPaint: Paint
    private val m_strokePaint: Paint
    private val m_drawPaint: Paint
    private var m_tileStrokePaint: Paint? = null
    private var m_scores: Array<Array<String?>>
    private var m_remText: String? = null
    private val m_mediumFontHt: Int
    private val m_defaultFontHt: Int
    private var m_minRemWidth = 0
    private val m_boundsScratch = Rect()
    private var m_letterRect: Rect? = null
    private var m_valRect: Rect? = null
    private val m_bonusColors: IntArray
    private val m_playerColors: IntArray
    private val m_otherColors: IntArray
    private val m_bonusSummaries: Array<String?>
    private val m_prefs: CommonPrefs
    private var m_lastSecsLeft = 0
    private var m_lastTimerPlayer = 0
    private var m_lastTimerTurnDone = false
    private var m_inTrade = false
    private var m_darkOnLight = false
    private val m_origin: Drawable
    private var m_blackArrow = false
    private var m_rightArrow: Drawable? = null
    private var m_downArrow: Drawable? = null
    var curPlayer = -1
        private set
    private var m_pendingScore = 0
    private var m_dict: DictWrapper? = null
    protected var m_dictChars: Array<String?>?
    private val m_hasSmallScreen: Boolean
    private var m_backgroundUsed = 0x00000000
    private var mPendingCount = 0
    private var mSawRecents = false

    // FontDims: exists to translate space available to the largest
    // font we can draw within that space taking advantage of our use
    // being limited to a known small subset of glyphs.  We need two
    // numbers from this: the textHeight to pass to Paint.setTextSize,
    // and the descent to use when drawing.  Both can be calculated
    // proportionally.  We know the ht we passed to Paint to get the
    // height we've now measured; that gives a percent to multiply any
    // future wantHt by.  Ditto for the descent
    private inner class FontDims internal constructor(
        askedHt: Float,
        topRow: Int,
        bottomRow: Int,
        width: Float
    ) {
        private val m_htProportion: Float
        private val m_descentProportion: Float
        private val m_widthProportion: Float

        init {
            // DbgUtils.logf( "FontDims(): askedHt=" + askedHt );
            // DbgUtils.logf( "FontDims(): topRow=" + topRow );
            // DbgUtils.logf( "FontDims(): bottomRow=" + bottomRow );
            // DbgUtils.logf( "FontDims(): width=" + width );
            val gotHt = (bottomRow - topRow + 1).toFloat()
            m_htProportion = gotHt / askedHt
            Assert.assertTrue(bottomRow + 1 >= askedHt)
            val descent = bottomRow + 1 - askedHt
            // DbgUtils.logf( "descent: " + descent );
            m_descentProportion = descent / askedHt
            Assert.assertTrue(m_descentProportion >= 0)
            m_widthProportion = width / askedHt
            // DbgUtils.logf( "m_htProportion: " + m_htProportion );
            // DbgUtils.logf( "m_descentProportion: " + m_descentProportion );
        }

        fun heightFor(ht: Int): Int {
            return (ht / m_htProportion).toInt()
        }

        fun descentFor(ht: Int): Int {
            return (ht * m_descentProportion).toInt()
        }

        fun widthFor(width: Int): Int {
            return (width / m_widthProportion).toInt()
        }
    }

    protected var m_fontDims: FontDims? = null

    constructor(context: Context, bitmap: Bitmap) : this(context, null, bitmap, null, null, null)
    constructor(
        activity: Activity, bitmap: Bitmap, jniThread: JNIThread?,
        dims: BoardDims?, nrp: NewRecentsProc?
    ) : this(activity, activity, bitmap, jniThread, dims, nrp)

    fun setJNIThread(jniThread: JNIThread?) {
        assertOnUIThread()
        if (null == jniThread) {
            // do nothing
        } else if (jniThread != m_jniThread) {
            Log.w(TAG, "changing threads")
        }
        m_jniThread = jniThread
        updateDictChars()
    }

    fun curPending(): Int {
        return m_pendingScore
    }

    fun setInTrade(inTrade: Boolean) {
        if (m_inTrade != inTrade) {
            m_inTrade = inTrade
            m_jniThread!!.handle(JNIThread.JNICmd.CMD_INVALALL)
        }
    }

    // DrawCtxt interface implementation
    override fun scoreBegin(
        rect: Rect, numPlayers: Int, scores: IntArray,
        remCount: Int
    ): Boolean {
        fillRectOther(rect, CommonPrefs.COLOR_BACKGRND)
        m_scores = arrayOfNulls(numPlayers)
        return true
    }

    override fun measureRemText(
        r: Rect, nTilesLeft: Int, width: IntArray,
        height: IntArray
    ): Boolean {
        val showREM = 0 <= nTilesLeft
        if (showREM) {
            // should cache a formatter
            m_remText = String.format("%d", nTilesLeft)
            m_fillPaint.textSize = m_mediumFontHt.toFloat()
            m_fillPaint.getTextBounds(
                m_remText, 0, m_remText!!.length,
                m_boundsScratch
            )
            var minWidth = m_boundsScratch.width()
            if (minWidth < m_minRemWidth) {
                minWidth = m_minRemWidth // it's a button; make it bigger
            }
            width[0] = minWidth
            height[0] = m_boundsScratch.height()
        }
        return showREM
    }

    override fun drawRemText(
        rInner: Rect, rOuter: Rect, nTilesLeft: Int,
        focussed: Boolean
    ) {
        val indx = if (focussed) CommonPrefs.COLOR_FOCUS else CommonPrefs.COLOR_TILE_BACK
        fillRectOther(rOuter, indx)
        m_fillPaint.setColor(adjustColor(BLACK))
        drawCentered(m_remText, rInner, null)
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
        m_scores[dsi.playerNum] = scoreInfo
        var rectHt = rect.height()
        if (!dsi.isTurn) {
            rectHt /= 2
        }
        var textHeight = rectHt - SCORE_HT_DROP
        if (textHeight < m_defaultFontHt) {
            textHeight = m_defaultFontHt
        }
        m_fillPaint.textSize = textHeight.toFloat()
        var needWidth = 0
        for (ii in scoreInfo.indices) {
            m_fillPaint.getTextBounds(
                scoreInfo[ii], 0, scoreInfo[ii]!!.length,
                m_boundsScratch
            )
            if (needWidth < m_boundsScratch.width()) {
                needWidth = m_boundsScratch.width()
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
        val texts = m_scores[dsi.playerNum]
        var color = m_playerColors[dsi.playerNum]
        if (!m_prefs.allowPeek) {
            color = adjustColor(color)
        }
        m_fillPaint.setColor(color)
        val height = rOuter.height() / texts.size
        rOuter.bottom = rOuter.top + height
        for (text in texts) {
            drawCentered(text, rOuter, null)
            rOuter.offset(0, height)
        }
        if (DEBUG_DRAWFRAMES) {
            m_strokePaint.setColor(BLACK)
            drawRect(rInner, m_strokePaint)
        }
    }

    override fun drawTimer(
        rect: Rect, player: Int,
        secondsLeft: Int, turnDone: Boolean
    ) {
        val activity = m_activity
        if (null == activity) {
            // Do nothing
        } else if (m_lastSecsLeft != secondsLeft || m_lastTimerPlayer != player || m_lastTimerTurnDone != turnDone) {
            val rectCopy = Rect(rect)
            val secondsLeftCopy = secondsLeft
            activity.runOnUiThread(Runnable {
                if (null != m_jniThread) {
                    m_lastSecsLeft = secondsLeftCopy
                    m_lastTimerPlayer = player
                    m_lastTimerTurnDone = turnDone
                    val negSign = if (secondsLeftCopy < 0) "-" else ""
                    val secondsLeft = abs(secondsLeftCopy.toDouble()).toInt()
                    val time = String.format(
                        "%s%d:%02d", negSign,
                        secondsLeft / 60, secondsLeft % 60
                    )
                    fillRectOther(rectCopy, CommonPrefs.COLOR_BACKGRND)
                    m_fillPaint.setColor(m_playerColors[player])
                    rectCopy.inset(0, rectCopy.height() / 5)
                    drawCentered(time, rectCopy, null)
                    m_jniThread!!.handle(JNIThread.JNICmd.CMD_DRAW)
                }
            })
        }
    }

    override fun drawCell(
        rect: Rect, text: String, tile: Int, tileValue: Int,
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
            if (m_inTrade) {
                fillRectOther(rect, CommonPrefs.COLOR_BACKGRND)
            }
            if (owner < 0) {
                owner = 0
            }
            var foreColor = m_playerColors[owner]
            if (0 != flags and DrawCtx.CELL_ISCURSOR) {
                backColor = m_otherColors[CommonPrefs.COLOR_FOCUS]
            } else if (empty) {
                if (0 == bonus) {
                    backColor = m_otherColors[CommonPrefs.COLOR_NOTILE]
                } else {
                    backColor = m_bonusColors[bonus]
                    bonusStr = m_bonusSummaries[bonus]
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
                backColor = m_otherColors[indx]
            }
            fillRect(rect, adjustColor(backColor))
            if (empty) {
                if (DrawCtx.CELL_ISSTAR and flags != 0) {
                    m_origin.bounds = rect
                    m_origin.alpha = if (m_inTrade) IN_TRADE_ALPHA shr 24 else 255
                    m_origin.draw(this@BoardCanvas)
                } else if (null != bonusStr) {
                    val color = m_otherColors[CommonPrefs.COLOR_BONUSHINT]
                    m_fillPaint.setColor(adjustColor(color))
                    val brect = Rect(rect)
                    val inset = (brect.height() / 3.5).toFloat()
                    brect.inset(0, inset.toInt())
                    drawCentered(bonusStr, brect, m_fontDims)
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
                m_fillPaint.setColor(adjustColor(foreColor))
                if (null == value) {
                    drawCentered(text, rect, m_fontDims)
                } else {
                    var smaller = Rect(rect)
                    smaller.bottom -= smaller.height() / 4
                    smaller.right -= smaller.width() / 4
                    drawCentered(text, smaller, m_fontDims)
                    smaller = Rect(rect)
                    smaller.left += 2 * smaller.width() / 3
                    smaller.top += 2 * smaller.height() / 3
                    drawCentered(value, smaller, m_fontDims)
                }
            }
            if (DrawCtx.CELL_ISBLANK and flags != 0) {
                markBlank(rect, backColor)
            }

            // frame the cell
            val frameColor = m_otherColors[CommonPrefs.COLOR_CELLLINE]
            m_strokePaint.setColor(adjustColor(frameColor))

            // PENDING: fetch/calculate this a lot less frequently!!
            val width = XWPrefs.getPrefsInt(m_activity, R.string.key_board_line_width, 1)
            m_strokePaint.strokeWidth = width.toFloat()
            drawRect(rect, m_strokePaint)
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
        if (m_blackArrow != useDark) {
            m_blackArrow = useDark
            m_rightArrow = null
            m_downArrow = m_rightArrow
        }
        val arrow: Drawable?
        if (vert) {
            if (null == m_downArrow) {
                m_downArrow = loadAndRecolor(R.drawable.ic_downarrow, useDark)
            }
            arrow = m_downArrow
        } else {
            if (null == m_rightArrow) {
                m_rightArrow = loadAndRecolor(R.drawable.ic_rightarrow, useDark)
            }
            arrow = m_rightArrow
        }
        rect.inset(2, 2)
        arrow!!.bounds = rect
        arrow.draw(this@BoardCanvas)
        postNAHint(R.string.not_again_arrow, R.string.key_notagain_arrow)
    }

    override fun trayBegin(rect: Rect, owner: Int, score: Int): Boolean {
        curPlayer = owner
        m_pendingScore = score
        if (null != m_tileStrokePaint) {
            // force new color just in case it's changed
            m_tileStrokePaint!!.setColor(m_otherColors[CommonPrefs.COLOR_CELLLINE])
        }
        return true
    }

    override fun drawTile(rect: Rect, text: String, `val`: Int, flags: Int): Boolean {
        return drawTileImpl(rect, text, `val`, flags, true)
    }

    override fun drawTileMidDrag(
        rect: Rect, text: String, `val`: Int, owner: Int,
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
            drawRect(rect, m_strokePaint)
        } else {
            fillRect(rect, m_playerColors[curPlayer])
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
        var playerColor = m_playerColors[playerNum]
        if (!curTurn) {
            playerColor = playerColor and NOT_TURN_ALPHA
        }
        m_fillPaint.setColor(playerColor)
        val text = if (score >= 0) String.format("%d", score) else "??"
        rect.bottom -= rect.height() / 2
        drawCentered(text, rect, null)
        rect.offset(0, rect.height())
        drawCentered(
            LocUtils.getString(m_context, R.string.pts),
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
            val frameColor = m_otherColors[CommonPrefs.COLOR_CELLLINE]
            m_strokePaint.setColor(adjustColor(frameColor))
            val xx = rect.left + rect.width() - 1
            drawLine(
                xx.toFloat(),
                rect.top.toFloat(),
                xx.toFloat(),
                (rect.top + rect.height()).toFloat(),
                m_strokePaint
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
            if (mSawRecents && null != mNRP) {
                mNRP.sawNew()
            }
            mSawRecents = false
        }
    }

    override fun dictChanged(newPtr: Long) {
        val curPtr = if (null == m_dict) 0 else m_dict!!.dictPtr
        var doPost = false
        if (curPtr != newPtr) {
            if (0L == newPtr) {
                m_fontDims = null
                m_dictChars = null
            } else if (0L == curPtr
                || !dict_tilesAreSame(curPtr, newPtr)
            ) {
                m_fontDims = null
                m_dictChars = null
                doPost = true
            }
            if (null != m_dict) {
                m_dict!!.release()
            }
            m_dict = DictWrapper(newPtr)
        }

        // If we're on the UI thread this is run inline, so make sure it's
        // after m_dict is set above.
        if (doPost) {
            m_activity!!.runOnUiThread { updateDictChars() }
        }
    }

    private fun updateDictChars() {
        if (null == m_jniThread) {
            // Log.d( TAG, "updateDictChars(): m_jniThread still null!!" );
        } else if (null == m_dict) {
            // Log.d( TAG, "updateDictChars(): m_dict still null!!" );
        } else {
            m_dictChars = dict_getChars(m_dict!!.dictPtr)
            // draw again
            m_jniThread!!.handle(JNIThread.JNICmd.CMD_INVALALL)
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
        rect: Rect, text: String, `val`: Int,
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
                    m_otherColors[if (isCursor) CommonPrefs.COLOR_FOCUS else CommonPrefs.COLOR_TILE_BACK]
                if (!clearBack) {
                    color = color and 0x7FFFFFFF // translucent if being dragged.
                }
                fillRect(rect, color)
                m_fillPaint.setColor(m_playerColors[curPlayer])
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
        val color = m_otherColors[CommonPrefs.COLOR_FOCUS]
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
        m_fillPaint.textSize = textSize.toFloat()
        if (descent == -1) {
            descent = m_fillPaint.getFontMetricsInt().descent
        }
        descent += 2
        m_fillPaint.getTextBounds(text, 0, text!!.length, m_boundsScratch)
        val extra = rect.width() - m_boundsScratch.width()
        if (0 >= extra) {
            m_fillPaint.textAlign = Align.LEFT
            drawScaled(text, rect, m_boundsScratch, descent)
        } else {
            val bottom = rect.bottom - descent
            var origin = rect.left
            origin += if (Align.CENTER == align) {
                rect.width() / 2
            } else {
                extra / 5 - m_boundsScratch.left
            }
            m_fillPaint.textAlign = align
            drawText(text, origin.toFloat(), bottom.toFloat(), m_fillPaint)
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
        canvas.drawText(text!!, -textBounds.left.toFloat(), bottom.toFloat(), m_fillPaint)
        drawBitmap(bitmap, null, rect, m_drawPaint)
    }

    private fun positionDrawTile(rect: Rect, text: String, `val`: Int) {
        var text: String? = text
        val offset = 2
        if (null != text) {
            if (null == m_letterRect) {
                m_letterRect = Rect(
                    0, 0, rect.width() - offset,
                    rect.height() * 3 / 4
                )
            }
            m_letterRect!!.offsetTo(rect.left + offset, rect.top + offset)
            drawIn(text, m_letterRect!!, m_fontDims, Align.LEFT)
            if (FRAME_TRAY_RECTS) {
                drawRect(m_letterRect!!, m_strokePaint)
            }
        }
        if (`val` >= 0) {
            val divisor = if (m_hasSmallScreen) 3 else 4
            if (null == m_valRect) {
                m_valRect = Rect(
                    0, 0, rect.width() / divisor,
                    rect.height() / divisor
                )
                m_valRect!!.inset(offset, offset)
            }
            m_valRect!!.offsetTo(
                rect.right - rect.width() / divisor,
                rect.bottom - rect.height() / divisor
            )
            text = String.format("%d", `val`)
            m_fillPaint.textSize = m_valRect!!.height().toFloat()
            m_fillPaint.textAlign = Align.RIGHT
            drawText(
                text, m_valRect!!.right.toFloat(), m_valRect!!.bottom.toFloat(),
                m_fillPaint
            )
            if (FRAME_TRAY_RECTS) {
                drawRect(m_valRect!!, m_strokePaint)
            }
        }
    }

    private fun fillRectOther(rect: Rect, index: Int) {
        fillRect(rect, m_otherColors[index])
    }

    private fun fillRect(rect: Rect, color: Int) {
        m_fillPaint.setColor(color)
        drawRect(rect, m_fillPaint)
    }

    private fun figureFontDims(): Boolean {
        if (null == m_fontDims && null != m_dictChars) {
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
            for (str in m_dictChars!!) {
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
            m_fontDims = FontDims(ht.toFloat(), topRow, bottomRow, maxWidth.toFloat())
        }
        return null != m_fontDims
    } // figureFontDims

    private fun adjustColor(color: Int): Int {
        var color = color
        if (m_inTrade) {
            color = color and IN_TRADE_ALPHA
        }
        return color
    }

    private fun darkOnLight(): Boolean {
        val background = m_otherColors[CommonPrefs.COLOR_NOTILE]
        if (background != m_backgroundUsed) {
            m_backgroundUsed = background
            m_darkOnLight = isLightColor(background)
        }
        return m_darkOnLight
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
            curColor = m_strokePaint.color
            m_strokePaint.setColor(WHITE)
        }
        drawArc(oval, 0f, 360f, false, m_strokePaint)
        if (whiteOnBlack) {
            m_strokePaint.setColor(curColor)
        }
    }

    private fun loadAndRecolor(resID: Int, useDark: Boolean): Drawable {
        val res = m_context.resources
        var arrow = res.getDrawable(resID)
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
            arrow = BitmapDrawable(bitmap)
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
        if (null == m_tileStrokePaint) {
            val paint = Paint()
            paint.style = Paint.Style.STROKE
            paint.strokeWidth = max(2.0, (rect.width() / 20).toDouble()).toFloat()
            paint.setColor(m_otherColors[CommonPrefs.COLOR_CELLLINE])
            m_tileStrokePaint = paint
        }
        return m_tileStrokePaint
    }

    init {
        m_hasSmallScreen = Utils.hasSmallScreen(m_context)
        val res = m_context.resources
        val scale = res.displayMetrics.density
        m_defaultFontHt = (MIN_FONT_DIPS * scale + 0.5f).toInt()
        m_mediumFontHt = m_defaultFontHt * 3 / 2
        if (null != dims) {
            m_minRemWidth = dims.cellSize
        }
        m_drawPaint = Paint()
        m_fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)
        m_strokePaint = Paint()
        m_strokePaint.style = Paint.Style.STROKE
        m_origin = res.getDrawable(R.drawable.ic_origin)
        m_prefs = CommonPrefs.get(m_context)
        m_playerColors = m_prefs.playerColors
        m_bonusColors = m_prefs.bonusColors
        m_otherColors = m_prefs.otherColors
        val ids = intArrayOf(
            R.string.bonus_l2x_summary,
            R.string.bonus_w2x_summary,
            R.string.bonus_l3x_summary,
            R.string.bonus_w3x_summary,
            R.string.bonus_l4x_summary,
            R.string.bonus_w4x_summary
        )
        m_bonusSummaries = arrayOfNulls(1 + ids.size)
        for (ii in ids.indices) {
            m_bonusSummaries[ii + 1] = res.getString(ids[ii])
        }
    }

    private fun postNAHint(msgID: Int, keyID: Int) {
        if (!sShown.contains(keyID)) {
            sShown.add(keyID)
            if (m_activity is XWActivity) {
                val activity = m_activity
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

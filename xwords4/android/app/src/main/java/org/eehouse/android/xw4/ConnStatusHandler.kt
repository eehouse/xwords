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
import android.graphics.PorterDuff
import android.graphics.Rect
import android.os.Handler
import android.provider.Settings
import android.text.format.DateUtils
import androidx.core.content.ContextCompat

import java.io.Serializable
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

import org.eehouse.android.xw4.jni.CommsAddrRec
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

object ConnStatusHandler {
    private val TAG: String = ConnStatusHandler::class.java.simpleName
    private val RECS_KEY = TAG + "/recs"
    private val STALL_STATS_KEY = TAG + "/stall_stats"
    private const val ORANGE = -0x5b00

    private const val SUCCESS_IN = 0
    private const val SUCCESS_OUT = 1
    private const val SHOW_SUCCESS_INTERVAL = 1000

    private var s_rect: Rect? = null
    private var s_downOnMe = false
    private var s_cbacks: ConnStatusCBacks? = null
    private var s_fillPaint = Paint(Paint.ANTI_ALIAS_FLAG)

    init {
        s_fillPaint.textAlign = Paint.Align.CENTER
    }

    private val s_showSuccesses = booleanArrayOf(false, false)
    private var s_moveCount = 0
    private var s_quashed = false

    private var s_needsSave = false

    fun setRect(left: Int, top: Int, right: Int, bottom: Int) {
        s_rect = Rect(left, top, right, bottom)
        s_fillPaint.textSize = (s_rect!!.height() / 2).toFloat()
    }

    fun clearRect() {
        s_rect = null
    }

    fun setHandler(cbacks: ConnStatusCBacks?) {
        s_cbacks = cbacks
    }

    fun handleDown(xx: Int, yy: Int): Boolean {
        s_downOnMe = null != s_rect && s_rect!!.contains(xx, yy)
        return s_downOnMe
    }

    fun handleUp(xx: Int, yy: Int): Boolean {
        val result = s_downOnMe && s_rect!!.contains(xx, yy)
        if (result && null != s_cbacks) {
            s_cbacks!!.onStatusClicked()
        }
        s_downOnMe = false
        return result
    }

    fun handleMove(xx: Int, yy: Int): Boolean {
        return s_downOnMe && s_rect!!.contains(xx, yy)
    }


    private val sDisplayOrder = arrayOf(
        CommsConnType.COMMS_CONN_RELAY,
        CommsConnType.COMMS_CONN_MQTT,
        CommsConnType.COMMS_CONN_BT,
        CommsConnType.COMMS_CONN_IR,
        CommsConnType.COMMS_CONN_IP_DIRECT,
        CommsConnType.COMMS_CONN_SMS,
        CommsConnType.COMMS_CONN_P2P,
        CommsConnType.COMMS_CONN_NFC,
    )

    fun getStatusText(
        context: Context, gamePtr: XwJNI.GamePtr,
        gameID: Int, connTypes: CommsConnTypeSet?,
        addr: CommsAddrRec?
    ): String? {
        val msg: String?
        if (null == connTypes || 0 == connTypes.size) {
            msg = null
        } else {
            val sb = StringBuffer()
            var tmp: String
            synchronized(ConnStatusHandler::class.java) {
                sb.append(
                    LocUtils.getString(
                        context, R.string.connstat_net_fmt,
                        connTypes.toString(context, true), gameID
                    )
                )
                for (typ in sDisplayOrder) {
                    if (!connTypes.contains(typ)) {
                        continue
                    }
                    var record = recordFor(context, typ, false)

                    // Don't show e.g. NFC unless it's been used
                    if (!typ.isSelectable) {
                        if (!record.haveFailure() && !record.haveSuccess()) {
                            continue
                        }
                    }

                    sb.append(String.format("\n\n*** %s ", typ.longName(context)))
                    val did = addDebugInfo(context, gamePtr, addr, typ)
                    if (null != did) {
                        sb.append(did).append(" ")
                    }
                    sb.append("***\n")

                    // For sends we list failures too.
                    tmp = LocUtils.getString(
                        context,
                        if (record.successNewer) R.string.connstat_succ else R.string.connstat_unsucc
                    )

                    var timeStr = record.newerStr(context)
                    if (null != timeStr) {
                        sb.append(
                            LocUtils.getString(
                                context, R.string.connstat_lastsend_fmt,
                                tmp, timeStr
                            )
                        )
                            .append("\n")
                    }

                    var fmtId = 0
                    if (record.successNewer) {
                        if (record.haveFailure()) {
                            fmtId = R.string.connstat_lastother_succ_fmt
                        }
                    } else {
                        if (record.haveSuccess()) {
                            fmtId = R.string.connstat_lastother_unsucc_fmt
                        }
                    }
                    timeStr = record.olderStr(context)
                    if (0 != fmtId && null != timeStr) {
                        sb.append(LocUtils.getString(context, fmtId, timeStr))
                            .append("\n")
                    }
                    sb.append("\n")

                    record = recordFor(context, typ, true)
                    if (record.haveSuccess()) {
                        sb.append(
                            LocUtils.getString(
                                context,R.string.connstat_lastreceipt_fmt,
                                record.newerStr(context))
                        )
                    } else {
                        sb.append(LocUtils.getString(context, R.string.connstat_noreceipt))
                    }

                    if (BuildConfig.DEBUG) {
                        val stallStats = stallStatsFor(context, typ)
                        if (null != stallStats) {
                            sb.append(stallStats)
                        }
                    }
                }
            }

            if (BuildConfig.DEBUG) {
                sb.append("\n").append(XwJNI.comms_getStats(gamePtr))
            }
            msg = sb.toString()
        }
        return msg
    }

    private fun stallStatsFor(context: Context, typ: CommsConnType): String? {
        // long[] nums = StallStats.get( context, typ ).averages();
        // return "Average for last 10 Intents (spanning %s): %dms";;
        val stats = StallStats.get(context, typ, false)
        return stats?.toString(context)
    }

    private fun invalidateParent() {
        if (null != s_cbacks) {
            s_cbacks!!.invalidateParent()
        }
    }

    fun updateMoveCount(
        context: Context, newCount: Int,
        quashed: Boolean
    ) {
        if (XWPrefs.moveCountEnabled(context)) {
            s_moveCount = newCount
            s_quashed = quashed
            invalidateParent()
        }
    }

    fun updateStatus(
        context: Context, cbacks: ConnStatusCBacks?,
        connType: CommsConnType, success: Boolean
    ) {
        updateStatusImpl(context, cbacks, connType, success, true)
        updateStatusImpl(context, cbacks, connType, success, false)
    }

    fun updateStatusIn(
        context: Context, cbacks: ConnStatusCBacks?,
        connType: CommsConnType, success: Boolean
    ) {
        updateStatusImpl(context, cbacks, connType, success, true)
    }

    fun updateStatusIn(
        context: Context, connType: CommsConnType,
        success: Boolean
    ) {
        updateStatusImpl(context, null, connType, success, true)
    }

    fun updateStatusOut(context: Context, connType: CommsConnType, success: Boolean) {
        updateStatusImpl(context, null, connType, success, false)
    }

    private fun updateStatusImpl(
        context: Context, cbacks: ConnStatusCBacks?,
        connType: CommsConnType, success: Boolean,
        isIn: Boolean
    ) {
        val cbacks =
            if (null == cbacks) s_cbacks
            else cbacks

        synchronized(ConnStatusHandler::class.java) {
            val record = recordFor(context, connType, isIn)
            record.update(success)
        }
        invalidateParent()
        saveState(context, cbacks)
        if (success) {
            showSuccess(cbacks, isIn)
        }
    }

    @JvmOverloads
    fun showSuccessIn(cbcks: ConnStatusCBacks? = s_cbacks) {
        showSuccess(cbcks, true)
    }

    @JvmOverloads
    fun showSuccessOut(cbcks: ConnStatusCBacks? = s_cbacks) {
        showSuccess(cbcks, false)
    }

    fun draw(
        context: Context, canvas: Canvas,
        connTypes: CommsConnTypeSet?, isSolo: Boolean
    ) {
        if (!isSolo && null != s_rect) {
            synchronized(ConnStatusHandler::class.java) {
                val connTypes = connTypes!!
                val scratchR = Rect(s_rect)
                val quarterHeight = scratchR.height() / 4

                val enabled = anyTypeEnabled(context, connTypes)

                // Do the background coloring and arrow. Top half first
                scratchR.bottom -= (2 * quarterHeight)
                fillHalf(context, canvas, scratchR, connTypes, enabled, false)
                scratchR.bottom -= quarterHeight
                drawArrow(context, canvas, scratchR, false)

                // bottom half and arrow
                scratchR.top = s_rect!!.top + (2 * quarterHeight)
                scratchR.bottom = s_rect!!.bottom
                fillHalf(context, canvas, scratchR, connTypes, enabled, true)
                scratchR.top += quarterHeight
                drawArrow(context, canvas, scratchR, true)

                // Center the icon in the remaining (vertically middle) rect
                scratchR.top = s_rect!!.top + quarterHeight
                scratchR.bottom = s_rect!!.bottom - quarterHeight
                val minDim = min(scratchR.width().toDouble(), scratchR.height().toDouble())
                    .toInt()
                val dx = (scratchR.width() - minDim) / 2
                val dy = (scratchR.height() - minDim) / 2
                scratchR.inset(dx, dy)
                Assert.assertTrueNR(1 >= abs((scratchR.width()
                                              - scratchR.height()).toDouble()))
                drawIn(context, canvas, R.drawable.ic_multigame, scratchR)
                if (XWPrefs.moveCountEnabled(context)) {
                    var str = ""
                    if (0 < s_moveCount) {
                        str += String.format("%d", s_moveCount)
                    }
                    if (s_quashed) {
                        str += "q"
                    }
                    if (0 < str.length) {
                        s_fillPaint.color = Color.BLACK
                        canvas.drawText(
                            str, (s_rect!!.left + (s_rect!!.width() / 2)).toFloat(),
                            (s_rect!!.top + (s_rect!!.height() * 2 / 3)).toFloat(),
                            s_fillPaint
                        )
                    }
                }
            }
        }
    }

    private fun fillHalf(
        context: Context, canvas: Canvas, rect: Rect,
        connTypes: CommsConnTypeSet, enabled: Boolean,
        isIn: Boolean
    ) {
        val enabled = enabled && null != newestSuccess(context, connTypes, isIn)
        s_fillPaint.color = if (enabled) XWApp.GREEN else XWApp.RED
        canvas.drawRect(rect, s_fillPaint)
    }

    private fun drawArrow(
        context: Context, canvas: Canvas, rect: Rect,
        isIn: Boolean
    ) {
        val showSuccesses = s_showSuccesses[if (isIn) SUCCESS_IN else SUCCESS_OUT]
        val color = if (showSuccesses) ORANGE else Color.WHITE
        val arrowID = if (isIn) R.drawable.ic_in_arrow else R.drawable.ic_out_arrow
        drawIn(context, canvas, arrowID, rect, color)
    }

    // This gets rid of lint warning, but I don't like it as it
    // effects the whole method.
    // @SuppressWarnings("unchecked")
    private var s_records: HashMap<CommsConnType, Array<SuccessRecord>>? = null
    private fun getRecords(context: Context): HashMap<CommsConnType, Array<SuccessRecord>>? {
        synchronized(ConnStatusHandler::class.java) {
            if (s_records == null) {
                s_records = DBUtils.getSerializableFor(
                    context,
                    RECS_KEY
                ) as HashMap<CommsConnType, Array<SuccessRecord>>?
                if (null == s_records) {
                    s_records = HashMap()
                }
            }
        }
        return s_records
    }

    private fun saveState(
        context: Context,
        cbcks: ConnStatusCBacks?
    ) {
        if (null == cbcks) {
            doSave(context)
        } else {
            var savePending: Boolean
            synchronized(ConnStatusHandler::class.java) {
                savePending = s_needsSave
                if (!savePending) {
                    s_needsSave = true
                }
            }

            if (!savePending) {
                val handler = cbcks.getHandler()
                if (null != handler) {
                    val proc = Runnable { doSave(context) }
                    handler.postDelayed(proc, 5000)
                }
            }
        }
    }

    private fun showSuccess(cbcks: ConnStatusCBacks?, isIn: Boolean) {
        if (null != cbcks) {
            synchronized(ConnStatusHandler::class.java) {
                if (isIn && s_showSuccesses[SUCCESS_IN]) {
                    // do nothing
                } else if (!isIn && s_showSuccesses[SUCCESS_OUT]) {
                    // do nothing
                } else {
                    val handler = cbcks.getHandler()
                    if (null != handler) {
                        val index = if (isIn) SUCCESS_IN else SUCCESS_OUT
                        s_showSuccesses[index] = true

                        val proc = Runnable {
                            synchronized(ConnStatusHandler::class.java) {
                                s_showSuccesses[index] = false
                                invalidateParent()
                            }
                        }
                        handler.postDelayed(proc, SHOW_SUCCESS_INTERVAL.toLong())
                        invalidateParent()
                    }
                }
            }
        }
    }

    private fun drawIn(
        context: Context, canvas: Canvas,
        id: Int,
        rect: Rect, color: Int = Color.WHITE
    ) {
        var icon = ContextCompat.getDrawable(context, id)!!
        if (Color.WHITE != color) {
            icon = icon.mutate()
            icon.setColorFilter(color, PorterDuff.Mode.MULTIPLY)
        }
        Assert.assertTrueNR(icon.bounds.width() == icon.bounds.height())
        icon.bounds = rect
        icon.draw(canvas)
    }

    private fun newestSuccess(
        context: Context,
        connTypes: CommsConnTypeSet?,
        isIn: Boolean
    ): SuccessRecord? {
        var result: SuccessRecord? = null
        if (null != connTypes) {
            val iter: Iterator<CommsConnType> = connTypes.iterator()
            while (iter.hasNext()) {
                val connType = iter.next()
                val record = recordFor(context, connType, isIn)
                if (record.successNewer) {
                    if (null == result || result.lastSuccess < record.lastSuccess) {
                        result = record
                    }
                }
            }
        }
        return result
    }

    private fun recordFor(
        context: Context,
        connType: CommsConnType,
        isIn: Boolean
    ): SuccessRecord {
        var records = getRecords(context)!![connType]
        if (null == records) {
            records = arrayOf(
                SuccessRecord(),
                SuccessRecord(),
            )
            getRecords(context)!![connType] = records
        }
        return records[if (isIn) 0 else 1]
    }

    private fun doSave(context: Context) {
        synchronized(ConnStatusHandler::class.java) {
            DBUtils.setSerializableFor(context, RECS_KEY, getRecords(context))
            s_needsSave = false
        }
    }

    private fun anyTypeEnabled(context: Context, connTypes: CommsConnTypeSet): Boolean {
        var enabled = false
        val iter: Iterator<CommsConnType> = connTypes.iterator()
        while (!enabled && iter.hasNext()) {
            enabled = connTypeEnabled(context, iter.next())
        }
        return enabled
    }

    private fun connTypeEnabled(
        context: Context, connType: CommsConnType
    ): Boolean {
        val result = when (connType) {
            CommsConnType.COMMS_CONN_SMS -> (XWPrefs.getNBSEnabled(context)
                                                 && !getAirplaneModeOn(context))
            CommsConnType.COMMS_CONN_BT -> BTUtils.BTEnabled()
            CommsConnType.COMMS_CONN_RELAY -> false
            CommsConnType.COMMS_CONN_P2P -> WiDirService.connecting()
            CommsConnType.COMMS_CONN_NFC -> NFCUtils.nfcAvail(context)[1]
            CommsConnType.COMMS_CONN_MQTT -> (XWPrefs.getMQTTEnabled(context)
                                                  && NetStateCache.netAvail(context))

            else -> {
                Log.w(TAG, "connTypeEnabled: %s not handled", connType.toString())
                false
            }
        }
        return result
    }

    private fun getAirplaneModeOn(context: Context): Boolean {
        val result =
            0 != Settings.System.getInt(
                context.contentResolver,
                Settings.System.AIRPLANE_MODE_ON, 0
            )
        return result
    }

    private fun addDebugInfo(
        context: Context, gamePtr: XwJNI.GamePtr,
        addr: CommsAddrRec?, typ: CommsConnType
    ): String? {
        var result: String? = null
        if (BuildConfig.DEBUG) {
            when (typ) {
                CommsConnType.COMMS_CONN_RELAY -> Assert.failDbg()
                CommsConnType.COMMS_CONN_MQTT -> if (null != addr) {
                    result = String.format("DevID: %s", addr.mqtt_devID)
                }

                CommsConnType.COMMS_CONN_P2P -> result = WiDirService.formatNetStateInfo()
                CommsConnType.COMMS_CONN_BT -> if (null != addr) {
                    result = addr.bt_hostName
                }

                CommsConnType.COMMS_CONN_SMS -> if (null != addr) {
                    result = addr.sms_phone
                }

                else -> {}
            }
        }

        if (null != result) {
            result = "($result)"
        }

        return result
    }

    interface ConnStatusCBacks {
        fun invalidateParent()
        fun onStatusClicked()
        fun getHandler(): Handler?
    }

    private class SuccessRecord : Serializable {
        var lastSuccess: Long = 0
        var lastFailure: Long = 0
        var successNewer: Boolean = false

        fun haveFailure(): Boolean {
            return lastFailure > 0
        }

        fun haveSuccess(): Boolean {
            return lastSuccess > 0
        }

        fun newerStr(context: Context): String? {
            return format(context, if (successNewer) lastSuccess else lastFailure)
        }

        fun olderStr(context: Context): String? {
            return format(context, if (successNewer) lastFailure else lastSuccess)
        }

        fun update(success: Boolean) {
            val now = System.currentTimeMillis()
            if (success) {
                lastSuccess = now
            } else {
                lastFailure = now
            }
            successNewer = success
        }

        companion object {
            private fun format(context: Context, millis: Long): String? {
                var result: String? = null
                if (millis > 0) {
                    val seq =
                        DateUtils.getRelativeDateTimeString(
                            context, millis,
                            DateUtils.SECOND_IN_MILLIS,
                            DateUtils.WEEK_IN_MILLIS,
                            0
                        )
                    result = seq.toString()
                }
                return result
            }
        }
    }

    private class StallStats : Serializable {
        var mData: LongArray = LongArray(MAX_STALL_DATA_LEN)
        var mStamps: LongArray = LongArray(MAX_STALL_DATA_LEN)
        var mUsed: Int = 0 // <= MAX_STALL_DATA_LEN

        @Synchronized
        fun append(context: Context, ageMS: Long) {
            if (MAX_STALL_DATA_LEN == mUsed) {
                --mUsed
                System.arraycopy(mData, 1, mData, 0, mUsed)
                System.arraycopy(mStamps, 1, mStamps, 0, mUsed)
            }
            mData[mUsed] = ageMS
            mStamps[mUsed] = System.currentTimeMillis()
            ++mUsed

            save(context)
        }

        @Synchronized
        fun toString(context: Context): String {
            val sb = StringBuffer()
                .append("\n\nService delay stats:\n")
            if (mUsed > 0) {
                var dataSum10: Long = 0
                var dataSum100: Long = 0
                val last10Indx = max(0.0, (mUsed - 10).toDouble()).toInt()
                for (ii in 0 until mUsed) {
                    val datum = mData[ii]
                    dataSum100 += datum
                    if (ii >= last10Indx) {
                        dataSum10 += datum
                    }
                }

                val firstStamp100 = mStamps[0]
                val firstStamp10 = mStamps[last10Indx]

                val num = min(10.0, mUsed.toDouble()).toInt()
                append(context, sb, num, dataSum10 / num, firstStamp10)
                append(context, sb, mUsed, dataSum100 / mUsed, firstStamp100)
            }
            return sb.toString()
        }

        private fun append(context: Context, sb: StringBuffer, len: Int, avg: Long, stamp: Long) {
            sb.append(
                String.format(
                    "For last %d: %dms avg. (oldest: %s)\n", len, avg,
                    DateUtils.getRelativeDateTimeString(
                        context, stamp,
                        DateUtils.SECOND_IN_MILLIS,
                        DateUtils.HOUR_IN_MILLIS,
                        0
                    )
                )
            )
        }

        companion object {
            private const val MAX_STALL_DATA_LEN = 100
            private var sStallStatsMap: HashMap<CommsConnType?, StallStats>? = null

            fun get(context: Context, typ: CommsConnType?, makeNew: Boolean): StallStats? {
                if (null == sStallStatsMap) {
                    sStallStatsMap = DBUtils.getSerializableFor(
                        context,
                        STALL_STATS_KEY
                    ) as HashMap<CommsConnType?, StallStats>?
                    if (null == sStallStatsMap) {
                        sStallStatsMap = HashMap()
                    }
                }

                var result = sStallStatsMap!![typ]
                if (result == null && makeNew) {
                    result = StallStats()
                    sStallStatsMap!![typ] = result
                }
                return result
            }

            private fun save(context: Context) {
                DBUtils.setSerializableFor(context, STALL_STATS_KEY, sStallStatsMap)
            }
        }
    }
}

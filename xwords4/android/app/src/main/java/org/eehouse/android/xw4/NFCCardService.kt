/*
 * Copyright 2019 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.nfc.cardemulation.HostApduService
import android.os.Bundle
import org.eehouse.android.xw4.DbgUtils.hexDump
import org.eehouse.android.xw4.NFCUtils.HEX_STR
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.IOException

class NFCCardService : HostApduService() {
    private var mMyDevID = 0

    // Remove this once we don't need logging to confirm stuff's loading
    override fun onCreate() {
        super.onCreate()
        mMyDevID = NFCUtils.getNFCDevID(this)
        Log.d(TAG, "onCreate() got mydevid %d", mMyDevID)
    }

    private var mGameID = 0
    override fun processCommandApdu(apdu: ByteArray?, extras: Bundle?): ByteArray {
        // Log.d( TAG, "processCommandApdu(%s)", DbgUtils.hexDump(apdu ) );
        var resStr = HEX_STR.STATUS_FAILED
        var isAidCase = false
        apdu?.let { apdu ->
            if (HEX_STR.CMD_MSG_PART.matchesFrom(apdu)) {
                resStr = HEX_STR.STATUS_SUCCESS
                val all = NFCUtils.reassemble(this, apdu, HEX_STR.CMD_MSG_PART)
                if (null != all) {
                    NFCUtils.addToMsgThread(this, all)
                }
            } else {
                Log.d(TAG, "processCommandApdu(): aid case?")
                if (!HEX_STR.DEFAULT_CLA.matchesFrom(apdu)) {
                    resStr = HEX_STR.CLA_NOT_SUPPORTED
                } else if (!HEX_STR.SELECT_INS.matchesFrom(apdu, 1)) {
                    resStr = HEX_STR.INS_NOT_SUPPORTED
                } else if (LEN_OFFSET >= apdu.size) {
                    Log.d(TAG, "processCommandApdu(): apdu too short")
                    // Not long enough for length byte
                } else {
                    try {
                        val bais = ByteArrayInputStream(
                            apdu, LEN_OFFSET,
                            apdu.size - LEN_OFFSET
                        )
                        val aidLen = bais.read().toByte()
                        Log.d(TAG, "aidLen=%d", aidLen)
                        if (bais.available() >= aidLen + 1) {
                            val aidPart = ByteArray(aidLen.toInt())
                            bais.read(aidPart)
                            val aidStr = Utils.ba2HexStr(aidPart)
                            if (BuildConfig.NFC_AID.equals(aidStr)) {
                                val minVersion = bais.read().toByte()
                                val maxVersion = bais.read().toByte()
                                if (minVersion == NFCUtils.VERSION_1) {
                                    val devID = NFCUtils.numFrom(bais)
                                    Log.d(
                                        TAG, "processCommandApdu(): read "
                                                + "remote devID: %d", devID
                                    )
                                    mGameID = NFCUtils.numFrom(bais)
                                    Log.d(TAG, "read gameID: %d", mGameID)
                                    if (0 < bais.available()) {
                                        Log.d(
                                            TAG, "processCommandApdu(): "
                                                    + "leaving anything behind?"
                                        )
                                    }
                                    resStr = HEX_STR.STATUS_SUCCESS
                                    isAidCase = true
                                } else {
                                    Log.e(
                                        TAG, "unexpected version %d; I'm too old?",
                                        minVersion
                                    )
                                }
                            } else {
                                Log.e(
                                    TAG, "aid mismatch: got %s but wanted %s",
                                    aidStr, BuildConfig.NFC_AID
                                )
                            }
                        }
                    } catch (ioe: IOException) {
                        Assert.failDbg()
                    }
                }
            }
        }
        val baos = ByteArrayOutputStream()
        try {
            baos.write(resStr.asBA())
            if (HEX_STR.STATUS_SUCCESS == resStr) {
                if (isAidCase) {
                    baos.write(NFCUtils.VERSION_1.toInt()) // min
                    baos.write(NFCUtils.numTo(mMyDevID))
                } else {
                    val token = NFCUtils.getMsgsFor(mGameID)
                    val tmp = NFCUtils.wrapMsg(token, Short.MAX_VALUE.toInt())
                    Assert.assertTrueNR(1 == tmp.size)
                    baos.write(tmp[0])
                }
            }
        } catch (ioe: IOException) {
            Assert.failDbg()
        }
        val result = baos.toByteArray()
        Log.d(
            TAG, "processCommandApdu(%s) => %s", hexDump(apdu),
            hexDump(result)
        )
        // this comes out of transceive() below!!!
        return result
    } // processCommandApdu

    override fun onDeactivated(reason: Int) {
        val str = when (reason) {
            DEACTIVATION_LINK_LOSS -> "DEACTIVATION_LINK_LOSS"
            DEACTIVATION_DESELECTED -> "DEACTIVATION_DESELECTED"
            else -> "<other>"
        }
        Log.d(TAG, "onDeactivated(reason=%s)", str)
    }

    companion object {
        private val TAG = NFCCardService::class.java.getSimpleName()
        private const val LEN_OFFSET = 4
    }
}

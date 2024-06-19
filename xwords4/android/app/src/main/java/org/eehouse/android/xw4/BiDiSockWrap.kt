/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2016 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import org.json.JSONObject
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.IOException
import java.io.UnsupportedEncodingException
import java.net.InetAddress
import java.net.Socket
import java.net.UnknownHostException
import java.util.concurrent.LinkedBlockingQueue
import kotlin.math.min

class BiDiSockWrap {
    interface Iface {
        fun gotPacket(wrap: BiDiSockWrap, bytes: ByteArray)
        fun onWriteSuccess(wrap: BiDiSockWrap)
        fun connectStateChanged(wrap: BiDiSockWrap, nowConnected: Boolean)
    }

    var socket: Socket? = null
        private set
    private var mIface: Iface
    private var mQueue: LinkedBlockingQueue<ByteArray>? = null
    private var mReadThread: Thread? = null
    private var mWriteThread: Thread? = null
    private var mRunThreads = false
    private var mActive = false
    private var mAddress: InetAddress? = null
    private var mPort = 0

    // For sockets that came from accept() on a ServerSocket
    constructor(socket: Socket, iface: Iface) {
        mIface = iface
        init(socket)
    }

    // For creating sockets that will connect to a remote ServerSocket. Must
    // call connect() afterwards.
    constructor(address: InetAddress?, port: Int, iface: Iface) {
        mIface = iface
        mAddress = address
        mPort = port
    }

    fun connect(): BiDiSockWrap {
        mActive = true
        Thread {
            var waitMillis: Long = 1000
            while (mActive) {
                try {
                    Thread.sleep(waitMillis)
                    Log.d(TAG, "trying to connect...")
                    val socket = Socket(mAddress, mPort)
                    Log.d(TAG, "connected!!!")
                    init(socket)
                    mIface.connectStateChanged(this@BiDiSockWrap, true)
                    break
                } catch (uhe: UnknownHostException) {
                    Log.ex(TAG, uhe)
                } catch (ioe: IOException) {
                    Log.ex(TAG, ioe)
                } catch (ie: InterruptedException) {
                    Log.ex(TAG, ie)
                }
                waitMillis = min((waitMillis * 2).toDouble(), (1000 * 60).toDouble())
                    .toLong()
            }
        }.start()

        return this
    }

    val isConnected: Boolean
        get() = null != socket

    private fun send(packet: String) {
        try {
            send(packet.toByteArray(charset("UTF-8")))
        } catch (uee: UnsupportedEncodingException) {
            Log.ex(TAG, uee)
        }
    }

    fun send(obj: JSONObject) {
        send(obj.toString())
    }

    fun send(packet: XWPacket) {
        send(packet.toString())
    }

    fun send(packet: ByteArray) {
        Assert.assertNotNull(packet)
        Assert.assertTrueNR(packet.size > 0)
        mQueue!!.add(packet)
    }

    private fun init(socket: Socket) {
        this.socket = socket
        mQueue = LinkedBlockingQueue()
        startThreads()
    }

    private fun closeSocket() {
        Log.d(TAG, "closeSocket()")
        mRunThreads = false
        mActive = false
        try {
            socket!!.close()
        } catch (ioe: IOException) {
            Log.ex(TAG, ioe)
        }
        mIface.connectStateChanged(this, false)
        mQueue!!.add(ByteArray(0))
    }

    private fun startThreads() {
        mRunThreads = true
        mWriteThread = Thread {
            Log.d(TAG, "write thread starting")
            try {
                val outStream = DataOutputStream(socket!!.getOutputStream())
                while (mRunThreads) {
                    val packet = mQueue!!.take()
                    Log.d(
                        TAG,
                        "write thread got packet of len %d",
                        packet.size
                    )
                    Assert.assertNotNull(packet)
                    if (0 == packet.size) {
                        closeSocket()
                        break
                    }

                    outStream.writeShort(packet.size)
                    outStream.write(packet, 0, packet.size)
                    mIface.onWriteSuccess(this@BiDiSockWrap)
                }
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
                closeSocket()
            } catch (ie: InterruptedException) {
                Assert.failDbg()
            }
            Log.d(TAG, "write thread exiting")
        }
        mWriteThread!!.start()

        mReadThread = Thread {
            Log.d(TAG, "read thread starting")
            try {
                val inStream = DataInputStream(socket!!.getInputStream())
                while (mRunThreads) {
                    val packet = ByteArray(
                        inStream.readShort().toInt()
                    )
                    inStream.readFully(packet)
                    mIface.gotPacket(this@BiDiSockWrap, packet)
                }
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
                closeSocket()
            }
            Log.d(TAG, "read thread exiting")
        }
        mReadThread!!.start()
    }

    companion object {
        private val TAG: String = BiDiSockWrap::class.java.simpleName
    }
}

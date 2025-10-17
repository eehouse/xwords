/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All
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
 *
 * NOTE: This file includes portions generated with the assistance of OpenAI’s
 * ChatGPT model (GPT-5-mini), and has been adapted for this project.
 */

package org.eehouse.android.xw4

import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.ParcelUuid
import android.text.TextUtils
import kotlinx.coroutines.delay
import java.lang.ref.WeakReference
import kotlin.collections.HashMap

import java.util.UUID

import org.eehouse.android.xw4.jni.Device

object BleNetwork {
    private const val TAG = "BleNetwork"
    private const val DEFAULT_MTU = 23
    // UUIDs for the custom BLE characteristic. For the service use the same
    // as old code for its rfcomm service record.
    private val WRITE_CHAR_UUID = UUID.fromString("9204047c-4cc4-4485-96cc-1407aa467875")

    private lateinit var mBluetoothAdapter: BluetoothAdapter
    private var mGattServer: BluetoothGattServer? = null
    private var mAdvertiser: BluetoothLeAdvertiser? = null
    private lateinit var mServiceUUID: UUID
    private var mContext: Context? = null
    private val sListeners: MutableSet<WeakReference<ScanListener>> = HashSet()

    // Keep track of active connections to optionally reuse
    private val activeGatts = HashMap<String, WeakReference<BleConnection>>()

    interface ScanListener {
        fun onDeviceScanned(dev: BluetoothDevice)
    }

    fun init(context: Context) {
        mContext = context
        mServiceUUID = Device.getUUID()
        Log.d(TAG, "UUID: $mServiceUUID")
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
        Assert.assertTrueNR(mBluetoothAdapter.isMultipleAdvertisementSupported)

        // Setup GATT server (Peripheral role)
        val mgr = context.getSystemService(Context. BLUETOOTH_SERVICE) as BluetoothManager
        mGattServer = mgr.openGattServer(context, gattServerCallback)

        val writeChar = BluetoothGattCharacteristic(
            WRITE_CHAR_UUID,
            BluetoothGattCharacteristic.PROPERTY_WRITE,
            BluetoothGattCharacteristic.PERMISSION_WRITE
        )
        BluetoothGattService(mServiceUUID,
                             BluetoothGattService.SERVICE_TYPE_PRIMARY).let {
            service ->
            service.addCharacteristic(writeChar)
            mGattServer?.addService(service)
        }

        // Start scanning to discover peers
        // Log.d(TAG, "starting scan")
        // mBluetoothAdapter.bluetoothLeScanner.startScan(scanCallback)
        // scan(context)
    }

    fun addScanListener(context: Context, listener: ScanListener) {
        synchronized(sListeners) {
            sListeners.add(WeakReference<ScanListener>(listener))
        }
        startScan(context)
    }

    fun removeScanListener(listener: ScanListener) {
        Log.d(TAG, "removeScanListener()")
        synchronized(sListeners) {
            val iter = sListeners.iterator()
            iter.forEach {
                if (it.get()?.equals(listener)?:false) {
                    iter.remove()
                    Log.d(TAG, "removeScanListener(): found it!!!")
                }
            }
        }
        if (sListeners.size == 0) {
            stopScan()
        }
    }

    private fun callListeners(dev: BluetoothDevice) {
        synchronized(sListeners) {
            sListeners.map {
                it.get()?.onDeviceScanned(dev)
            }
        }
    }

    fun startScan(context: Context = mContext!!) {
        Log.d(TAG, "startScan()")
        stopScan()
        if ( 0 < sListeners.size ) {
            val scanFilter = ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(mServiceUUID))
                .build()

            val scanSettings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build()

            Log.d(TAG, "calling startScan()")
            mBluetoothAdapter.bluetoothLeScanner
                .startScan(listOf(scanFilter), scanSettings, scanCallback)
        }
    }

    fun sendPacket(context: Context, name: String?, mac: String?, packet: ByteArray) {
        Log.d(TAG, "sendPacket($name, $mac, len=${packet.size})")
        getRemoteDevice(name, mac)?.let {
            getGattFor(context, it).enqueue(packet)
        }
    }

    private fun getGattFor(context: Context, device: BluetoothDevice): BleConnection {
        var gatt = activeGatts[device.address]?.get()
        if ( null == gatt ) {
            gatt = BleConnection(context, device)
            // gatt.requestMtu(512)
            // Log.d(TAG, "requesting mtu DONE")
            activeGatts[device.address] = WeakReference<BleConnection>(gatt)
        }
        return gatt
    }

    fun shutdown() {
        mAdvertiser?.stopAdvertising(advertiseCallback)
        stopScan()
        mGattServer?.close()
        activeGatts.values.map { ref ->
            ref.get()?.disconnect()
        }
        activeGatts.clear()
    }

    private fun stopScan() {
        mBluetoothAdapter?.bluetoothLeScanner?.stopScan(scanCallback)
    }

    private fun getRemoteDevice(btName: String?, btAddr: String?): BluetoothDevice? {
        var result: BluetoothDevice? = null
        if (!TextUtils.isEmpty(btAddr)) {
            result = mBluetoothAdapter!!.getRemoteDevice(btAddr)
        }
        if (null == result || TextUtils.isEmpty(result.name)) {
            result = null
            Log.d(TAG, "getRemoteDevice($btAddr); no name; trying again")
            Assert.assertTrueNR(!TextUtils.isEmpty(btName))
            for (dev in mBluetoothAdapter!!.bondedDevices) {
                if (dev.name == btName) {
                    result = dev
                    break
                }
            }
        }
        Log.d(TAG, "getRemoteDevice($btName, $btAddr) => $result")
        return result
    }

    private fun startAdvertising() {
        Log.d(TAG, "startAdvertising()")
        // Start advertising
        mAdvertiser = mBluetoothAdapter.bluetoothLeAdvertiser
        val settings = AdvertiseSettings.Builder()
        // .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_POWER)
            .setConnectable(true)
            // .setConnectable(false)
            .setTimeout(0)
            .build()
        val data = AdvertiseData.Builder()
            .setIncludeDeviceName(false)
            .addServiceUuid(ParcelUuid(mServiceUUID))
            .build()
        mAdvertiser?.startAdvertising(settings, data, advertiseCallback)
    }

    // GATT server callbacks (receiving messages)
    private val gattServerCallback = object : BluetoothGattServerCallback() {

        override fun onServiceAdded(status: Int, service: BluetoothGattService) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "service added; ready to advertise")
                startAdvertising()
                // scan(mContext!!)
            }
        }
        
        override fun onCharacteristicWriteRequest(
            device: BluetoothDevice,
            requestId: Int,
            characteristic: BluetoothGattCharacteristic,
            preparedWrite: Boolean,
            responseNeeded: Boolean,
            offset: Int,
            packet: ByteArray
        ) {
            if (characteristic.uuid == WRITE_CHAR_UUID) {
                Log.d(TAG, "callback(): got packet of len ${packet.size}")
                Device.parseBTPacket(device.name, device.address, packet)
                if (responseNeeded) {
                    mGattServer?.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null)
                }
            }
        }
    }

    // Advertiser callbacks
    private val advertiseCallback = object : AdvertiseCallback() {
        override fun onStartSuccess(settingsInEffect: AdvertiseSettings) {
            Log.d(TAG, "onStartSuccess(): BLE advertising started")
        }

        override fun onStartFailure(errorCode: Int) {
            Log.e(TAG, "BLE advertising failed: $errorCode")
        }
    }

    // Scanner callback (optional: can be used for caching devices or discovering)
    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            Log.d(TAG, "onScanResult(${result.device.address})")
            callListeners(result.device)
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "BLE scan failed: $errorCode")
        }
    }

    private class BleConnection( private val context: Context, val device: BluetoothDevice )
        : BluetoothGattCallback()
    {
        var mGatt: BluetoothGatt?
        var mMtu: Int = DEFAULT_MTU
        val mPackets = ArrayDeque<ByteArray>()
        var mWriting: Boolean = false

        init {
            mGatt = device.connectGatt(context, false, this)
        }

        fun disconnect() {
            mGatt?.disconnect()
            mGatt?.close()
            mGatt = null
        }

        fun enqueue(packet: ByteArray) {
            Log.d(TAG, "enqueue($packet)")
            if (mPackets.none{ it.contentEquals(packet) }) {
                mPackets.addLast(packet)
                writeAny()
            } else {
                Log.d(TAG, "duplicate packet dropped")
            }
        }

        private fun requestMtu() {
            Utils.launch {
                for (ii in (0..3)) {
                    if (mMtu != DEFAULT_MTU) {
                        break;
                    }
                    Log.d(TAG, "requesting mtu")
                    mGatt!!.requestMtu(512)
                    delay(500)
                }
            }
        }
        
        private fun writeAny() {
            Log.d(TAG, "writeAny(): have ${mPackets.size} packets")
            if (!mWriting && mPackets.isNotEmpty()) {
                val gatt = mGatt!!
                gatt.getService(mServiceUUID)?.let { service ->
                    val firstPacket = mPackets.removeFirst()
                    if ( firstPacket.size <= (mMtu - 3) ) {
                        mWriting = true
                        service.getCharacteristic(WRITE_CHAR_UUID).let { char ->
                            char.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                            char.value = firstPacket
                            gatt.writeCharacteristic(char)
                        }
                        Log.d(TAG, "writeAny(): wrote packet of len ${firstPacket.size}")
                    } else {
                        mPackets.addFirst(firstPacket)
                    }
                } ?: Log.d(TAG, "no service")
            }
        }

        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                requestMtu()
                gatt.discoverServices()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            writeAny()
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            mWriting = false
            writeAny()
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                var addr = device.address
                Log.d(TAG, "onMtuChanged(): MTU for $addr changing from $mMtu to $mtu")
                mMtu = mtu
                Device.onBLEMtuChanged(addr, mtu-3)
                writeAny()
            }
        }
    }
}

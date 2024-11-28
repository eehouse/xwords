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
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.text.TextUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch

import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.BufferedWriter
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStreamWriter
import java.io.UnsupportedEncodingException
import java.net.HttpURLConnection
import java.net.InetAddress
import java.net.MalformedURLException
import java.net.ProtocolException
import java.net.Socket
import java.net.URL
import java.net.URLEncoder
import java.net.UnknownHostException
import javax.net.SocketFactory
import kotlin.concurrent.thread

import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

object NetUtils {
    private val TAG: String = NetUtils::class.java.simpleName

    const val k_PARAMS: String = "params"

    fun makeProxySocket(
        context: Context,
        timeoutMillis: Int
    ): Socket? {
        var socket: Socket? = null
        try {
            val port = XWPrefs.getDefaultProxyPort(context)
            val host = XWPrefs.getHostName(context)

            val factory = SocketFactory.getDefault()
            val addr = InetAddress.getByName(host)
            socket = factory.createSocket(addr, port)
            socket.soTimeout = timeoutMillis
        } catch (uhe: UnknownHostException) {
            Log.ex(TAG, uhe)
        } catch (ioe: IOException) {
            Log.ex(TAG, ioe)
        }
        return socket
    }

    private fun urlForGameID(context: Context, gameID: Int): String {
        val host = XWPrefs.getPrefsString(context, R.string.key_mqtt_host)
        val myID = XwJNI.dvc_getMQTTDevID()
        // Use the route that doesn't require login
        val url = String.format(
            "https://%s/xw4/ui/gameinfo?gid16=%X&devid=%s",
            host, gameID, myID
        )
        return url
    }

    fun gameURLToClip(context: Context, gameID: Int) {
        val url = urlForGameID(context, gameID)
        Utils.stringToClip(context, url)
        Utils.showToast(context, R.string.relaypage_url_copied)
    }

    fun copyAndLaunchGamePage(context: Context, gameID: Int) {
        // Requires a login, so only of use to me right now....
        val url = urlForGameID(context, gameID)
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
        context.startActivity(intent)
    }

    private val FORCE_HOST: String? = null

    // "eehouse.org"
    fun forceHost(host: String?): String? {
        var host = host
        if (null != FORCE_HOST) {
            host = FORCE_HOST
        }
        return host
    }

    // Pick http or https. SSL is broken on KitKat, and it's well after that
    // that https starts being required. So use http on and before KitKat,
    // just to be safe.
    fun ensureProto(context: Context, url: String): String
    {
        val pref = LocUtils.getCheckPref(
            context, R.array.url_schemes,
            key = R.string.key_url_scheme,
            default = R.string.url_scheme_default
        )
        val dflt = LocUtils.getString(context, R.string.url_scheme_default)

        val useHTTPs =
            if (dflt == pref) {
                // On my emulator, https doesn't work for version 24 ("N") and
                // below. So we'll try defaulting to http for everything up to
                // and including 24.
                Build.VERSION.SDK_INT > Build.VERSION_CODES.N
            } else if (LocUtils.getString(
                           context, R.string.url_scheme_http) == pref) {
                false
            } else {
                Assert.assertTrueNR(
                    LocUtils.getString(context, R.string.url_scheme_https)
                        == pref
                )
                true
            }

        val result =
            if (useHTTPs) url.replaceFirst("^http:".toRegex(), "https:")
            else url.replaceFirst("^https:".toRegex(), "http:")
        if (url != result) {
            Log.d(TAG, "ensureProto(%s) => %s", url, result)
        }
        return result
    }

    fun ensureProto(context: Context, uri: Uri): Uri {
        val uriString = ensureProto(context, uri.toString())
        return Uri.parse(uriString)
    }

    fun launchWebBrowserWith(context: Context, uriResID: Int) {
        val uri = context.getString(uriResID)
        launchWebBrowserWith(context, uri)
    }

    fun launchWebBrowserWith(context: Context, uri: String?) {
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(uri))
        context.startActivity(intent)
    }

    fun sendViaWeb(
        context: Context, resultKey: Int,
        api: String?, jsonParams: String
    ) {
        GlobalScope.launch(Dispatchers.IO) {
            val conn = makeHttpMQTTConn(context, api)
            val directJson = true
            val result = runConn(conn, jsonParams, directJson)
            if (0 != resultKey) {
                XwJNI.dvc_onWebSendResult(resultKey, true, result)
            }
        }
    }

    fun makeHttpMQTTConn(
        context: Context,
        proc: String?
    ): HttpURLConnection? {
        val url = XWPrefs.getDefaultMQTTUrl(context)
        return makeHttpConn(context, url, proc)
    }

    fun makeHttpUpdateConn(
        context: Context,
        proc: String?
    ): HttpURLConnection? {
        val url = XWPrefs.getDefaultUpdateUrl(context)
        return makeHttpConn(context, url, proc)
    }

    private fun makeHttpConn(
        context: Context, path: String,
        proc: String?
    ): HttpURLConnection? {
        var result: HttpURLConnection? = null
        try {
            val url = String.format("%s/%s", ensureProto(context, path), proc)
            result = URL(url).openConnection() as HttpURLConnection
        } catch (mue: MalformedURLException) {
            Assert.assertNull(result)
            Log.ex(TAG, mue)
        } catch (ioe: IOException) {
            Assert.assertNull(result)
            Log.ex(TAG, ioe)
        }
        return result
    }

    internal fun runConn(conn: HttpURLConnection?, param: JSONArray): String?
    {
        return runConn(conn, param.toString(), false)
    }

    fun runConn(conn: HttpURLConnection?, param: JSONObject): String?
    {
        return runConn(conn, param.toString(), false)
    }

    fun runConn(
        conn: HttpURLConnection?, param: JSONObject,
        directJson: Boolean
    ): String? {
        return runConn(conn, param.toString(), directJson)
    }

    private fun runConn(
        conn: HttpURLConnection?, param: String,
        directJson: Boolean
    ): String? {
        var param: String? = param
        var result: String? = null
        if (!directJson) {
            val params: MutableMap<String, String?> = HashMap()
            params[k_PARAMS] = param
            param = getPostDataString(params)
        }

        if (null != conn && null != param) {
            try {
                conn.readTimeout = 15000
                conn.connectTimeout = 15000
                conn.requestMethod = "POST"
                if (directJson) {
                    conn.setRequestProperty("Content-Type", "application/json;charset=UTF-8")
                } else {
                    conn.setFixedLengthStreamingMode(param.length)
                }
                conn.doInput = true
                conn.doOutput = true

                val os = conn.outputStream
                val writer = BufferedWriter(OutputStreamWriter(os, "UTF-8"))
                writer.write(param)
                writer.flush()
                writer.close()
                os.close()

                val responseCode = conn.responseCode
                if (HttpURLConnection.HTTP_OK == responseCode) {
                    val `is` = conn.inputStream
                    val bis = BufferedInputStream(`is`)

                    val bas = ByteArrayOutputStream()
                    val buffer = ByteArray(1024)
                    while (true) {
                        val nRead = bis.read(buffer)
                        if (0 > nRead) {
                            break
                        }
                        bas.write(buffer, 0, nRead)
                    }
                    result = String(bas.toByteArray())
                } else {
                    Log.w(
                        TAG, "runConn: responseCode: %d/%s for url: %s",
                        responseCode, conn.responseMessage,
                        conn.url
                    )
                    logErrorStream(conn.errorStream)
                }
            } catch (pe: ProtocolException) {
                Log.ex(TAG, pe)
            } catch (ioe: IOException) {
                Log.d(TAG, "runConn(%s) failed with IOException: %s",
                      conn, ioe.message)
            }
        } else {
            Log.e(TAG, "not running conn %s with params %s", conn, param)
        }

        return result
    }

    private fun logErrorStream(istream: InputStream) {
        try {
            val baos = ByteArrayOutputStream()
            val buffer = ByteArray(1024)
            while (true) {
                val length = istream.read(buffer)
                if (length == -1) {
                    break
                }
                baos.write(buffer, 0, length)
            }
            Log.e(TAG, baos.toString())
        } catch (ex: Exception) {
            Log.e(TAG, ex.message!!)
        }
    }

    // This handles multiple params but only every gets passed one!
    private fun getPostDataString(params: Map<String, String?>): String? {
        var result: String? = null
        try {
            val pairs = ArrayList<String?>()
            // StringBuilder sb = new StringBuilder();
            // String[] pair = { null, null };
            for ((key, value) in params) {
                pairs.add(
                    URLEncoder.encode(key, "UTF-8")
                            + "="
                            + URLEncoder.encode(value, "UTF-8")
                )
            }
            result = TextUtils.join("&", pairs)
        } catch (uee: UnsupportedEncodingException) {
            Log.ex(TAG, uee)
        }

        return result
    }

    private fun sumStrings(strs: Array<String>?): Int {
        var len = 0
        if (null != strs) {
            for (str in strs) {
                len += str.length
            }
        }
        return len
    }
}

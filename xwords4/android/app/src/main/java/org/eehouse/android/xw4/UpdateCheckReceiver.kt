/*
 * Copyright 2012 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.AsyncTask
import android.os.SystemClock
import android.text.TextUtils

import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import kotlin.math.abs

import org.eehouse.android.xw4.DictUtils.DictAndLoc
import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.DictUtils.ON_SERVER
import org.eehouse.android.xw4.DwnldDelegate.DownloadFinishedListener
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

class UpdateCheckReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent?.action != null && intent.action == Intent.ACTION_BOOT_COMPLETED) {
            restartTimer(context)
        } else {
            checkVersions(context, false)
            restartTimer(context)
        }
    }

    private class UpdateQueryTask(
        private val mContext: Context, private val m_params: JSONObject,
        private val m_fromUI: Boolean, private val m_pm: PackageManager,
        private val m_packageName: String,
        private val m_dals: Array<DictAndLoc>?
    ) : AsyncTask<Void?, Void?, String?>() {
        override fun doInBackground(vararg params: Void?): String? {
            val conn = NetUtils.makeHttpUpdateConn(mContext, "getUpdates")
            var json: String? = null
            if (null != conn) {
                json = NetUtils.runConn(conn, m_params)
            }
            return json
        }

        override fun onPostExecute(json: String?) {
            if (null != json) {
                if (LOG_QUERIES) {
                    Log.d(TAG, "onPostExecute(): received: %s", json)
                }
                makeNotificationsIf(json)
                XWPrefs.setHaveCheckedUpgrades(mContext, true)
            }
        }

        private fun makeNotificationsIf(jstr: String) {
            var gotOne = false
            try {
                // Log.d( TAG, "makeNotificationsIf(response=%s)", jstr );
                val jobj = JSONObject(jstr)
                // Add upgrade

                if (jobj.has(k_APP)) {
                    if (Perms23.permInManifest(
                            mContext,
                            Perms23.Perm
                                .REQUEST_INSTALL_PACKAGES
                        )
                    ) {
                        val app = jobj.getJSONObject(k_APP)
                        if (app.has(k_URL)) {
                            val ai =
                                m_pm.getApplicationInfo(m_packageName, 0)
                            val label = m_pm.getApplicationLabel(ai).toString()

                            // If there's a download dir AND an installer
                            // app, handle this ourselves.  Otherwise just
                            // launch the browser
                            val useBrowser: Boolean
                            val downloads = DictUtils.getDownloadDir(mContext)
                            if (null == downloads) {
                                useBrowser = true
                            } else {
                                val tmp = File(
                                    downloads,
                                    "xx" + XWConstants.APK_EXTN
                                )
                                useBrowser = !Utils.canInstall(mContext, tmp)
                            }

                            val urlParm = app.getString(k_URL)

                            // Debug builds check frequently on a timer and
                            // when it's something we don't want it's annoying
                            // to get a lot of offers. So track the URL used,
                            // and only offer once per URL unless the request
                            // was manual.
                            var skipIt = false
                            if (BuildConfig.NON_RELEASE && !m_fromUI) {
                                val prevURL = DBUtils.getStringFor(mContext, KEY_PREV_URL)
                                if (urlParm == prevURL) {
                                    skipIt = true
                                } else {
                                    DBUtils.setStringFor(mContext, KEY_PREV_URL, urlParm)
                                }
                            }
                            if (!skipIt) {
                                gotOne = true
                                val url = NetUtils.ensureProto(mContext, urlParm)
                                val intent = if (useBrowser) {
                                    Intent(
                                        Intent.ACTION_VIEW,
                                        Uri.parse(url)
                                    )
                                } else {
                                    DwnldDelegate.makeAppDownloadIntent(mContext, url)
                                }

                                // If I asked explicitly, let's download now.
                                if (m_fromUI && !useBrowser) {
                                    mContext.startActivity(intent)
                                } else {
                                    // title and/or body might be in the reply
                                    var title = app.optString(k_UPGRADE_TITLE, null)
                                    if (null == title) {
                                        title = LocUtils.getString(
                                            mContext,
                                            R.string.new_app_avail_fmt,
                                            label
                                        )
                                    }
                                    var body = app.optString(k_UPGRADE_BODY, null)
                                    if (null == body) {
                                        body = LocUtils.getString(mContext, R.string.new_app_avail)
                                    }
                                    Utils.postNotification(
                                        mContext, intent, title,
                                        body, title.hashCode()
                                    )
                                }
                            }
                        }
                    } else {
                        Log.d(TAG, "need to notify upgrade available")
                    }
                }

                // dictionaries upgrade
                if (jobj.has(k_DICTS)) {
                    val dicts = jobj.getJSONArray(k_DICTS)
                    for (ii in 0 until dicts.length()) {
                        val dict = dicts.getJSONObject(ii)
                        if (dict.has(k_INDEX)) {
                            val index = dict.getInt(k_INDEX)
                            val dal = m_dals!![index]
                            if (dict.has(k_URL)) {
                                val url = dict.getString(k_URL)
                                postDictNotification(
                                    mContext, url,
                                    dal!!.name, dal.loc, true
                                )
                                gotOne = true
                            }
                            if (dict.has(k_SERVED_FLAG)) {
                                val served = dict.getBoolean(k_SERVED_FLAG)
                                DBUtils.updateServed(mContext, dal!!, served)
                            }
                        }
                    }
                }
            } catch (jse: JSONException) {
                Log.ex(TAG, jse)
                Log.w(TAG, "sent: \"%s\"", m_params.toString())
                Log.w(TAG, "received: \"%s\"", jstr)
            } catch (nnfe: PackageManager.NameNotFoundException) {
                Log.ex(TAG, nnfe)
            }

            if (!gotOne && m_fromUI) {
                Utils.showToast(mContext, R.string.checkupdates_none_found)
            }
        }
    }

    companion object {
        private val TAG: String = UpdateCheckReceiver::class.java.simpleName
        private const val LOG_QUERIES = false

        const val NEW_DICT_URL: String = "NEW_DICT_URL"
        const val NEW_DICT_LOC: String = "NEW_DICT_LOC"
        const val NEW_DICT_NAME: String = "NEW_DICT_NAME"
        const val NEW_XLATION_CBK: String = "NEW_XLATION_CBK"

        private const val INTERVAL_ONEHOUR = (1000 * 60 * 60).toLong()
        private const val INTERVAL_ONEDAY = INTERVAL_ONEHOUR * 24
        private const val INTERVAL_NDAYS: Long = 1
        private const val KEY_PREV_URL = "PREV_URL"

        // constants that are also used in info.py
        private const val k_NAME = "name"
        private const val k_AVERS = "avers"
        private const val k_VARIANT = "variant"
        private const val k_GVERS = "gvers"
        private const val k_INSTALLER = "installer"
        private const val k_DEVOK = "devOK"
        private const val k_APP = "app"
        private const val k_DICTS = "dicts"
        private const val k_LANG = "lang"
        private const val k_LANGCODE = "lc"
        private const val k_MD5SUM = "md5sum"
        private const val k_FULLSUM = "fullsum"
        private const val k_INDEX = "index"
        private const val k_SERVED_FLAG = "served"
        private const val k_LEN = "len"
        private const val k_URL = "url"
        private const val k_MQTTDEVID = "devid"
        private const val k_DEBUG = "dbg"
        private const val k_XLATEINFO = "xlatinfo"
        private const val k_UPGRADE_TITLE = "title"
        private const val k_UPGRADE_BODY = "body"

        fun restartTimer(context: Context) {
            val am =
                context.getSystemService(Context.ALARM_SERVICE) as AlarmManager

            val intent = Intent(context, UpdateCheckReceiver::class.java)
            val pi = PendingIntent.getBroadcast(
                context, 0, intent,
                PendingIntent.FLAG_IMMUTABLE
            )
            am.cancel(pi)

            var interval_millis: Long
            if (BuildConfig.NON_RELEASE) {
                interval_millis = INTERVAL_ONEHOUR
            } else {
                interval_millis = INTERVAL_ONEDAY
                if (!devOK(context)) {
                    interval_millis *= INTERVAL_NDAYS
                }
            }

            interval_millis = (interval_millis / 2
                    + abs((Utils.nextRandomInt() % interval_millis).toDouble())).toLong()
            am.setInexactRepeating(
                AlarmManager.ELAPSED_REALTIME_WAKEUP,
                SystemClock.elapsedRealtime() + interval_millis,
                interval_millis, pi
            )
        }

        fun checkVersions(context: Context, fromUI: Boolean) {
            checkVersions(context, fromUI, false)
        }

        fun checkDictVersions(context: Context) {
            checkVersions(context, false, true)
        }

        private fun checkVersions(
            context: Context, fromUI: Boolean,
            dictsOnly: Boolean
        ) {
            val params = JSONObject()
            val pm = context.packageManager
            val packageName = BuildConfig.APPLICATION_ID
            var versionCode: Int
            try {
                versionCode = pm.getPackageInfo(packageName, 0).versionCode
            } catch (nnfe: PackageManager.NameNotFoundException) {
                Log.ex(TAG, nnfe)
                versionCode = 0
            }

            // App update
            if (BuildConfig.FOR_FDROID || dictsOnly) {
                // Do nothing; can't upgrade app
            } else {
                var installer = pm.getInstallerPackageName(packageName)
                if (null == installer) {
                    installer = "none"
                }

                try {
                    val appParams = JSONObject()

                    appParams.put(k_VARIANT, BuildConfig.VARIANT_CODE)
                    appParams.put(k_GVERS, BuildConfig.GIT_REV)
                    appParams.put(k_INSTALLER, installer)
                    if (devOK(context)) {
                        appParams.put(k_DEVOK, true)
                    }
                    appParams.put(k_DEBUG, BuildConfig.DEBUG)
                    params.put(k_APP, appParams)

                    val devID = XwJNI.dvc_getMQTTDevID()
                    params.put(k_MQTTDEVID, devID)
                } catch (jse: JSONException) {
                    Log.ex(TAG, jse)
                }
            }

            // Dict update
            val dals = getDownloadedDicts(context)
            if (null != dals) {
                val dictParams = JSONArray()
                for (ii in dals.indices) {
                    dictParams.put(makeDictParams(context, dals[ii], ii))
                }
                try {
                    params.put(k_DICTS, dictParams)
                } catch (jse: JSONException) {
                    Log.ex(TAG, jse)
                }
            }

            if (0 < params.length()) {
                try {
                    params.put(k_NAME, packageName)
                    params.put(k_AVERS, versionCode)
                    if (LOG_QUERIES) {
                        Log.d(TAG, "checkVersions(): sending: %s", params)
                    }
                    UpdateQueryTask(
                        context, params, fromUI, pm,
                        packageName, dals
                    ).execute()
                } catch (jse: JSONException) {
                    Log.ex(TAG, jse)
                }
            }
        }

        private fun getDownloadedDicts(context: Context): Array<DictAndLoc>? {
            val tmp = DictUtils.dictList(context).orEmpty().filter {
                it.loc in listOf(DictLoc.DOWNLOAD, DictLoc.EXTERNAL, DictLoc.INTERNAL)
            }

            val result: Array<DictAndLoc>? =
                if (tmp.isEmpty()) null
                else tmp.toTypedArray()
            return result
        }

        private fun makeDictParams(
            context: Context,
            dal: DictAndLoc?,
            index: Int
        ): JSONObject {
            val params = JSONObject()
            val isoCode = DictLangCache.getDictISOCode(context, dal!!)
            val langStr = DictLangCache.getLangNameForISOCode(context, isoCode!!)
            val sums = DictLangCache.getDictMD5Sums(context, dal.name)
            val served = DictLangCache.getOnServer(context, dal)
            Assert.assertTrueNR(null != sums[1])
            val len = DictLangCache.getFileSize(context, dal)
            try {
                params.put(k_NAME, dal.name)
                params.put(k_LANG, langStr)
                params.put(k_LANGCODE, isoCode)
                params.put(k_MD5SUM, sums[0])
                params.put(k_FULLSUM, sums[1])
                params.put(k_INDEX, index)
                params.put(k_LEN, len)
                if (served != ON_SERVER.UNKNOWN) {
                    params.put(k_SERVED_FLAG, served == ON_SERVER.YES)
                }
            } catch (jse: JSONException) {
                Log.ex(TAG, jse)
            }
            return params
        }

        private fun devOK(context: Context): Boolean {
            return XWPrefs.getPrefsBoolean(
                context, R.string.key_update_prerel,
                false
            )
        }

        private fun postDictNotification(
            context: Context, url: String?,
            name: String?, loc: DictLoc,
            isUpdate: Boolean
        ) {
            val intent = Intent(context, MainActivity::class.java) // PENDING TEST THIS!!!
            intent.putExtra(NEW_DICT_URL, url)
            intent.putExtra(NEW_DICT_NAME, name)
            intent.putExtra(NEW_DICT_LOC, loc.ordinal)

            val strID = if (isUpdate) R.string.new_dict_avail_fmt
            else R.string.dict_avail_fmt
            val body = LocUtils.getString(context, strID, name)

            Utils.postNotification(
                context, intent,
                R.string.new_dict_avail,
                body, url.hashCode()
            )
        }

        fun downloadPerNotification(context: Context, intent: Intent): Boolean {
            val dictUri = intent.getStringExtra(NEW_DICT_URL)
            val name = intent.getStringExtra(NEW_DICT_NAME)
            val handled = (!TextUtils.isEmpty(dictUri)
                    && !TextUtils.isEmpty(name))
            if (handled) {
                val uri = Uri.parse(dictUri)
                DwnldDelegate.downloadDictInBack(context, uri, name!!, null)
            }
            return handled
        }

        fun postedForDictDownload(
            context: Context, uri: Uri,
            dfl: DownloadFinishedListener?
        ): Boolean {
            val durl = uri.getQueryParameter("durl")
            val isDownloadURI = null != durl
            if (isDownloadURI) {
                val name = uri.getQueryParameter("name")
                Assert.assertTrueNR(null != name) // derive from uri?
                val dictUri = Uri.parse(durl)
                val segs = dictUri.pathSegments
                if ("byod" == segs[0]) {
                    DwnldDelegate.downloadDictInBack(context, dictUri, name!!, dfl)
                } else {
                    postDictNotification(
                        context, durl, name,
                        DictLoc.INTERNAL, false
                    )
                }
            }
            // Log.d( TAG, "postDictNotification(%s) => %b", uri, isDownloadURI );
            return isDownloadURI
        }
    }
}

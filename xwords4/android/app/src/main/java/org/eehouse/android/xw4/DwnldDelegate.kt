/*
 * Copyright 2009-2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Intent
import android.net.Uri
import android.os.AsyncTask
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import java.io.File
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.MalformedURLException
import java.net.URI
import java.net.URISyntaxException

import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.DictUtils.DownProgListener
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.Utils.ISOCode

class DwnldDelegate(delegator: Delegator) : ListDelegateBase(delegator, R.layout.import_dict) {
    private val m_activity: Activity
    private var m_views: ArrayList<LinearLayout>? = null
    private var m_dfts: ArrayList<DownloadFilesTask>? = null

    interface DownloadFinishedListener {
        fun downloadFinished(isoCode: ISOCode, name: String, success: Boolean)
    }

    interface OnGotLcDictListener {
        fun gotDictInfo(success: Boolean, isoCode: ISOCode, name: String?)
    }

    // Track callbacks for downloads.
    private class ListenerData(
        var m_uri: Uri?,
        var m_name: String,
        var m_lstnr: DownloadFinishedListener
    )

    init {
        m_activity = delegator.getActivity()!!
    }

    private inner class DownloadFilesTask(
        uri: Uri?,
        name: String?,
        item: LinearLayout?,
        isApp: Boolean
    ) : AsyncTask<Void?, Void?, Void?>(), DownProgListener {
        private var m_savedDict: String? = null
        private var m_uri: Uri? = null
        private val m_name: String?
        private var m_isApp = false
        private var m_appFile: File? = null
        private var m_totalRead = 0
        private val m_listItem: LinearLayout
        private val m_progressBar: ProgressBar

        init {
            m_uri = uri
            m_name = name
            m_isApp = isApp
            m_listItem = (item)!!
            m_progressBar = item.findViewById<View>(R.id.progress_bar) as ProgressBar

            if (isApp) {
                nukeOldApks()
            }
        }

        fun setLabel(text: String?): DownloadFilesTask {
            val tv = m_listItem.findViewById<View>(R.id.dwnld_message) as TextView
            tv.text = text
            return this
        }

        fun forApp(): Boolean {
            return m_isApp
        }

        // Nuke any .apk we downloaded more than 1 week ago
        private fun nukeOldApks() {
            val apksDir = File(m_activity!!.filesDir, APKS_DIR)
            if (apksDir.exists()) {
                val files = apksDir.listFiles()
                if (0 < files.size) {
                    // 1 week ago
                    val LAST_MOD_MIN =
                        System.currentTimeMillis() - (1000 * 60 * 60 * 24 * 7)
                    var nDeleted = 0
                    for (apk: File in files) {
                        if (apk.isFile) {
                            val lastMod = apk.lastModified()
                            if (lastMod < LAST_MOD_MIN) {
                                val gone = apk.delete()
                                Assert.assertTrueNR(gone)
                                if (gone) {
                                    ++nDeleted
                                }
                            }
                        }
                    }
                    if (BuildConfig.NON_RELEASE && 0 < nDeleted) {
                        val msg = getString(
                            R.string.old_apks_deleted_fmt,
                            nDeleted
                        )
                        Log.d(TAG, msg)
                        DbgUtils.showf(msg)
                    }
                }
            }
        }

        override fun doInBackground(vararg unused: Void?): Void?
        {
            m_savedDict = null
            m_appFile = null

            try {
                val uri = m_uri!!
                val jUri = URI(uri.scheme, uri.schemeSpecificPart, uri.fragment)
                val conn = jUri.toURL().openConnection()
                val fileLen = conn.contentLength
                post(Runnable { m_progressBar.max = fileLen })
                val istream = conn.getInputStream()
                val name = m_name ?: basename(uri.path)
                if (m_isApp) {
                    Assert.assertTrueNR(null == m_name)
                    m_appFile = saveToPrivate(istream, name, this)
                } else {
                    m_savedDict = saveDict(istream, name, this)
                    // force a check so BYOD lists will show as custom
                    UpdateCheckReceiver.checkDictVersions(m_activity)
                }
                istream.close()
            } catch (use: URISyntaxException) {
                Log.ex(TAG, use)
            } catch (mue: MalformedURLException) {
                Log.ex(TAG, mue)
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
            }
            return null
        }

        override fun onCancelled() {
            callListener(m_uri, false)
        }

        override fun onPostExecute(unused: Void?) {
            if (null != m_savedDict) {
                val loc =
                    XWPrefs.getDefaultLoc((m_activity)!!)
                DictLangCache.inval(m_activity, m_savedDict, loc, true)
                callListener(m_uri, true)
            } else if (null != m_appFile) {
                // launch the installer
                val intent = Utils.makeInstallIntent(
                    (m_activity)!!, m_appFile
                )
                startActivity(intent)
            } else {
                // we failed at something....
                callListener(m_uri, false)
            }

            if (1 >= m_views!!.size) {
                finish()
            } else {
                m_views!!.remove(m_listItem)
                m_dfts!!.remove(this)
                mkListAdapter()
            }
        }

        //////////////////////////////////////////////////////////////////////
        // interface DictUtils.DownProgListener
        //////////////////////////////////////////////////////////////////////
        override fun progressMade(nBytes: Int) {
            m_totalRead += nBytes
            post(object : Runnable {
                override fun run() {
                    m_progressBar.progress = m_totalRead
                }
            })
        }

        private fun saveToPrivate(
            istream: InputStream, name: String,
            dpl: DownProgListener
        ): File? {
            var appFile: File? = null
            var success = false
            val buf = ByteArray(1024 * 4)

            try {
                // directory first
                appFile = File(m_activity!!.filesDir, APKS_DIR)
                appFile.mkdirs()
                appFile = File(appFile, name)
                val fos = FileOutputStream(appFile)
                var cancelled = false
                while (true) {
                    cancelled = isCancelled()
                    if (cancelled) {
                        break
                    }
                    val nRead = istream.read(buf, 0, buf.size)
                    if (0 > nRead) {
                        break
                    }
                    fos.write(buf, 0, nRead)
                    dpl.progressMade(nRead)
                }
                fos.close()
                success = !cancelled
            } catch (fnf: FileNotFoundException) {
                Log.ex(TAG, fnf)
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
            }

            if (!success) {
                appFile!!.delete()
                appFile = null
            }
            return appFile
        }
    }


    private inner class ImportListAdapter() : XWListAdapter(m_views!!.size) {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            return m_views!![position]
        }
    }

    override fun init(savedInstanceState: Bundle?) {
        m_dfts = ArrayList()
        var dft: DownloadFilesTask? = null
        var uris: Array<Uri>? = null
        var item: LinearLayout ? = null

        val intent = intent
        val uri = intent.data // launched from Manifest case
        if (null == uri) {
            val appUrl = intent.getStringExtra(APK_EXTRA)
            var names: Array<String?>? = null
            val isApp = null != appUrl
            if (isApp) {
                uris = arrayOf(Uri.parse(appUrl))
            } else {
                val parcels = intent.getParcelableArrayExtra(DICTS_EXTRA)!!
                names = intent.getStringArrayExtra(NAMES_EXTRA)
                uris = parcels.map{it as Uri}.toTypedArray()
            }
            if (null != uris) {
                m_views = ArrayList()
                for (ii in uris.indices) {
                    item = inflate(R.layout.import_dict_item) as LinearLayout
                    val name = names?.get(ii)
                    m_dfts!!.add(DownloadFilesTask(uris[ii], name, item, isApp))
                    m_views!!.add((item)!!)
                }
            }
        } else if (((null != intent.type
                    && (intent.type == "application/x-xwordsdict"))
                    || uri.toString().endsWith(XWConstants.DICT_EXTN))
        ) {
            item = inflate(R.layout.import_dict_item) as LinearLayout
            dft = DownloadFilesTask(uri, null, item, false)
            uris = arrayOf(uri)
        }

        if (null != dft) {
            Assert.assertTrue(0 == m_dfts!!.size)
            m_dfts!!.add(dft)
            m_views = ArrayList(1)
            m_views!!.add((item)!!)
            dft = null
        }

        if (0 == m_dfts!!.size) {
            finish()
        } else if (!anyNeedsStorage()) {
            doWithPermissions(uris!!)
        } else {
            tryGetPerms(
                Perm.STORAGE, R.string.download_rationale,
                Action.STORAGE_CONFIRMED, uris as Any
            )
        }
    }

    private fun doWithPermissions(uris: Array<Uri>) {
        Assert.assertTrue(m_dfts!!.size == uris.size)
        mkListAdapter()

        for (ii in uris.indices) {
            var showName: String = basename(uris[ii].path)
            showName = DictUtils.removeDictExtn((showName))
            val msg =
                getString(R.string.downloading_dict_fmt, showName)

            m_dfts!![ii]
                .setLabel(msg)
                .execute()
        }
    } // doWithPermissions

    private fun anyNeedsStorage(): Boolean {
        var result = false
        val loc = XWPrefs.getDefaultLoc((m_activity)!!)

        for (task: DownloadFilesTask in m_dfts!!) {
            if (task.forApp()) {
                // Needn't do anything
            } else if (DictLoc.DOWNLOAD == loc) {
                result = true
                break
            }
        }
        return result
    }

    override fun handleBackPressed(): Boolean {
        // cancel any tasks that remain
        val iter: Iterator<DownloadFilesTask> = m_dfts!!.iterator()
        while (iter.hasNext()) {
            val dft = iter.next()
            dft.cancel(true)
        }
        return super.handleBackPressed()
    }

    override fun onPosButton(action: Action, vararg params: Any?): Boolean {
        var handled = true
        when (action) {
            Action.STORAGE_CONFIRMED -> doWithPermissions(params[0] as Array<Uri>)
            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    override fun onNegButton(action: Action,
                             vararg params: Any?): Boolean
    {
        var handled = true
        when (action) {
            Action.STORAGE_CONFIRMED -> finish()
            else -> handled = super.onPosButton(action, *params)
        }
        return handled
    }

    private fun mkListAdapter() {
        setListAdapter(ImportListAdapter())
    }

    private fun saveDict(
        inputStream: InputStream, name: String,
        dpl: DownProgListener
    ): String? {
        var name: String? = name
        val loc = XWPrefs.getDefaultLoc((m_activity)!!)
        if (!DictUtils.saveDict((m_activity), inputStream, name!!, loc, dpl)) {
            name = null
        }
        return name
    }

    private fun basename(path: String?): String {
        return File(path).name
    }

    private fun callListener(uri: Uri?, success: Boolean) {
        if (null != uri) {
            var ld: ListenerData?
            synchronized(s_listeners) {
                ld = s_listeners.remove(uri)
            }
            if (null != ld) {
                var name: String = ld!!.m_name
                val isoCode = isoCodeFromUri(uri)
                if (null == name) {
                    name = uri.toString()
                }
                ld!!.m_lstnr.downloadFinished(isoCode, name, success)
            }
        }
    }

    companion object {
        private val TAG: String = DwnldDelegate::class.java.simpleName
        private val APKS_DIR = "apks"

        // URIs coming in in intents
        private val APK_EXTRA = "APK"
        private val DICTS_EXTRA = "XWDS"
        private val NAMES_EXTRA = "NAMES"

        private val s_listeners: MutableMap<Uri?, ListenerData> = HashMap()

        private fun isoCodeFromUri(uri: Uri): ISOCode {
            val segs = uri.pathSegments
            val result = ISOCode(segs[segs.size - 2])
            return result
        }

        private fun rememberListener(
            uri: Uri?, name: String,
            lstnr: DownloadFinishedListener
        ) {
            val ld = ListenerData(uri, name, lstnr)
            synchronized(s_listeners) {
                s_listeners.put(uri, ld)
            }
        }

        fun downloadDictInBack(
            context: Context, isoCode: ISOCode?,
            dictName: String,
            lstnr: DownloadFinishedListener?
        ) {
            val uri = Utils.makeDictUriFromCode(context, isoCode, dictName)
            downloadDictInBack(context, uri, dictName, lstnr)
        }

        fun downloadDictInBack(
            context: Context, uri: Uri,
            dictName: String,
            lstnr: DownloadFinishedListener?
        ) {
            val uris = arrayOf(uri)
            val names = arrayOf(dictName)
            downloadDictsInBack(context, uris, names, lstnr)
        }

        fun downloadDictsInBack(
            context: Context, uris: Array<Uri>,
            names: Array<String>,
            lstnr: DownloadFinishedListener?
        ) {
            // Convert to use http if necessary
            val withProto = arrayOfNulls<Uri>(uris.size)
            for (ii in withProto.indices) {
                withProto[ii] = NetUtils.ensureProto(context, uris[ii])
            }

            if (null != lstnr) {
                for (ii in withProto.indices) {
                    rememberListener(withProto[ii], names[ii], lstnr)
                }
            }

            val intent = Intent(context, DwnldActivity::class.java)
            intent.putExtra(DICTS_EXTRA, withProto) // uris implement Parcelable
            intent.putExtra(NAMES_EXTRA, names)
            context.startActivity(intent)
        }

        fun makeAppDownloadIntent(context: Context, url: String): Intent {
            val intent = Intent(context, DwnldActivity::class.java)
            intent.putExtra(APK_EXTRA, url)
            return intent
        }
    }
}

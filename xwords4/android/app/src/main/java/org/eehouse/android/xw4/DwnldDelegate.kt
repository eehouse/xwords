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
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.net.MalformedURLException
import java.net.URI
import java.net.URISyntaxException

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.withContext

import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.DictUtils.DownProgListener
import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.Device

class DwnldDelegate(delegator: Delegator)
    : ListDelegateBase(delegator, R.layout.import_dict)
{
    private val mActivity: Activity
    private val mViews = ArrayList<LinearLayout>()
    private val mDfts = ArrayList<DownloadFilesTask>()

    interface DownloadFinishedListener {
        fun downloadFinished(isoCode: ISOCode, name: String, success: Boolean)
    }

    // Track callbacks for downloads.
    private class ListenerData(
        var m_uri: Uri?,
        var m_name: String,
        var m_lstnr: DownloadFinishedListener
    )

    init {
        mActivity = delegator.getActivity()!!
    }

    private inner class DownloadFilesTask(
        uri: Uri?,
        name: String?,
        item: LinearLayout?
    ) : DownProgListener {
        private var m_savedDict: String? = null
        private var m_uri: Uri? = null
        private val m_name: String?
        private var m_totalRead = 0
        private val m_listItem: LinearLayout
        private val m_progressBar: ProgressBar
        private var mJob: Job? = null
        private var mCancelled = false

        init {
            m_uri = uri
            m_name = name
            m_listItem = (item)!!
            m_progressBar = item.findViewById<View>(R.id.progress_bar) as ProgressBar
        }

        override fun isCancelled(): Boolean {
            return mCancelled
         }

        fun execute() {
            mJob = Utils.launch(Dispatchers.IO) {
                try {
                    doInBackground()
                    withContext(Dispatchers.Main) {
                        onPostExecute()
                    }
                } catch (e: CancellationException) {
                    mCancelled = true
                }
            }
        }

        fun setLabel(text: String?): DownloadFilesTask {
            val tv = m_listItem.findViewById<View>(R.id.dwnld_message) as TextView
            tv.text = text
            return this
        }

        private fun doInBackground() {
            m_savedDict = null

            try {
                val uri = m_uri!!
                val jUri = URI(uri.scheme, uri.schemeSpecificPart, uri.fragment)
                val conn = jUri.toURL().openConnection()
                val fileLen = conn.contentLength
                post { m_progressBar.max = fileLen }
                val istream = conn.getInputStream()
                val name = m_name ?: basename(uri.path!!)
                m_savedDict = saveDict(istream, name, this)
                // force a check so BYOD lists will show as custom
                UpdateCheckReceiver.checkDictVersions(mActivity)
                istream.close()
            } catch (use: URISyntaxException) {
                Log.ex(TAG, use)
            } catch (mue: MalformedURLException) {
                Log.ex(TAG, mue)
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
            }
        }

        private fun onPostExecute() {
            m_savedDict?.let { savedDict ->
                    XWPrefs.getDefaultLoc(mActivity).also { loc ->
                        DictLangCache.inval(mActivity, savedDict, loc, true)
                    }
                    true
                } ?: false
                .let { success ->
                    callListener(m_uri, success)
                }

            if (1 >= mViews.size) {
                finish()
            } else {
                mViews.remove(m_listItem)
                mDfts.remove(this)
                mkListAdapter()
            }
        }

        fun cancel() {
            mJob?.cancel()
        }

        //////////////////////////////////////////////////////////////////////
        // interface DictUtils.DownProgListener
        //////////////////////////////////////////////////////////////////////
        override fun progressMade(nBytes: Int) {
            m_totalRead += nBytes
            post { m_progressBar.progress = m_totalRead }
        }
    }

    private inner class ImportListAdapter() : XWListAdapter(mViews.size) {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            return mViews[position]
        }
    }

    override fun init(savedInstanceState: Bundle?) {
        mDfts.clear()
        var dft: DownloadFilesTask? = null
        var uris: Array<Uri>? = null
        var item: LinearLayout ? = null

        val intent = intent
        val uri = intent.data // launched from Manifest case
        if (null == uri) {
            val parcels = intent.getParcelableArrayExtra(DICTS_EXTRA)!!
            val names = intent.getStringArrayExtra(NAMES_EXTRA)
            uris = parcels.map{it as Uri}.toTypedArray()
            mViews.clear()
            for (ii in uris.indices) {
                item = inflate(R.layout.import_dict_item) as LinearLayout
                val name = names?.get(ii)
                mDfts.add(DownloadFilesTask(uris[ii], name, item))
                mViews.add(item!!)
            }
        } else if (((null != intent.type
                    && (intent.type == "application/x-xwordsdict"))
                    || uri.toString().endsWith(XWConstants.DICT_EXTN))
        ) {
            item = inflate(R.layout.import_dict_item) as LinearLayout
            dft = DownloadFilesTask(uri, null, item)
            uris = arrayOf(uri)
        }

        if (null != dft) {
            Assert.assertTrue(0 == mDfts.size)
            mDfts.add(dft)
            mViews.clear()
            mViews.add(item!!)
            dft = null
        }

        if (0 == mDfts.size) {
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
        Assert.assertTrue(mDfts.size == uris.size)
        mkListAdapter()

        for (ii in uris.indices) {
            var showName: String = basename(uris[ii].path!!)
            showName = DictUtils.removeDictExtn(showName)
            val msg =
                getString(R.string.downloading_dict_fmt, showName)

            mDfts[ii]
                .setLabel(msg)
                .execute()
        }
    } // doWithPermissions

    private fun anyNeedsStorage(): Boolean {
        val loc = XWPrefs.getDefaultLoc(mActivity)
        val result = DictLoc.DOWNLOAD == loc
        return result
    }

    override fun handleBackPressed(): Boolean {
        // cancel any tasks that remain
        mDfts.map{ it.cancel() }
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
        val result =
            XWPrefs.getDefaultLoc(mActivity).let { loc ->
                if (DictUtils.saveDict(mActivity, inputStream, name!!, loc, dpl)) name
                else null
            }
        return result
    }

    private fun basename(path: String): String {
        return File(path).name
    }

    private fun callListener(uri: Uri?, success: Boolean) {
        // if (null != uri) {
        uri?.let { uri ->
            synchronized(s_listeners) {
                s_listeners.remove(uri)
            }?.let {
                val isoCode = isoCodeFromUri(uri)
                it.m_lstnr.downloadFinished(isoCode, it.m_name, success)
            }
        }
    }

    companion object {
        private val TAG: String = DwnldDelegate::class.java.simpleName
        private val APKS_DIR = "apks"

        // URIs coming in in intents
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
            ListenerData(uri, name, lstnr).also {
                synchronized(s_listeners) {
                    s_listeners.put(uri, it)
                }
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
    }
}

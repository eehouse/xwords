/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.os.Handler
import android.widget.ArrayAdapter

import java.util.Arrays

import org.eehouse.android.xw4.DictUtils.DictAndLoc
import org.eehouse.android.xw4.DictUtils.DictLoc
import org.eehouse.android.xw4.DictUtils.ON_SERVER
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.Utils.getMD5SumFor
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.DictInfo
import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.loc.LocUtils

object DictLangCache {
    private val TAG: String = DictLangCache::class.java.simpleName

    private var s_adaptedLang: ISOCode? = null
    private var s_langsAdapter: LangsArrayAdapter? = null
    private var s_dictsAdapter: ArrayAdapter<String>? = null
    private var s_last: String? = null
    private var s_handler: Handler? = null

    private val KeepLast = Comparator<String?> { str1, str2 ->
        if (s_last == str1) {
            1
        } else if (s_last == str2) {
            -1
        } else {
            str1!!.compareTo(str2!!, ignoreCase = true)
        }
    }

    fun annotatedDictName(context: Context, dal: DictAndLoc): String? {
        var result: String? = null
        val info = getInfo(context, dal)
        if (null != info) {
            val wordCount = info.wordCount

            val langName = getDictLangName(context, dal.name)
            result = LocUtils.getString(
                context, R.string.dict_desc_fmt,
                dal.name, langName,
                wordCount
            )
        }
        return result
    }

    // This populates the cache and will take significant time if it's mostly
    // empty and there are a lot of dicts.
    fun getLangCount(context: Context, isoCode: ISOCode): Int {
        var count = 0
        val dals = DictUtils.dictList(context)
        for (dal in dals!!) {
            if (isoCode.equals(getDictISOCode(context, dal))) {
                ++count
            }
        }
        return count
    }

    private fun getInfosHaveLang(context: Context, isoCode: ISOCode?): Array<DictInfo> {
        val al: MutableList<DictInfo> = ArrayList()
        val dals = DictUtils.dictList(context)
        for (dal in dals!!) {
            val info = getInfo(context, dal)
            if (null != info && isoCode!!.equals(info.isoCode())) {
                al.add(info)
            }
        }
        val result = al.toTypedArray<DictInfo>()
        return result
    }

    fun haveDict(context: Context, isoCode: ISOCode?, dictName: String): Boolean {
        var found = false
        val infos = getInfosHaveLang(context, isoCode)
        for (info in infos) {
            if (dictName == info.name) {
                found = true
                break
            }
        }
        return found
    }

    private fun getHaveLang(
        context: Context, isoCode: ISOCode?,
        comp: Comparator<DictInfo>?,
        withCounts: Boolean
    ): Array<String> {
        val infos = getInfosHaveLang(context, isoCode)

        if (null != comp) {
            Arrays.sort(infos, comp)
        }

        val al: MutableList<String> = ArrayList()
        val fmt = "%s (%d)" // must match stripCount below
        for (info in infos) {
            var name = info.name!!
            if (withCounts) {
                name = String.format(fmt, name, info.wordCount)
            }
            al.add(name)
        }
        val result = al.toTypedArray<String>()
        if (null == comp) {
            Arrays.sort(result)
        }
        return result
    }

    fun getHaveLang(context: Context, isoCode: ISOCode?): Array<String> {
        return getHaveLang(context, isoCode, null, false)
    }

    fun getDALsHaveLang(context: Context, isoCode: ISOCode): Array<DictAndLoc> {
        Assert.assertNotNull(isoCode)
        val al: MutableList<DictAndLoc> = ArrayList()
        val dals = DictUtils.dictList(context)

        for (dal in dals!!) {
            val info = getInfo(context, dal)
            if (null != info && isoCode.equals(info.isoCode())) {
                al.add(dal)
            }
        }
        val result = al.toTypedArray<DictAndLoc>()
        // Log.d( TAG, "getDALsHaveLang(%s) => %s", isoCode, result );
        return result
    }

    private val s_ByCount = Comparator<DictInfo> { di1, di2 -> di2.wordCount - di1.wordCount }

    fun getHaveLangByCount(context: Context, isoCode: ISOCode?): Array<String> {
        return getHaveLang(context, isoCode, s_ByCount, false)
    }

    fun getHaveLangCounts(context: Context, isoCode: ISOCode?): Array<String> {
        return getHaveLang(context, isoCode, null, true)
    }

    fun stripCount(nameWithCount: String): String {
        val indx = nameWithCount.lastIndexOf(" (")
        return nameWithCount.substring(0, indx)
    }

    fun getOnServer(context: Context, dal: DictAndLoc): ON_SERVER? {
        val info = getInfo(context, dal)
        return info.onServer
    }

    fun getOnServer(context: Context, dictName: String?): ON_SERVER? {
        val info = getInfo(context, dictName)
        return info!!.onServer
    }

    fun getDictISOCode(context: Context, dal: DictAndLoc): ISOCode? {
        val result = getInfo(context, dal).isoCode()
        Assert.assertTrueNR(null != result)
        return result
    }

    fun getDictISOCode(context: Context, dictName: String?): ISOCode {
        val info = getInfo(context, dictName)
        val result = info!!.isoCode()
        Assert.assertTrueNR(null != result)
        return result!!
    }

    fun getLangNameForISOCode(context: Context, isoCode: ISOCode): String? {
        var langName: String?
        DLCache.get(context).use { cache ->
            langName = cache!!.get(isoCode)
            if (null == langName) {
                // Any chance we have a installed dict providing this? How to
                // search given we can't read an ISOCode from a dict without
                // opening it.
            }
        }
        // Log.d( TAG, "getLangNameForISOCode(%s) => %s", isoCode, langName );
        return langName
    }

    fun setLangNameForISOCode(
        context: Context, isoCode: ISOCode?,
        langName: String?
    ) {
        // Log.d( TAG, "setLangNameForISOCode(%s=>%s)", isoCode, langName );
        DLCache.get(context).use { cache ->
            cache!!.put(isoCode, langName)
        }
    }

    fun getLangIsoCode(context: Context, langName: String): ISOCode {
        var result: ISOCode
        DLCache.get(context).use { cache ->
            // Log.d( TAG, "looking for %s in %H", langName, cache );
            result = cache!!.get(langName)!!
        }
        // Log.d( TAG, "getLangIsoCode(%s) => %s", langName, result );
        return result
    }

    fun getDictLangName(context: Context, dictName: String?): String? {
        val isoCode = getDictISOCode(context, dictName)
        return getLangNameForISOCode(context, isoCode)
    }

    fun getDictMD5Sums(context: Context, dict: String?): Array<String?> {
        val result = arrayOf<String?>(null, null)
        val info = getInfo(context, dict)
        info?.let {
            result[0] = it.md5Sum
            result[1] = it.fullSum
        }
        return result
    }

    fun getFileSize(context: Context, dal: DictAndLoc): Long {
        val path = dal.getPath(context!!)
        return path!!.length()
    }

    // May be called from background thread
    fun inval(
        context: Context, name: String?,
        loc: DictLoc?, added: Boolean
    ) {
        DBUtils.dictsRemoveInfo(context, DictUtils.removeDictExtn(name!!))

        if (added) {
            val dal = DictAndLoc(name, loc!!)
            getInfo(context, dal)
        }

        if (null != s_handler) {
            s_handler!!.post {
                if (null != s_dictsAdapter) {
                    rebuildAdapter(
                        s_dictsAdapter,
                        getHaveLang
                            (
                            context,
                            s_adaptedLang
                        )
                    )
                }
                if (null != s_langsAdapter) {
                    s_langsAdapter!!.rebuild()
                }
            }
        }
    }

    @JvmOverloads
    fun listLangs(context: Context,
                  dals: Array<DictAndLoc>? = DictUtils.dictList(context)): Array<String>
    {
        val langs: MutableSet<String> = HashSet()
        for (dal in dals!!) {
            var name = getDictLangName(context, dal.name)
            if (null == name || 0 == name.length) {
                Log.w(TAG, "bad lang name for dal name %s", dal.name)

                val di = getInfo(context, dal)
                if (null != di) {
                    name = di.langName
                    DLCache.get(context).use { cache ->
                        cache!!.put(di.isoCode(), name)
                    }
                }
            }
            if (null != name && 0 < name.length) {
                langs.add(name)
            }
        }
        val result = arrayOfNulls<String>(langs.size)
        return langs.toTypedArray()
    }

    fun getBestDefault(
        context: Context, isoCode: ISOCode,
        human: Boolean
    ): String? {
        var dictName: String? =
            if (human) CommonPrefs.getDefaultHumanDict(context)
            else CommonPrefs.getDefaultRobotDict(context)
        if (!isoCode.equals(getDictISOCode(context, dictName))) {
            val dicts = getHaveLangByCount(context, isoCode)
            dictName = if (dicts.size > 0) {
                // Human gets biggest; robot gets smallest
                dicts[if (human) 0 else dicts.size - 1]
            } else {
                null
            }
        }
        return dictName
    }

    private fun rebuildAdapter(
        adapter: ArrayAdapter<String>?,
        items: Array<String>
    ) {
        adapter!!.clear()

        for (item in items) {
            adapter.add(item)
        }
        if (null != s_last) {
            adapter.add(s_last)
        }
        adapter.sort(KeepLast)
    }

    fun setLast(lastItem: String?) {
        s_last = lastItem
        s_handler = Handler()
    }

    fun getLangsAdapter(context: Context): LangsArrayAdapter {
        if (null == s_langsAdapter) {
            s_langsAdapter =
                LangsArrayAdapter(
                    context,
                    android.R.layout.simple_spinner_item
                )
            s_langsAdapter!!.rebuild()
        }
        return s_langsAdapter!!
    }

    fun getDictsAdapter(
        context: Context,
        isoCode: ISOCode
    ): ArrayAdapter<String> {
        if (!isoCode.equals(s_adaptedLang)) {
            s_dictsAdapter =
                ArrayAdapter(context, android.R.layout.simple_spinner_item)
            rebuildAdapter(s_dictsAdapter, getHaveLang(context, isoCode))
            s_adaptedLang = isoCode
        }
        return s_dictsAdapter!!
    }

    private fun getInfo(context: Context, name: String?): DictInfo? {
        var result = DBUtils.dictsGetInfo(context, name)
        if (null == result) {
            val loc = DictUtils.getDictLoc(context, name!!)
            result = getInfo(context, DictAndLoc(name, loc!!))
        }
        return result
    }

    private fun getInfo(context: Context, dal: DictAndLoc): DictInfo {
        var info = DBUtils.dictsGetInfo(context, dal.name)

        // Tmp test that recovers from problem with new background download code
        if (null != info && null == info.isoCode()) {
            Log.w(
                TAG, "getInfo: dropping info for %s b/c lang code wrong",
                dal.name
            )
            info = null
        }

        if (null == info) {
            val names = arrayOf<String?>(dal.name)
            val pairs = DictUtils.openDicts(context, names)

            info = XwJNI.dict_getInfo(
                pairs.m_bytes[0], dal.name,
                pairs.m_paths[0],
                DictLoc.DOWNLOAD == dal.loc
            )
            if (null != info) {
                info.name = dal.name
                info.fullSum = getMD5SumFor(context, dal)
                Assert.assertTrueNR(null != info.fullSum)

                DBUtils.dictsSetInfo(context, dal, info)
                // Log.d( TAG, "getInfo() => %s", info );
            } else {
                Log.i(TAG, "getInfo(): unable to open dict %s", dal.name)
            }
        }
        return info
    }

    class LangsArrayAdapter(private val m_context: Context, itemLayout: Int) :
        ArrayAdapter<String?>(
            m_context, itemLayout
        ) {
        fun rebuild() {
            val langsSet: MutableSet<String> = HashSet()
            val dals = DictUtils.dictList(m_context)
            for (dal in dals!!) {
                val lang = getDictLangName(m_context, dal.name)
                if (null != lang && lang.isNotEmpty()) {
                    langsSet.add(lang)
                }
            }

            // Now build the array data
            clear()
            for (str in langsSet) {
                add(str)
            }
            if (null != s_last) {
                add(s_last)
            }
            sort(KeepLast)
        }

        fun getPosForLang(langName: String): Int {
            var result = -1
            for (ii in 0 until count) {
                if (langName == getLangAtPosition(ii)) {
                    result = ii
                    break
                }
            }
            return result
        }

        fun getLangAtPosition(position: Int): String? {
            return getItem(position)
        }
    }

    private class DLCache : AutoCloseable {
        private var mLangNames = HashMap<ISOCode?, String?>()
        private var mCurRev = 0

        @Transient
        private var mDirty = false

        @Transient
        private var mContext: Context? = null

        constructor()

        constructor(data: HashMap<ISOCode?, String?>, rev: Int) {
            mLangNames = data
            mCurRev = rev
        }

        fun get(langName: String): ISOCode? {
            var result: ISOCode? = null
            for (code in mLangNames.keys) {
                if (langName == mLangNames[code]) {
                    result = code
                    break
                }
            }
            if (null == result) {
                Log.d(TAG, "langName '%s' not in %s", langName, this)
            }
            return result
        }

        fun get(code: ISOCode?): String? {
            val result = mLangNames[code]
            if (null == result) {
                Log.d(TAG, "code '%s' not in %s", code, this)
            }
            return result
        }

        fun put(code: ISOCode?, langName: String?) {
            if (langName != mLangNames[code]) {
                mDirty = true
                mLangNames[code] = langName
            }
        }

        // @Override
        // public String toString()
        // {
        //     ArrayList<String> pairs = new ArrayList<>();
        //     for ( ISOCode code : mLangNames.keySet() ) {
        //         pairs.add(String.format("%s<=>%s", code, mLangNames.get(code) ) );
        //     }
        //     return TextUtils.join( ", ", pairs );
        // }
        override fun close() {
            if (mDirty) {
                DBUtils.setSerializableFor(mContext!!, CACHE_KEY_DATA, mLangNames)
                DBUtils.setIntFor(mContext!!, CACHE_KEY_REV, mCurRev)
                Log.d(TAG, "saveCache(%H) stored %s", this, this)
                mDirty = false
            }
            unlock()
            synchronized(sCache) {
                (sCache as Object).notifyAll()
            }
        }

        private fun update(context: Context) {
            if (mCurRev < BuildConfig.VERSION_CODE) {
                val res = context.resources
                val entries = res.getStringArray(R.array.language_names)
                var ii = 0
                while (ii < entries.size) {
                    val isoCode = ISOCode(entries[ii])
                    val langName = entries[ii + 1]
                    put(isoCode, langName)
                    ii += 2
                }
                mCurRev = BuildConfig.VERSION_CODE
                if (mDirty) {
                    Log.d(TAG, "updated cache; now %s", this)
                }
            }
        }

        private fun tryLock(context: Context): Boolean {
            val canLock = null == mContext
            if (canLock) {
                mContext = context
            }
            return canLock
        }

        private fun unlock() {
            Assert.assertTrueNR(null != mContext)
            mContext = null
        }

        companion object {
            private val CACHE_KEY_DATA = TAG + "/cache_data"
            private val CACHE_KEY_REV = TAG + "/cache_rev"
            private val sCache = arrayOf<DLCache?>(null)

            fun get(context: Context): DLCache? {
                var result: DLCache?
                synchronized(sCache) {
                    result = sCache[0]
                    if (null == result) {
                        val data = DBUtils.getSerializableFor(
                            context,
                            CACHE_KEY_DATA
                        ) as HashMap<ISOCode?, String?>?
                        if (null != data) {
                            val rev = DBUtils.getIntFor(context, CACHE_KEY_REV, 0)
                            result = DLCache(data, rev)
                            Log.d(TAG, "loaded cache: %s", result)
                        }
                    }
                    if (null == result) {
                        result = DLCache()
                    }
                    result!!.update(context)
                    sCache[0] = result
                    try {
                        while (!result!!.tryLock(context)) {
                            (sCache as Object).wait()
                        }
                    } catch (ioe: InterruptedException) {
                        Log.ex(TAG, ioe)
                        Assert.failDbg()
                    }
                }

                // Log.d( TAG, "getCache() => %H", sCache[0] );
                return sCache[0]
            }
        }
    }
}

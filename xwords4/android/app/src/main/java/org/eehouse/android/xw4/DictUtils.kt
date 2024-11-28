/*
 * Copyright 2009-2011 by Eric House (xwords@eehouse.org).  All rights
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
import android.os.Build
import android.os.Environment

import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream

import org.eehouse.android.xw4.jni.XwJNI

object DictUtils {
    private val TAG: String = DictUtils::class.java.simpleName

    private var s_dirGetter: SafeDirGetter? = null

    init {
        val sdkVersion = Build.VERSION.SDK.toInt()
        if (8 <= sdkVersion) {
            s_dirGetter = DirGetter()
        }
    }

    const val INVITED: String = "invited"

    private var s_dictListCache: Array<DictAndLoc>? = null

    init {
        MountEventReceiver.register(
            object:MountEventReceiver.SDCardNotifiee {
                override fun cardMounted(nowMounted: Boolean) {
                    invalDictList()
                }
            })
    }

    fun invalDictList() {
        s_dictListCache = null
        // Should I have a list of folks who want to know when this
        // changes?
    }

    private fun addLogDupIf(
        context: Context, map: MutableMap<String?, DictAndLoc>,
        path: String?, dir: File?, loc: DictLoc
    ) {
        if (isDict(context, path, dir)) {
            val name = removeDictExtn(File(path).name)
            if (map.containsKey(name)) {
                Log.d(TAG, "replacing info for %s with from %s", name, loc)
            }
            map[name] = DictAndLoc(name, loc)
        }
    }

    private fun tryDir(
        context: Context, dir: File?, strict: Boolean,
        loc: DictLoc, map: MutableMap<String?, DictAndLoc>
    ) {
        if (null != dir) {
            val list = dir.list()
            if (null != list) {
                for (file in list) {
                    addLogDupIf(context, map, file, if (strict) dir else null, loc)
                }
            }
        }
    }

    private var s_hadStorage: Boolean? = null
    fun dictList(context: Context): Array<DictAndLoc>? {
        // Note: if STORAGE permission is changed the set being returned here
        // will change. Might want to check for that and invalidate this list
        // if it's changed.
        val haveStorage = Perms23.havePermissions(
            context,
            Perms23.Perm.STORAGE
        )
        val permsChanged = (null == s_hadStorage
                || haveStorage != s_hadStorage)

        if (permsChanged || null == s_dictListCache) {
            val map: MutableMap<String?, DictAndLoc> = HashMap()

            for (file in getAssets(context)) {
                addLogDupIf(context, map, file, null, DictLoc.BUILT_IN)
            }

            for (file in context.fileList()) {
                addLogDupIf(context, map, file, null, DictLoc.INTERNAL)
            }

            tryDir(context, getSDDir(context), false, DictLoc.EXTERNAL, map)
            tryDir(
                context, getDownloadDir(context), true,
                DictLoc.DOWNLOAD, map
            )

            val dictSet: Collection<DictAndLoc> = map.values
            s_dictListCache =
                dictSet.toTypedArray<DictAndLoc>()
            s_hadStorage = haveStorage
            // Log.d( TAG, "created map: %s", map );
        }
        return s_dictListCache
    }

    fun getDictLoc(context: Context, name: String): DictLoc? {
        var name = name
        var loc: DictLoc? = null
        name = addDictExtn(name)

        for (file in getAssets(context)) {
            if (file == name) {
                loc = DictLoc.BUILT_IN
                break
            }
        }

        if (null == loc) {
            try {
                val fis = context.openFileInput(name)
                fis.close()
                loc = DictLoc.INTERNAL
            } catch (fnf: FileNotFoundException) {
                // Log.ex( fnf );
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
            }
        }

        if (null == loc) {
            val file = getSDPathFor(context, name)
            if (null != file && file.exists()) {
                loc = DictLoc.EXTERNAL
            }
        }

        if (null == loc) {
            val file = getDownloadsPathFor(context, name)
            if (null != file && file.exists()) {
                loc = DictLoc.DOWNLOAD
            }
        }

        // DbgUtils.logf( "getDictLoc(%s)=>%h(%s)", name, loc,
        //                ((null != loc)?loc.toString():"UNKNOWN") );
        return loc
    }

    fun dictExists(context: Context, name: String): Boolean {
        return null != getDictLoc(context, name)
    }

    fun dictIsBuiltin(context: Context, name: String): Boolean {
        return DictLoc.BUILT_IN == getDictLoc(context, name)
    }

    fun moveDict(context: Context, name: String,
                 from: DictLoc, to: DictLoc
    ): Boolean {
        var name = name
        val success: Boolean
        name = addDictExtn(name)

        val toPath = getDictFile(context, name, to)
        if (null != toPath && toPath.exists()) {
            success = false
        } else {
            success = copyDict(context, name, from, to)
            if (success) {
                deleteDict(context, name, from)
                invalDictList()
            }
        }
        return success
    }

    private fun copyDict(
        context: Context, name: String,
        from: DictLoc, to: DictLoc
    ): Boolean {
        Assert.assertFalse(from == to)
        var success = false

        try {
            val fis = if (DictLoc.INTERNAL == from
            ) context.openFileInput(name)
            else FileInputStream(getDictFile(context, name, from))

            val fos = if (DictLoc.INTERNAL == to
            ) context.openFileOutput(name, Context.MODE_PRIVATE)
            else FileOutputStream(getDictFile(context, name, to))

            success = DBUtils.copyStream(fos, fis)
            fos.close()
            fis.close()
        } catch (ex: IOException) {
            Log.ex(TAG, ex)
        }
        return success
    } // copyDict

    fun deleteDict(context: Context, name: String, loc: DictLoc) {
        var name = name
        name = addDictExtn(name)
        var path: File? = null
        when (loc) {
            DictLoc.DOWNLOAD -> path = getDownloadsPathFor(context, name)
            DictLoc.EXTERNAL -> path = getSDPathFor(context, name)
            DictLoc.INTERNAL -> context.deleteFile(name)
            else -> Assert.failDbg()
        }
        path?.delete()

        invalDictList()
    }

    fun deleteDict(context: Context, name: String) {
        val loc = getDictLoc(context, name)
        if (null != loc) {
            deleteDict(context, name, loc)
        }
    }

    private fun openDict(
        context: Context,
        name: String,
        loc: DictLoc = DictLoc.UNKNOWN
    ): ByteArray? {
        var bytes: ByteArray? = null
        val name = addDictExtn(name)

        if (loc == DictLoc.UNKNOWN || loc == DictLoc.BUILT_IN) {
            try {
                val am = context.assets
                val dict = am.open(name!!)

                val len = dict.available() // this may not be the
                // full length!
                val bas = ByteArrayOutputStream(len)
                val tmp = ByteArray(1024 * 16)
                while (true) {
                    val nRead = dict.read(tmp, 0, tmp.size)
                    if (0 >= nRead) {
                        break
                    }
                    bas.write(tmp, 0, nRead)
                }

                Assert.assertTrue(-1 == dict.read())
                bytes = bas.toByteArray()
            } catch (ee: IOException) {
            }
        }

        // not an asset?  Try external and internal storage
        if (null == bytes) {
            try {
                var fis: FileInputStream? = null
                if (null == fis) {
                    if (loc == DictLoc.UNKNOWN || loc == DictLoc.DOWNLOAD) {
                        val path = getDownloadsPathFor(context, name)
                        if (null != path && path.exists()) {
                            // DbgUtils.logf( "loading %s from Download", name );
                            fis = FileInputStream(path)
                        }
                    }
                }
                if (loc == DictLoc.UNKNOWN || loc == DictLoc.EXTERNAL) {
                    val sdFile = getSDPathFor(context, name)
                    if (null != sdFile && sdFile.exists()) {
                        // DbgUtils.logf( "loading %s from SD", name );
                        fis = FileInputStream(sdFile)
                    }
                }
                if (null == fis) {
                    if (loc == DictLoc.UNKNOWN || loc == DictLoc.INTERNAL) {
                        // DbgUtils.logf( "loading %s from private storage", name );
                        fis = context.openFileInput(name)
                    }
                }
                val len = fis!!.channel.size().toInt()
                bytes = ByteArray(len)
                fis.read(bytes, 0, len)
                fis.close()
                Log.i(TAG, "Successfully loaded %s", name)
            } catch (fnf: FileNotFoundException) {
                // Log.ex( fnf );
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
            }
        }

        return bytes
    } // openDict

    private fun getDictPath(context: Context, name: String): String? {
        var name = name
        name = addDictExtn(name)

        var file = context.getFileStreamPath(name)
        if (!file!!.exists()) {
            file = getSDPathFor(context, name)
            if (null != file && !file.exists()) {
                file = null
            }
        }
        val path = file?.path
        return path
    }

    private fun getDictFile(context: Context, name: String, to: DictLoc): File? {
        val path =
            when (to) {
                DictLoc.DOWNLOAD -> getDownloadsPathFor(context, name)
                DictLoc.EXTERNAL -> getSDPathFor(context, name)
                DictLoc.INTERNAL -> context.getFileStreamPath(name)
                else -> {
                    Assert.failDbg()
                    null
                }
            }
        return path
    }

    fun openDicts(context: Context, names: Array<String?>): DictPairs {
        val dictBytes = arrayOfNulls<ByteArray>(names.size)
        val dictPaths = arrayOfNulls<String>(names.size)

        val seen = HashMap<String, ByteArray?>()
        for (ii in names.indices) {
            var bytes: ByteArray? = null
            var path: String? = null
            names[ii]?.let {
                path = getDictPath(context, it)
                if (null == path) {
                    bytes = seen[it]
                    if (null == bytes) {
                        bytes = openDict(context, it)
                        seen[it] = bytes
                    }
                }
            }
            dictBytes[ii] = bytes
            dictPaths[ii] = path
        }
        return DictPairs(dictBytes, dictPaths)
    }

    fun saveDict(
        context: Context, `in`: InputStream,
        name: String, loc: DictLoc,
        dpl: DownProgListener
    ): Boolean {
        var name = name
        var success = false
        val tmpFile: File?
        val useSD = DictLoc.EXTERNAL == loc

        name = addDictExtn(name)
        val tmpName = name + "_tmp"
        tmpFile = if (useSD) {
            getSDPathFor(context, tmpName)
        } else {
            File(context.filesDir, tmpName)
        }

        if (null != tmpFile) {
            try {
                val fos = FileOutputStream(tmpFile)
                val buf = ByteArray(1024 * 4)
                var cancelled = false
                while (true) {
                    cancelled = dpl.isCancelled()
                    if (cancelled) {
                        tmpFile.delete()
                        break
                    }
                    val nRead = `in`.read(buf, 0, buf.size)
                    if (0 > nRead) {
                        break
                    }
                    fos.write(buf, 0, nRead)
                    dpl.progressMade(nRead)
                }
                fos.close()
                success = !cancelled
                if (success) {
                    invalDictList()
                }
            } catch (fnf: FileNotFoundException) {
                Log.ex(TAG, fnf)
            } catch (ioe: IOException) {
                Log.ex(TAG, ioe)
                tmpFile.delete()
            }
        }

        if (success) {
            val file = File(tmpFile!!.parent, name)
            success = tmpFile.renameTo(file)
            Assert.assertTrue(success || !BuildConfig.DEBUG)
        }

        // Log.d( TAG, "saveDict(%s/%s) => %b", name, loc, success );
        return success
    }

    /*
    // The goal here was to attach intalled dicts to email to save
    // folks having to download them, but there are too many problems
    // to make me think I can create a good UE.  Chief is that the
    // email clients are ignoring my mime type (though they require
    // one in the Intent) because they think they know what an .xwd
    // file is.  Without the right mime type I can't get Crosswords
    // launched to receive the attachment on the other end.
    // Additionally, gmail at least requires that the path start with
    // /mnt/sdcard, and there's no failure notification so I get to
    // warn users myself, or disable the share menuitem for dicts in
    // internal storage.
    public static void shareDict( Context context, String name )
    {
        Intent intent = new Intent( Intent.ACTION_SEND );
        intent.setType( "application/x-xwordsdict" );

        File dictFile = new File( getDictPath( context, name ) );
        DbgUtils.logf( "path: %s", dictFile.getPath() );
        Uri uri = Uri.fromFile( dictFile );
        DbgUtils.logf( "uri: %s", uri.toString() );
        intent.putExtra( Intent.EXTRA_STREAM, uri );

        intent.putExtra( Intent.EXTRA_SUBJECT,
                         context.getString( R.string.share_subject ) );
        intent.putExtra( Intent.EXTRA_TEXT,
                         Utils.format( context, R.string.share_bodyf, name ) );

        String title = context.getString( R.string.share_chooser );
        context.startActivity( Intent.createChooser( intent, title ) );
    }
     */
    private fun isGame(file: String): Boolean {
        return file.endsWith(XWConstants.GAME_EXTN)
    }

    private fun isDict(context: Context, file: String?, dir: File?): Boolean {
        val ok = file!!.endsWith(XWConstants.DICT_EXTN)
        return ok
    }

    fun removeDictExtn(str: String): String {
        var str = str
        while (str.endsWith(XWConstants.DICT_EXTN)) {
            val indx = str.lastIndexOf(XWConstants.DICT_EXTN)
            str = str.substring(0, indx)
        }
        return str
    }

    fun addDictExtn(str: String): String {
        var str = str
        if (!str.endsWith(XWConstants.DICT_EXTN)) {
            str += XWConstants.DICT_EXTN
        }
        return str
    }

    private fun getAssets(context: Context): Array<String> {
        try {
            val assetMgr = context.assets
            return assetMgr.list("")!!
        } catch (ioe: IOException) {
            Log.ex(TAG, ioe)
            return arrayOf<String>()
        }
    }

    fun haveWriteableSD(): Boolean {
        val state = Environment.getExternalStorageState()

        return state == Environment.MEDIA_MOUNTED
        // want this later? Environment.MEDIA_MOUNTED_READ_ONLY
    }

    private fun getSDDir(context: Context): File? {
        var result: File? = null
        if (haveWriteableSD()) {
            val storage = Environment.getExternalStorageDirectory()
            if (null != storage) {
                val packdir = String.format(
                    "Android/data/%s/files/",
                    BuildConfig.APPLICATION_ID
                )
                result = File(storage.path, packdir)
                if (!result.exists()) {
                    result.mkdirs()
                    if (!result.exists()) {
                        Log.w(TAG, "unable to create sd dir %s", packdir)
                        result = null
                    }
                }
            }
        }
        return result
    }

    private fun getSDPathFor(context: Context, name: String): File? {
        var result = getSDDir(context)?.let { File(it, name) }
        return result
    }

    fun haveDownloadDir(context: Context): Boolean {
        return null != getDownloadDir(context)
    }

    // Loop through three ways of getting the directory until one
    // produces a directory I can write to.
    fun getDownloadDir(context: Context): File? {
        var result: File? = null
        var attempt = 0
        outer@ while (true) {
            when (attempt) {
                0 -> {
                    val myPath = XWPrefs.getMyDownloadDir(context)
                    if (null == myPath || 0 == myPath.length) {
                        ++attempt
                        continue
                    }
                    result = File(myPath)
                }

                1 -> {
                    if (null == s_dirGetter) {
                        ++attempt
                        continue
                    }
                    result = s_dirGetter!!.getDownloadDir()
                }

                2 -> {
                    if (!haveWriteableSD()) {
                        ++attempt
                        continue
                    }
                    result = Environment.getExternalStorageDirectory()
                    if (2 == attempt && null != result) {
                        // the old way...
                        result = File(result, "download/")
                    }
                }

                else -> break@outer
            }
            // Exit test for loop
            if (null != result) {
                if (result.exists() && result.isDirectory && result.canWrite()) {
                    break@outer
                }
            }
            ++attempt
        }
        return result
    }

    private fun getDownloadsPathFor(context: Context, name: String?): File? {
        var result: File? = null
        val dir = getDownloadDir(context)
        if (dir != null) {
            result = File(dir, name)
        }
        return result
    }

    interface DownProgListener {
        fun progressMade(nBytes: Int)
        fun isCancelled(): Boolean
    }

    // Standard hack for using APIs from an SDK in code to ship on
    // older devices that don't support it: prevent class loader from
    // seeing something it'll barf on by loading it manually
    private interface SafeDirGetter {
        fun getDownloadDir(): File?
    }

    enum class ON_SERVER {
        UNKNOWN,
        YES,
        NO,
    }

    // keep in sync with loc_names string-array
    enum class DictLoc {
        UNKNOWN,
        BUILT_IN,
        INTERNAL,
        EXTERNAL,
        DOWNLOAD;

        fun needsStoragePermission(): Boolean {
            return this == DOWNLOAD
        }
    }

    class DictPairs(@JvmField var m_bytes: Array<ByteArray?>,
                    @JvmField var m_paths: Array<String?>) {
        fun anyMissing(names: Array<String?>): Boolean {
            var missing = false
            for (ii in m_paths.indices) {
                names[ii]?.let {
                    // It's ok for there to be no dict IFF there's no
                    // name.  That's a player using the default dict.
                    if (null == m_bytes[ii] && null == m_paths[ii]) {
                        Log.d(TAG, "anyMissing(): no bytes or path for $it")
                        missing = true
                        break
                    }
                }
            }
            return missing
        }
    } // DictPairs


    class DictAndLoc(pname: String, @JvmField var loc: DictLoc) : Comparable<Any?> {
        @JvmField
        var name = removeDictExtn(pname)
        var onServer: ON_SERVER? = null
        fun getPath(context: Context): File? {
            val path = getDictFile(context, addDictExtn(name), loc)
            return path
        }

        override fun equals(obj: Any?): Boolean {
            var result = false
            if (obj is DictAndLoc) {
                val other = obj

                result = name == other.name && loc == other.loc
            }
            return result
        }

        override fun compareTo(obj: Any?): Int {
            val other = obj as DictAndLoc?
            return name.compareTo(other!!.name)
        }

        override fun toString(): String {
            return String.format("%s:%s", name, loc)
        }
    }

    private class DirGetter : SafeDirGetter {
        override fun getDownloadDir(): File?
        {
            val path = Environment
                .getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            return path
        }
    }
}

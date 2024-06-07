/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2022 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.net.Uri
import android.provider.OpenableColumns

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.FileInputStream
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.io.IOException
import java.io.Serializable
import java.text.DateFormat
import java.util.Date
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

import org.eehouse.android.xw4.Assert.failDbg
import org.eehouse.android.xw4.DBHelper
import org.eehouse.android.xw4.DBUtils
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.loc.LocUtils

object ZipUtils {
    private val TAG: String = ZipUtils::class.java.simpleName

    @JvmStatic
    fun getMimeType(isStore: Boolean): String {
        return if (isStore) "application/x-zip" else "*/*"
        // return "application/octet-stream";
    }

    @JvmStatic
    fun getFileName(context: Context): String {
        val format = DateFormat.getDateInstance(DateFormat.SHORT)
        val date = format.format(Date())
        val name = LocUtils
            .getString(context, R.string.archive_filename_fmt, date)
            .replace('/', '-')
        return name
    }

    @JvmStatic
    fun getFileName(context: Context, uri: Uri?): String? {
        var result: String? = null
        val cursor = context.contentResolver
            .query(uri!!, null, null, null, null)
        if (null != cursor && cursor.moveToNext()) {
            val indx = cursor
                .getColumnIndex(OpenableColumns.DISPLAY_NAME)
            result = cursor.getString(indx)
        }
        return result
    }

    @JvmStatic
    fun hasWhats(context: Context, uri: Uri?): Boolean {
        val whats = getHasWhats(context, uri)
        return 0 < whats.size
    }

    fun getHasWhats(context: Context, uri: Uri?): List<SaveWhat> {
        val result: MutableList<SaveWhat> = ArrayList()
        try {
            iterate(context, uri, object : EntryIter {
                override fun withEntry(zis: ZipInputStream, what: SaveWhat): Boolean {
                    result.add(what)
                    return true
                }
            })
        } catch (ioe: IOException) {
            Log.ex(TAG, ioe)
        }
        Log.d(TAG, "getHasWhats() => %s", result)
        return result
    }

    // Return true if anything is loaded/changed, as caller will use result to
    // decide whether to restart process.
    @JvmStatic
    fun load(
        context: Context, uri: Uri,
        whats: List<SaveWhat?>
    ): Boolean {
        var result = false
        try {
            result = iterate(context, uri, object : EntryIter {
                @Throws(FileNotFoundException::class, IOException::class)
                override fun withEntry(zis: ZipInputStream, what: SaveWhat): Boolean {
                    var success = true
                    if (whats.contains(what)) {
                        when (what) {
                            SaveWhat.COLORS -> success = loadSettings(context, zis)
                            SaveWhat.SETTINGS -> success = loadSettings(context, zis)
                            SaveWhat.GAMES -> success = loadGames(context, zis)
                            else -> failDbg()
                        }
                    }
                    return success
                }
            })
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
            result = false
        }
        Log.d(TAG, "load(%s) => %b", whats, result)
        return result
    }

    @Throws(IOException::class, FileNotFoundException::class)
    private fun iterate(context: Context, uri: Uri?, iter: EntryIter): Boolean {
        var success = true
        context
            .contentResolver.openInputStream(uri!!).use { `is` ->
                val zis = ZipInputStream(`is`)
                while (success) {
                    val ze = zis.nextEntry ?: break
                    val name = ze.name
                    Log.d(TAG, "next entry name: %s", name)
                    val what = SaveWhat.valueOf(name)
                    success = iter.withEntry(zis, what)
                }
                zis.close()
            }
        return success
    }

    @JvmStatic
    fun save(
        context: Context, uri: Uri,
        whats: List<SaveWhat>
    ): Boolean {
        Log.d(TAG, "save(%s)", whats)
        var success = false
        val resolver = context.contentResolver
        // resolver.delete( uri, null, null ); // nuke the file if exists
        try {
            resolver.openOutputStream(uri).use { os ->
                val zos = ZipOutputStream(os)
                for (what in whats) {
                    zos.putNextEntry(ZipEntry(what.entryName()))
                    when (what) {
                        SaveWhat.COLORS -> success = saveColors(context, zos)
                        SaveWhat.SETTINGS -> success = saveSettings(context, zos)
                        SaveWhat.GAMES -> success = saveGames(context, zos)
                        else -> failDbg()
                    }
                    if (success) {
                        zos.closeEntry()
                    } else {
                        break
                    }
                }
                zos.close()
                os!!.close()
            }
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
        }
        Log.d(TAG, "save(%s) DONE", whats)
        return success
    }

    @Throws(FileNotFoundException::class, IOException::class)
    private fun saveGames(context: Context, zos: ZipOutputStream): Boolean {
        val name = DBHelper.getDBName()
        val gamesDB = context.getDatabasePath(name)
        val fis = FileInputStream(gamesDB)
        val success = DBUtils.copyStream(zos, fis)
        return success
    }

    @Throws(FileNotFoundException::class, IOException::class)
    private fun loadGames(context: Context, zis: ZipInputStream?): Boolean {
        val name = DBHelper.getDBName()
        val gamesDB = context.getDatabasePath(name)
        val fos = FileOutputStream(gamesDB)
        val success = DBUtils.copyStream(fos, zis!!)
        return success
    }

    @Throws(IOException::class)
    private fun saveSerializable(zos: ZipOutputStream, data: Serializable): Boolean {
        val asBytes = Utils.serializableToBytes(data)
        val bis = ByteArrayInputStream(asBytes)
        val success = DBUtils.copyStream(zos, bis)
        return success
    }

    @Throws(IOException::class)
    private fun saveColors(context: Context, zos: ZipOutputStream): Boolean {
        val map = PrefsDelegate.getPrefsColors(context)
        return saveSerializable(zos, map)
    }

    @Throws(IOException::class)
    private fun saveSettings(context: Context, zos: ZipOutputStream): Boolean {
        val map = PrefsDelegate.getPrefsNoColors(context)
        return saveSerializable(zos, map)
    }

    private fun loadSettings(context: Context, zis: ZipInputStream?): Boolean {
        val bos = ByteArrayOutputStream()
        val success = DBUtils.copyStream(bos, zis!!)
        if (success) {
            val map = Utils.bytesToSerializable(bos.toByteArray())
            PrefsDelegate.loadPrefs(context, map)
        }
        return success
    }

    enum class SaveWhat(private val mTitle: Int, private val mExpl: Int) {
        COLORS(R.string.archive_title_colors, R.string.archive_expl_colors),
        SETTINGS(R.string.archive_title_settings, R.string.archive_expl_settings),
        GAMES(R.string.archive_title_games, R.string.archive_expl_games),
        ;

        fun entryName(): String {
            return toString()
        }

        fun titleID(): Int {
            return mTitle
        }

        fun explID(): Int {
            return mExpl
        }
    }

    private interface EntryIter {
        @Throws(FileNotFoundException::class, IOException::class)
        fun withEntry(zis: ZipInputStream, what: SaveWhat): Boolean
    }
}

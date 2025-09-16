/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4.jni

import java.io.Serializable

object TmpDict {

    interface DictIterProcs {
        fun onIterReady(iterRef: IterWrapper?)
    }

    // Dicts
    class DictWrapper {
        var dictPtr: Long
            private set

        constructor() {
            dictPtr = 0L
        }

        constructor(dictPtr: Long) {
            this.dictPtr = dictPtr
            dict_ref(dictPtr)
        }

        fun release() {
            if (0L != dictPtr) {
                dict_unref(dictPtr)
                dictPtr = 0
            }
        }

        // @Override
        @Throws(Throwable::class)
        fun finalize() {
            release()
        }

        fun getChars(): Array<String>? {
            return dict_getChars(dictPtr)
        }

        fun tilesToStr(tiles: ByteArray?, delim: String?): String? {
            return dict_tilesToStr(dictPtr, tiles, delim)
        }

        fun getInfo(check: Boolean): DictInfo {
            val jniState = Device.ptrGlobals()
            return dict_getInfo(jniState, dictPtr, check)
        }

        fun getTilesInfo(): String {
            val jniState = Device.ptrGlobals()
            return dict_getTilesInfo(jniState, dictPtr)
        }

        fun hasDuplicates(): Boolean {
            return dict_hasDuplicates(dictPtr)
        }

        fun strToTiles(str: String): Array<ByteArray>? {
            return dict_strToTiles(dictPtr, str)
        }

        fun getDesc(): String? {
            return dict_getDesc(dictPtr)
        }

        suspend fun makeDI(pats: Array<PatDesc>,
                           minLen: Int, maxLen: Int
        ): IterWrapper? {
            val jniState = Device.ptrGlobals()
            val iterPtr = Device.await {
                di_init(jniState, dictPtr, pats, minLen, maxLen)
            } as Long
            val wrapper =
                if (0L == iterPtr) null
                else IterWrapper(iterPtr)
            return wrapper
        }
    }

    class IterWrapper(private val ref: Long) {

        @Throws(Throwable::class)
        fun finalize() {
            di_destroy(ref)
        }

        fun getIndices(): IntArray? {
            return di_getIndices(ref)
        }

        fun getPrefixes(): Array<String>? {
            return di_getPrefixes(ref)
        }

        fun wordCount(): Int {
            return di_wordCount(ref)
        }

        fun nthWord(nn: Int, delim: String?): String? {
            return di_nthWord(ref, nn, delim)
        }

        fun getMinMax(): IntArray {
            return di_getMinMax(ref)
        }

    }

    class PatDesc : Serializable {
        @JvmField
        var strPat: String? = null
        @JvmField
        var tilePat: ByteArray? = null
        @JvmField
        var anyOrderOk = false
        override fun toString(): String {
            return String.format(
                "{str: %s; nTiles: %d; anyOrderOk: %b}",
                strPat, tilePat?.size ?: 0, anyOrderOk )
        }
    }

	@JvmStatic
    private external fun di_destroy(closure: Long)
	@JvmStatic
    private external fun di_wordCount(closure: Long): Int
	@JvmStatic
    private external fun di_nthWord(closure: Long, nn: Int,
                                    delim: String?): String?
	@JvmStatic
    private external fun di_getMinMax(closure: Long): IntArray
	@JvmStatic
    private external fun di_getPrefixes(closure: Long): Array<String>?
	@JvmStatic
    private external fun di_getIndices(closure: Long): IntArray?
    @JvmStatic
    external fun dict_tilesAreSame(curPtr: Long, newPtr: Long): Boolean
    @JvmStatic
    external fun dict_getChars(dict: Long): Array<String>?

    fun dict_getInfo(
        dict: ByteArray?, name: String?, path: String?,
        check: Boolean
    ): DictInfo {
        val wrapper = makeDict(dict, name, path)
        return wrapper.getInfo(check)
    }

    // Dict iterator
    const val MAX_COLS_DICT = 15 // from dictiter.h
    fun makeDict(
        bytes: ByteArray?, name: String?, path: String?
    ) : DictWrapper {
        val jniState = Device.ptrGlobals()
        val dict = dict_make(jniState, bytes, name, path)
        return DictWrapper(dict)
    }

	@JvmStatic
    private external fun dict_make(jniState: Long, dict: ByteArray?,
                                   name: String?, path: String?)
        : Long

	@JvmStatic
    private external fun dict_ref(dictPtr: Long)
	@JvmStatic
    private external fun dict_unref(dictPtr: Long)
	@JvmStatic
    private external fun dict_strToTiles(dictPtr: Long,
                                         str: String):
        Array<ByteArray>?
	@JvmStatic
    private external fun dict_tilesToStr(dictPtr: Long, tiles: ByteArray?, delim: String?): String?
	@JvmStatic
    private external fun dict_hasDuplicates(dictPtr: Long): Boolean
	@JvmStatic
    private external fun dict_getTilesInfo(jniState: Long, dictPtr: Long): String
	@JvmStatic
    private external fun dict_getInfo(
        jniState: Long, dictPtr: Long,
        check: Boolean
    ): DictInfo

	@JvmStatic
    private external fun dict_getDesc(dictPtr: Long): String?
	@JvmStatic
    private external fun di_init(
        jniState: Long, dictPtr: Long,
        pats: Array<PatDesc>, minLen: Int, maxLen: Int
    ): Long
}


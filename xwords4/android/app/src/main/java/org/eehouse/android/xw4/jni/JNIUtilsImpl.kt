/*
 * Copyright 2009-2024 by Eric House (xwords@eehouse.org).  All
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
package org.eehouse.android.xw4.jni

import android.content.Context

import java.io.ByteArrayInputStream
import java.io.IOException
import java.io.InputStreamReader
import java.io.UnsupportedEncodingException

import org.eehouse.android.xw4.Assert
import org.eehouse.android.xw4.DBUtils
import org.eehouse.android.xw4.Log
import org.eehouse.android.xw4.Utils
import org.eehouse.android.xw4.XWApp

class JNIUtilsImpl private constructor(private val m_context: Context) : JNIUtils {
    /** Working around lack of utf8 support on the JNI side: given a
     * utf-8 string with embedded small number vals starting with 0,
     * convert into individual strings.  The 0 is the problem: it's
     * not valid utf8.  So turn it and the other nums into strings and
     * catch them on the other side.
     *
     * Changes for "synonyms" (A and a being the same tile): return an
     * array of Strings for each face.  Each face is
     * <letter>[<delim><letter></letter>]*, so for each loop until the delim
     * isn't found.
    </delim></letter> */
    override fun splitFaces(chars: ByteArray, isUTF8: Boolean): Array<Array<String>>
    {
        val faces = ArrayList<Array<String>>()
        val bais = ByteArrayInputStream(chars)
        var isr: InputStreamReader
        try {
            isr = InputStreamReader(bais, if (isUTF8) "UTF8" else "ISO8859_1")
        } catch (uee: UnsupportedEncodingException) {
            Log.i(TAG, "splitFaces: %s", uee.toString())
            isr = InputStreamReader(bais)
        }

        val codePoints = IntArray(1)

        // "A aB bC c"
        var lastWasDelim = false
        var face: ArrayList<String>? = null
        while (true) {
            var chr = -1
            try {
                chr = isr.read()
            } catch (ioe: IOException) {
                Log.w(TAG, ioe.toString())
            }
            if (-1 == chr) {
                addFace(faces, face)
                break
            } else if (SYNONYM_DELIM.code == chr) {
                Assert.assertNotNull(face)
                lastWasDelim = true
                continue
            } else {
                var letter: String
                if (chr < 32) {
                    letter = String.format("%d", chr)
                } else {
                    codePoints[0] = chr
                    letter = String(codePoints, 0, 1)
                }
                // Ok, we have a letter.  Is it part of an existing
                // one or the start of a new?  If the latter, insert
                // what we have before starting over.
                if (null == face) { // start of a new, clearly
                    // do nothing
                } else {
                    Assert.assertTrue(0 < face.size)
                    if (!lastWasDelim) {
                        addFace(faces, face)
                        face = null
                    }
                }
                lastWasDelim = false
                if (null == face) {
                    face = ArrayList()
                }
                face.add(letter)
            }
        }

        val result = faces.toTypedArray<Array<String>>()
        return result
    }

    private fun addFace(faces: ArrayList<Array<String>>, face: ArrayList<String>?) {
        faces.add(face!!.toTypedArray<String>())
    }

    override fun getMD5SumFor(bytes: ByteArray?): String? {
        return Utils.getMD5SumFor(bytes)
    }

    override fun getMD5SumFor(dictName: String, bytes: ByteArray?): String? {
        var result: String? = null
        if (null == bytes) {
            result = DBUtils.dictsGetMD5Sum(m_context, dictName)
        } else {
            result = getMD5SumFor(bytes)
            // Is this needed?  Caller might be doing it anyway.
            DBUtils.dictsSetMD5Sum(m_context, dictName, result!!)
        }
        return result!!
    }

    companion object {
        private val TAG: String = JNIUtilsImpl::class.java.simpleName

        private const val SYNONYM_DELIM = ' '

        private var s_impl: JNIUtilsImpl? = null
        @Synchronized
        fun get(): JNIUtils {
            if (null == s_impl) {
                s_impl = JNIUtilsImpl(XWApp.getContext())
            }
            return s_impl!!
        }
    }
}

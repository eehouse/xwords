/*
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All
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
import android.graphics.Bitmap
import org.eehouse.android.xw4.jni.DrawCtx
import org.eehouse.android.xw4.jni.TmpDict

private val TAG = ThumbCanvas::class.java.getSimpleName()
class ThumbCanvas(context: Context, bitmap: Bitmap, private val mSize: Int)
	: BoardCanvas(context, bitmap, DrawCtx.DT_THUMB) {

    // Unlike superclass, where the game was loaded on the main thread
    // but dictChanged() is called on the JNI thread, this instance
    // will have been created on the same background thread that's
    // calling us.  So don't switch threads for the dict_getChars()
    // call
    override fun dictChanged(dictPtr: Long) {
        if (0L != dictPtr) {
            mFontDims = null
            mDictChars = TmpDict.dict_getChars(dictPtr)
        }
    }

    override fun getThumbSize(): Int {
        return mSize;
    }
}

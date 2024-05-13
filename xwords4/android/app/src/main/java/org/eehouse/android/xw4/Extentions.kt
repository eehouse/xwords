/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.ContentValues
import android.os.Bundle
import org.json.JSONObject
import java.io.Serializable

fun Bundle.putLongAnd(key: String, value: Long): Bundle {
	this.putLong( key, value )
	return this
}

fun Bundle.putIntAnd(key: String, value: Int): Bundle {
	this.putInt( key, value )
	return this
}

fun Bundle.putBooleanAnd(key: String, value: Boolean): Bundle {
	this.putBoolean( key, value )
	return this
}

fun Bundle.putBooleanArrayAnd(key: String, value: BooleanArray?): Bundle {
	this.putBooleanArray( key, value )
	return this
}

fun Bundle.putStringAnd(key: String, value: String?): Bundle {
	this.putString(key, value)
    return this
}

fun Bundle.putStringArrayAnd(key: String, value: Array<String?>?): Bundle {
	this.putStringArray(key, value)
    return this
}

fun Bundle.putSerializableAnd(key: String, value: Serializable?): Bundle {
	this.putSerializable(key, value)
    return this
}

fun Bundle.putCharSequenceAnd(key: String, value: CharSequence?): Bundle {
	this.putCharSequence(key, value)
    return this
}

fun ContentValues.putAnd(key: String, value: Int): ContentValues {
	this.put(key, value)
	return this
}

fun ContentValues.putAnd(key: String, value: String?): ContentValues {
	this.put(key, value)
	return this
}

fun ContentValues.putAnd(key: String, value: Long): ContentValues {
	this.put(key, value)
	return this
}

fun ContentValues.putAnd(key: String, value: ByteArray): ContentValues {
	this.put(key, value)
	return this
}

fun JSONObject.putAnd(key: String, value: String): JSONObject {
	this.put( key, value )
	return this
}

fun JSONObject.putAnd(key: String, value: Int): JSONObject {
	this.put( key, value )
	return this
}

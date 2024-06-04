/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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

import org.eehouse.android.xw4.DbgUtils.printStack

object Assert {
    private val TAG: String = Assert::class.java.simpleName

    fun fail() {
        assertTrue(false)
    }

    @JvmStatic
    fun failDbg() {
        assertFalse(BuildConfig.DEBUG)
    }

    @JvmStatic
    fun assertFalse(`val`: Boolean) {
        assertTrue(!`val`)
    }

    @JvmStatic
    fun assertTrue(`val`: Boolean) {
        if (!`val`) {
            Log.e(TAG, "firing assert!")
            printStack(TAG)
            assert(false)
            throw RuntimeException()
        }
    }

    // NR: non-release
    @JvmStatic
    fun assertTrueNR(`val`: Boolean) {
        if (BuildConfig.NON_RELEASE) {
            assertTrue(`val`)
        }
    }

    @JvmStatic
    fun assertNotNull(`val`: Any?) {
        assertTrue(`val` != null)
    }

    @JvmStatic
    fun assertVarargsNotNullNR(vararg params: Any?) {
        if (BuildConfig.NON_RELEASE) {
            if (null == params) {
                printStack(TAG)
                // assertNotNull( params );
            }
        }
    }

    @JvmStatic
    fun assertNull(`val`: Any?) {
        assertTrue(`val` == null)
    }

    fun assertEquals(obj1: Any?, obj2: Any?) {
        assertTrue(
            (obj1 == null && obj2 == null)
                    || (obj1 != null && obj1 == obj2)
        )
    }
}
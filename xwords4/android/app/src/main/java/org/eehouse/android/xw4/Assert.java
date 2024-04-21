/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
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

package org.eehouse.android.xw4;

public class Assert {
    private static final String TAG = Assert.class.getSimpleName();

    public static void fail() { assertTrue(false); }

    public static void failDbg() { assertFalse( BuildConfig.DEBUG ); }

    public static void assertFalse(boolean val)
    {
        assertTrue(! val);
    }
    
    public static void assertTrue( boolean val )
    {
        if (! val) {
            Log.e( TAG, "firing assert!" );
            DbgUtils.printStack( TAG );
            assert false;
            throw new RuntimeException();
        }
    }

    // NR: non-release
    public static void assertTrueNR( boolean val )
    {
        if ( BuildConfig.NON_RELEASE ) {
            assertTrue( val );
        }
    }

    public static void assertNotNull( Object val )
    {
        assertTrue( val != null );
    }

    public static void assertVarargsNotNullNR( Object[] params )
    {
        if ( BuildConfig.NON_RELEASE ) {
            if ( null == params ) {
                DbgUtils.printStack(TAG);
                // assertNotNull( params );
            }
        }
    }

    public static void assertNull( Object val )
    {
        assertTrue( val == null );
    }

    public static void assertEquals( Object obj1, Object obj2 )
    {
        assertTrue( (obj1 == null && obj2 == null)
                    || (obj1 != null && obj1.equals(obj2)) );
    }
}

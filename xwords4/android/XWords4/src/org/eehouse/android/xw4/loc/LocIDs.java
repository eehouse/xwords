/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4.loc;

import java.util.Iterator;
import java.util.Map;

import junit.framework.Assert;

import org.eehouse.android.xw4.DbgUtils;

public class LocIDs extends LocIDsData {
    private static String[] s_keys;

    protected static String getNthKey( int indx )
    {
        if ( null == s_keys ) {
            Map<String, Integer> map = LocIDsData.S_MAP;
            s_keys = new String[map.size()];
            Iterator<String> iter = map.keySet().iterator();
            for ( int ii = 0; iter.hasNext(); ++ii ) {
                s_keys[ii] = iter.next();
            }
        }

        return s_keys[indx];
    }

    protected static int getID( String key )
    {
        int result = LocIDsData.NOT_FOUND;
        if ( null != key && LocIDsData.S_MAP.containsKey( key ) ) {
            Assert.assertNotNull( LocIDsData.S_MAP );
            DbgUtils.logf( "calling get with key %s", key );
            result = LocIDsData.S_MAP.get( key ); // NPE
        }
        return result;
    }

    protected static int size()
    {
        return LocIDsData.S_MAP.size();
    }
}

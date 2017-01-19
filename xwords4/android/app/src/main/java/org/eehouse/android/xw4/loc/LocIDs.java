/* -*- compile-command: "find-and-gradle.sh installXw4Debug"; -*- */
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

import android.content.Context;

import org.eehouse.android.xw4.DbgUtils;

import java.util.HashMap;

public class LocIDs extends LocIDsData {
    private static final String TAG = LocIDs.class.getSimpleName();
    private static String[] s_keys;
    private static HashMap<String, Integer> S_MAP = null;

    protected static int getID( Context context, String key )
    {
        int result = LocIDsData.NOT_FOUND;
        if ( null != key && getS_MAP(context).containsKey( key ) ) {
            // Assert.assertNotNull( LocIDsData.S_MAP );
            DbgUtils.logw( TAG, "calling get with key %s", key );
            result = getS_MAP( context ).get( key ); // NPE
        }
        return result;
    }

    protected static int size()
    {
        return S_IDS.length;
    }

    protected static HashMap<String, Integer> getS_MAP( Context context )
    {
        if ( null == S_MAP ) {
            S_MAP = new HashMap<String, Integer>(S_IDS.length);
            for ( int id : S_IDS ) {
                String str = context.getString( id );
                S_MAP.put( str, id );
            }

            LocIDsData.checkStrings( context );
        }
        return S_MAP;
    }
}

/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2020 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context;

import java.util.HashMap;
import java.util.Map;
import java.io.Serializable;

public class Quarantine {
    private static final String TAG = Quarantine.class.getSimpleName();
    private static final String DATA_KEY = TAG + "/key";

    public static boolean safeToOpen( long rowid )
    {
        int count;
        synchronized ( sDataRef ) {
            count = get().getFor( rowid );
        }
        boolean result = count == 0; // Not too strict?
        if ( !result ) {
            Log.d( TAG, "safeToOpen(%d) => %b (count=%d)", rowid, result, count );
        }
        return result;
    }

    public static void clear( long rowid )
    {
        synchronized ( sDataRef ) {
            get().clear( rowid );
            store();
        }
    }

    public static void recordOpened( long rowid )
    {
        synchronized ( sDataRef ) {
            get().increment( rowid );
            store();
            Log.d( TAG, "recordOpened(%d): %s", rowid, sDataRef[0].toString() );
        }
    }

    public static void recordClosed( long rowid )
    {
        synchronized ( sDataRef ) {
            get().decrement( rowid );
            store();
            Log.d( TAG, "recordClosed(%d): %s", rowid, sDataRef[0].toString() );
        }
    }

    private static class Data implements Serializable {
        private HashMap<Long, Integer> mCounts = new HashMap<>();

        synchronized void increment( long rowid ) {
            if ( ! mCounts.containsKey(rowid) ) {
                mCounts.put(rowid, 0);
            }
            mCounts.put( rowid, mCounts.get(rowid) + 1 );
        }

        synchronized void decrement( long rowid )
        {
            Assert.assertTrue( mCounts.containsKey(rowid) );
            mCounts.put( rowid, mCounts.get(rowid) - 1 );
            Assert.assertTrueNR( mCounts.get(rowid) >= 0 );
        }

        synchronized int getFor( long rowid )
        {
            int result = mCounts.containsKey(rowid) ? mCounts.get( rowid ) : 0;
            return result;
        }

        synchronized void clear( long rowid )
        {
            mCounts.put( rowid, 0 );
        }

        @Override
        synchronized public String toString()
        {
            StringBuilder sb = new StringBuilder().append("[");
            synchronized ( mCounts ) {
                for ( long rowid : mCounts.keySet() ) {
                    int count = mCounts.get(rowid);
                    sb.append( String.format("{%d: %d}", rowid, count ) );
                }
            }
            return sb.append("]").toString();
        }
    }

    private static Data[] sDataRef = {null};
    
    private static void store()
    {
        synchronized( sDataRef ) {
            DBUtils.setSerializableFor( getContext(), DATA_KEY, sDataRef[0] );
        }
    }

    private static Data get()
    {
        Data data;
        synchronized ( sDataRef ) {
            data = sDataRef[0];
            if ( null == data ) {
                data = (Data)DBUtils.getSerializableFor( getContext(), DATA_KEY );
                if ( null == data ) {
                    data = new Data();
                } else {
                    Log.d( TAG, "loading existing: %s", data );
                }
                sDataRef[0] = data;
            }
        }
        return data;
    }

    private static Context getContext()
    {
        return XWApp.getContext();
    }
}

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

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class Quarantine {
    private static final String TAG = Quarantine.class.getSimpleName();
    private static final String DATA_KEY = TAG + "/key";
    private static QData[] sDataRef = {null};

    public static int getCount( long rowid )
    {
        int result;
        synchronized ( sDataRef ) {
            result = get().countFor( rowid );
        }
        return result;
    }

    private static final Set<Long> sLogged = new HashSet<>();
    public synchronized static boolean safeToOpen( long rowid )
    {
        int count = getCount( rowid );
        boolean result = count < BuildConfig.BAD_COUNT;
        if ( !result ) {
            Log.d( TAG, "safeToOpen(%d) => %b (count=%d)", rowid, result, count );
            if ( BuildConfig.NON_RELEASE && !sLogged.contains(rowid) ) {
                sLogged.add(rowid);
                Log.d( TAG, "printing calling stack:" );
                DbgUtils.printStack( TAG );
                List<StackTraceElement[]> list = get().listFor( rowid );
                for ( int ii = 0; ii < list.size(); ++ii ) {
                    StackTraceElement[] trace = list.get( ii );
                    Log.d( TAG, "printing saved stack %d (of %d):", ii, list.size() );
                    DbgUtils.printStack( TAG, trace );
                }
            }
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
            int newCount = get().increment( rowid );
            store();
            Log.d( TAG, "recordOpened(%d): %s (count now %d)", rowid,
                   sDataRef[0].toString(), newCount );
            // DbgUtils.printStack( TAG );
        }
    }

    public static void recordClosed( long rowid )
    {
        synchronized ( sDataRef ) {
            get().clear( rowid );
            store();
            Log.d( TAG, "recordClosed(%d): %s (count now 0)", rowid,
                   sDataRef[0].toString() );
        }
    }

    public static void markBad( long rowid )
    {
        synchronized ( sDataRef ) {
            for ( int ii = 0; ii < BuildConfig.BAD_COUNT; ++ii ) {
                get().increment( rowid );
            }
            store();
            Log.d( TAG, "markBad(%d): %s", rowid, sDataRef[0].toString() );
        }
        GameListItem.inval( rowid );
    }

    private static class QData implements Serializable {
        private HashMap<Long, List<StackTraceElement[]>> mCounts = new HashMap<>();

        synchronized int increment( long rowid )
        {
            if ( ! mCounts.containsKey(rowid) ) {
                mCounts.put(rowid, new ArrayList<StackTraceElement[]>());
            }
            // null: in release case, we just need size() to work
            StackTraceElement[] stack = BuildConfig.NON_RELEASE
                ? Thread.currentThread().getStackTrace() : null;
            List<StackTraceElement[]> list = mCounts.get( rowid );
            list.add( stack );
            return list.size();
        }

        synchronized int countFor( long rowid )
        {
            List<StackTraceElement[]> list =  listFor( rowid );
            int result = list == null ? 0 : list.size();
            return result;
        }

        synchronized List<StackTraceElement[]> listFor( long rowid )
        {
            return mCounts.containsKey( rowid ) ? mCounts.get( rowid ) : null;
        }

        synchronized void clear( long rowid )
        {
            mCounts.remove( rowid );
        }

        synchronized void removeZeros()
        {
            for ( Iterator<List<StackTraceElement[]>> iter = mCounts.values().iterator();
                  iter.hasNext(); ) {
                if ( 0 == iter.next().size() ) {
                    iter.remove();
                }
            }
        }

        @Override
        synchronized public String toString()
        {
            StringBuilder sb = new StringBuilder();
            synchronized ( mCounts ) {
                sb.append("{len:").append(mCounts.size())
                    .append(", data:[");
                for ( long rowid : mCounts.keySet() ) {
                    int count = mCounts.get(rowid).size();
                    sb.append( String.format("{%d: %d}", rowid, count ) );
                }
            }
            return sb.append("]}").toString();
        }
    }

    private static void store()
    {
        synchronized( sDataRef ) {
            DBUtils.setSerializableFor( getContext(), DATA_KEY, sDataRef[0] );
        }
    }

    private static QData get()
    {
        QData data;
        synchronized ( sDataRef ) {
            data = sDataRef[0];
            if ( null == data ) {
                data = (QData)DBUtils.getSerializableFor( getContext(), DATA_KEY );
                if ( null == data ) {
                    data = new QData();
                } else {
                    Log.d( TAG, "loading existing: %s", data );
                    data.removeZeros();
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

/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2010 - 2015 by Eric House (xwords@eehouse.org).  All
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

package org.eehouse.android.xw4;

import android.content.Context;
import android.content.Intent;
import androidx.core.app.JobIntentService;

import java.util.HashMap;
import java.util.Map;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

abstract class XWJIService extends JobIntentService {
    private static final String TAG = XWJIService.class.getSimpleName();
    private static final boolean LOG_INTENT_COUNTS = false;
    private static final boolean LOG_PACKETS = false;

    static final String CMD_KEY = "CMD";
    private static final String TIMESTAMP = "TIMESTAMP";

    public interface XWJICmds {
        public int ordinal();
    }

    abstract void onHandleWorkImpl( Intent intent, XWJICmds cmd, long timestamp );
    abstract XWJICmds[] getCmds();

    private static Map<Class, Integer> sJobIDs = new HashMap<>();
    private static Map<Class, CommsConnType> sTypes = new HashMap<>();
    static void register( Class clazz, int jobID, CommsConnType typ )
    {
        sJobIDs.put( clazz, jobID );
        sTypes.put( clazz, typ );
    }
    
    @Override
    public final void onHandleWork( Intent intent )
    {
        long timestamp = getTimestamp(intent);
        long ageMS = System.currentTimeMillis() - timestamp;

        XWJICmds cmd = cmdFrom( intent );
        if ( LOG_PACKETS ) {
            Log.d( getClass().getSimpleName(),
                   "onHandleWork(): cmd=%s; age=%dms; threadCount: %d)",
                   cmd, ageMS, Thread.activeCount() );
        }

        onHandleWorkImpl( intent, cmd, timestamp );
    }

    protected static void enqueueWork( Context context, Class clazz, Intent intent )
    {
        enqueueWork( context, clazz, sJobIDs.get(clazz), intent );
    }

    static XWJICmds cmdFrom( Intent intent, XWJICmds[] values )
    {
        int ord = intent.getIntExtra( CMD_KEY, -1 );
        return values[ord];
    }

    XWJICmds cmdFrom( Intent intent )
    {
        return cmdFrom( intent, getCmds() );
    }

    long getTimestamp( Intent intent )
    {
        long result = intent.getLongExtra( TIMESTAMP, 0 );
        return result;
    }

    static Intent getIntentTo( Context context, Class clazz, XWJICmds cmd )
    {
        Intent intent = new Intent( context, clazz )
            .putExtra( CMD_KEY, cmd.ordinal() )
            .putExtra( TIMESTAMP, System.currentTimeMillis() );
        return intent;
    }
}

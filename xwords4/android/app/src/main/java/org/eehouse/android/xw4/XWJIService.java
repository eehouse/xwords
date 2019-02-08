/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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


import android.support.v4.app.JobIntentService;
import android.content.Context;
import android.content.Intent;

abstract class XWJIService extends JobIntentService {
    static final String CMD_KEY = "CMD";
    private static final String TIMESTAMP = "TIMESTAMP";

    public interface XWJICmds {
        public int ordinal();
    }

    abstract void onHandleWorkImpl( Intent intent, XWJICmds cmd, long timestamp );
    abstract XWJICmds[] getCmds();
    
    @Override
    public final void onHandleWork( Intent intent )
    {
        long timestamp = getTimestamp(intent);
        XWJICmds cmd = cmdFrom( intent );
        Log.d( getClass().getSimpleName(),
               "onHandleWork(): cmd=%s; age=%dms; threadCount: %d)",
               cmd, System.currentTimeMillis() - timestamp,
               Thread.activeCount() );

        onHandleWorkImpl( intent, cmd, timestamp );
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

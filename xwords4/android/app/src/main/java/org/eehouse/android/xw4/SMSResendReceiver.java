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

import android.content.Context;

import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

/*
 * SMS messages get dropped. We resend pending relay messages when we gain
 * network connectivity. There's no similar event for gaining the ability to
 * send SMS, so this class handles doing it on a timer. With backoff.
 */

public class SMSResendReceiver {
    private static final String TAG = SMSResendReceiver.class.getSimpleName();

    private static final String BACKOFF_KEY = TAG + "/backoff";
    private static final int MIN_BACKOFF_SECONDS = 60 * 5;
    private static final int MAX_BACKOFF_SECONDS = 60 * 60 * 12;

    private static TimerReceiver.TimerCallback sTimerCallbacks
        = new TimerReceiver.TimerCallback() {
                @Override
                public void timerFired( Context context )
                {
                    GameUtils.resendAllIf( context, CommsConnType.COMMS_CONN_SMS, true,
                                           new GameUtils.ResendDoneProc() {
                                               @Override
                                               public void onResendDone( Context context,
                                                                         int nSent ) {
                                                   int backoff = -1;
                                                   if ( 0 < nSent ) {
                                                       backoff = setTimer( context, true );
                                                   }
                                                   if ( BuildConfig.NON_RELEASE ) {
                                                       DbgUtils.showf( context,
                                                                       "%d SMS msgs resent;"
                                                                       + " backoff: %d",
                                                                       nSent, backoff);
                                                   }
                                               }
                                           } );

                }

                @Override
                public long incrementBackoff( long prevBackoff )
                {
                    Assert.failDbg();
                    return 0;
                }
            };

    static void resetTimer( Context context )
    {
        DBUtils.setIntFor( context, BACKOFF_KEY, MIN_BACKOFF_SECONDS );
        setTimer( context );
    }

    private static int setTimer( Context context )
    {
        return setTimer( context, false );
    }
    
    private static int setTimer( Context context, boolean advance )
    {
        int backoff = DBUtils.getIntFor( context, BACKOFF_KEY, MIN_BACKOFF_SECONDS );
        if ( advance ) {
            backoff = Math.min( MAX_BACKOFF_SECONDS, backoff * 2 );
            DBUtils.setIntFor( context, BACKOFF_KEY, backoff );
        }

        long millis = 1000L * backoff;
        TimerReceiver.setTimerRelative( context, sTimerCallbacks, millis );
        return backoff;
    }
}

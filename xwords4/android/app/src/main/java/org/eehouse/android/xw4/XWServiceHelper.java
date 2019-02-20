/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2010 - 2018 by Eric House (xwords@eehouse.org).  All
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

import android.app.Service;
import android.content.Context;
import android.content.Intent;

import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.jni.UtilCtxtImpl;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;

import java.util.HashMap;
import java.util.Map;

abstract class XWServiceHelper {
    private static final String TAG = XWServiceHelper.class.getSimpleName();  
    private Service mService;
    private static MultiService s_srcMgr = new MultiService();

    public static enum ReceiveResult { OK, GAME_GONE, UNCONSUMED };

    XWServiceHelper( Service service )
    {
        mService = service;
    }

    abstract MultiMsgSink getSink( long rowid );
    abstract void postNotification( String device, int gameID, long rowid );

    protected ReceiveResult receiveMessage( Context context, int gameID,
                                            MultiMsgSink sink, byte[] msg,
                                            CommsAddrRec addr )
    {
        ReceiveResult result;
        long[] rowids = DBUtils.getRowIDsFor( context, gameID );
        if ( null == rowids || 0 == rowids.length ) {
            result = ReceiveResult.GAME_GONE;
        } else {
            result = ReceiveResult.UNCONSUMED;
            for ( long rowid : rowids ) {
                if ( receiveMessage( context, rowid, sink, msg, addr ) ) {
                    result = ReceiveResult.OK;
                }
            }
        }
        return result;
    }

    protected boolean receiveMessage( Context context, long rowid,
                                      MultiMsgSink sink, byte[] msg,
                                      CommsAddrRec addr )
    {
        boolean allConsumed = true;
        boolean[] isLocalP = new boolean[1];
        boolean consumed = false;

        try ( JNIThread jniThread = JNIThread.getRetained( rowid ) ) {
            if ( null != jniThread ) {
                jniThread.receive( msg, addr );
                consumed = true;
            } else {
                GameUtils.BackMoveResult bmr = new GameUtils.BackMoveResult();
                if ( null == sink ) {
                    sink = getSink( rowid );
                }
                if ( GameUtils.feedMessage( context, rowid, msg, addr,
                                            sink, bmr, isLocalP ) ) {
                    GameUtils.postMoveNotification( context, rowid, bmr,
                                                    isLocalP[0] );
                    consumed = true;
                }
            }
        }
        if ( allConsumed && !consumed ) {
            allConsumed = false;
        }
        return allConsumed;
    }

    public final static void setListener( MultiService.MultiEventListener li )
    {
        s_srcMgr.setListener( li );
    }

    public final static void clearListener( MultiService.MultiEventListener li )
    {
        s_srcMgr.clearListener( li );
    }

    protected void postEvent( MultiEvent event, Object ... args )
    {
        if ( 0 == s_srcMgr.postEvent( event, args ) ) {
            Log.d( TAG, "postEvent(): dropping %s event",
                   event.toString() );
        }
    }

    protected boolean handleInvitation( Context context, NetLaunchInfo nli,
                                        String device, DictFetchOwner dfo )
    {
        boolean success = nli.isValid() && checkNotInFlight( nli );
        if ( success ) {
            long[] rowids = DBUtils.getRowIDsFor( mService, nli.gameID() );
            if ( 0 == rowids.length ) {
                // cool: we're good
            } else if ( rowids.length < nli.nPlayersT ) {
                success = XWPrefs.getSecondInviteAllowed( mService );

                // Allowing a second game allows the common testing action of
                // sending invitation to myself. But we still need to check
                // for duplicates! forceChannel's hard to dig up, but works
                for ( int ii = 0; success && ii < rowids.length; ++ii ) {
                    long rowid = rowids[ii];
                    CurGameInfo gi = null;
                    try ( GameLock lock = GameLock.tryLockRO( rowid ) ) {
                        // drop invite if can't open game; likely a dupe!
                        if ( null != lock ) {
                            gi = new CurGameInfo( mService );
                            GamePtr gamePtr = GameUtils
                                .loadMakeGame( mService, gi, lock );
                            gamePtr.release();
                        } else {
                            DbgUtils.toastNoLock( TAG, context, rowid,
                                                  "handleInvitation()" );
                        }
                    }

                    if ( null == gi ) {
                        // locked. Maybe it's open?
                        try ( JNIThread thrd = JNIThread.getRetained( rowid ) ) {
                            if ( null != thrd ) {
                                gi = thrd.getGI();
                            }
                        }
                    }
                    success = null != gi && gi.forceChannel != nli.forceChannel;
                }
            } else {
                success = false;
            }

            if ( success ) {
                if ( DictLangCache.haveDict( mService, nli.lang, nli.dict ) ) {
                    long rowid = GameUtils.makeNewMultiGame( mService, nli,
                                                             getSink( 0 ),
                                                             getUtilCtxt() );

                    if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                        DBUtils.setName( mService, rowid, nli.gameName );
                    }

                    postNotification( device, nli.gameID(), rowid );
                } else {
                    Intent intent = MultiService
                        .makeMissingDictIntent( mService, nli, dfo );
                    MultiService.postMissingDictNotification( mService, intent,
                                                              nli.gameID() );
                }
            }
        }
        Log.d( TAG, "handleInvitation() => %b", success );
        return success;
    }

    private UtilCtxt m_utilCtxt;
    protected UtilCtxt getUtilCtxt()
    {
        if ( null == m_utilCtxt ) {
            m_utilCtxt = new UtilCtxtImpl( mService );
        }
        return m_utilCtxt;
    }
    
    // Check that we aren't already processing an invitation with this
    // inviteID.
    private static final long SEEN_INTERVAL_MS = 1000 * 2;
    private static Map<String, Long> s_seen = new HashMap<>();
    private boolean checkNotInFlight( NetLaunchInfo nli )
    {
        boolean inProcess;
        String inviteID = nli.inviteID();
        synchronized( s_seen ) {
            long now = System.currentTimeMillis();
            Long lastSeen = s_seen.get( inviteID );
            inProcess = null != lastSeen && lastSeen + SEEN_INTERVAL_MS > now;
            if ( !inProcess ) {
                s_seen.put( inviteID, now );
            }
        }
        Log.d( TAG, "checkNotInFlight('%s') => %b", inviteID, !inProcess );
        return !inProcess;
    }
}

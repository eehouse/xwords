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
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.jni.UtilCtxtImpl;

import java.util.HashMap;
import java.util.Map;


abstract class XWServiceHelper {
    private static final String TAG = XWServiceHelper.class.getSimpleName();  
    private Service mService;
    private static MultiService s_srcMgr = null;

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
        JNIThread jniThread = JNIThread.getRetained( rowid, false );
        boolean consumed = false;
        if ( null != jniThread ) {
            consumed = true;
            jniThread.receive( msg, addr ).release();
        } else {
            GameUtils.BackMoveResult bmr = new GameUtils.BackMoveResult();
            if ( null == sink ) {
                sink = getSink( rowid );
            }
            if ( GameUtils.feedMessage( context, rowid, msg, addr,
                                        sink, bmr, isLocalP ) ) {
                consumed = true;
                GameUtils.postMoveNotification( context, rowid, bmr,
                                                isLocalP[0] );
            }
        }
        if ( allConsumed && !consumed ) {
            allConsumed = false;
        }
        return allConsumed;
    }

    public final static void setListener( MultiService.MultiEventListener li )
    {
        if ( null == s_srcMgr ) {
            // DbgUtils.logf( "XWService.setListener: registering %s",
            // li.getClass().getName() );
            s_srcMgr = new MultiService();
        }
        s_srcMgr.setListener( li );
    }

    protected void postEvent( MultiEvent event, Object ... args )
    {
        if ( null != s_srcMgr ) {
            s_srcMgr.postEvent( event, args );
        } else {
            Log.d( TAG, "postEvent(): dropping %s event",
                   event.toString() );
        }
    }

    protected boolean handleInvitation( NetLaunchInfo nli, String device,
                                        DictFetchOwner dfo )
    {
        boolean success = false;
        long[] rowids = DBUtils.getRowIDsFor( mService, nli.gameID() );
        if ( 0 == rowids.length
             || ( rowids.length < nli.nPlayersT // will break for two-per-device game
                  && XWPrefs.getSecondInviteAllowed( mService ) ) ) {

            if ( nli.isValid() && checkNotDupe( nli ) ) {

                if ( DictLangCache.haveDict( mService, nli.lang, nli.dict ) ) {
                    long rowid = GameUtils.makeNewMultiGame( mService, nli,
                                                             getSink( 0 ),
                                                             getUtilCtxt() );

                    if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                        DBUtils.setName( mService, rowid, nli.gameName );
                    }

                    postNotification( device, nli.gameID(), rowid );
                    success = true;
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
    private static final long SEEN_INTERVAL_MS = 1000 * 5;
    private static Map<String, Long> s_seen = new HashMap<>();
    private boolean checkNotDupe( NetLaunchInfo nli )
    {
        String inviteID = nli.inviteID();
        synchronized( s_seen ) {
            long now = System.currentTimeMillis();
            Long lastSeen = s_seen.get( inviteID );
            boolean seen = null != lastSeen && lastSeen + SEEN_INTERVAL_MS > now;
            if ( !seen ) {
                s_seen.put( inviteID, now );
            }
            Log.d( TAG, "checkNotDupe('%s') => %b", inviteID, !seen );
            return !seen;
        }
    }
}

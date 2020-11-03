/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import org.eehouse.android.xw4.loc.LocUtils;

import java.util.HashMap;
import java.util.Map;

abstract class XWServiceHelper {
    private static final String TAG = XWServiceHelper.class.getSimpleName();  
    private Context mContext;
    private static MultiService s_srcMgr = new MultiService();

    public static enum ReceiveResult { OK, GAME_GONE, UNCONSUMED };

    XWServiceHelper( Context context )
    {
        mContext = context;
    }

    Context getContext() { return mContext; }

    MultiMsgSink getSink( long rowid )
    {
        return new MultiMsgSink( getContext(), rowid );
    }

    void postNotification( String device, int gameID, long rowid )
    {
        Context context = getContext();
        String body = LocUtils.getString( context, R.string.new_game_body );
        GameUtils.postInvitedNotification( context, gameID, body, rowid );
    }

    protected ReceiveResult receiveMessage( int gameID,
                                            MultiMsgSink sink, byte[] msg,
                                            CommsAddrRec addr )
    {
        ReceiveResult result;
        long[] rowids = DBUtils.getRowIDsFor( mContext, gameID );
        if ( 0 == rowids.length ) {
            result = ReceiveResult.GAME_GONE;
        } else {
            result = ReceiveResult.UNCONSUMED;
            for ( long rowid : rowids ) {
                if ( receiveMessage( rowid, sink, msg, addr ) ) {
                    result = ReceiveResult.OK;
                }
            }
        }
        return result;
    }

    protected boolean receiveMessage( long rowid, MultiMsgSink sink,
                                      byte[] msg, CommsAddrRec addr )
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
                if ( GameUtils.feedMessage( mContext, rowid, msg, addr,
                                            sink, bmr, isLocalP ) ) {
                    GameUtils.postMoveNotification( mContext, rowid, bmr,
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

    protected boolean handleInvitation( NetLaunchInfo nli,
                                        String device, DictFetchOwner dfo )
    {
        boolean success = false;
        if ( !nli.isValid() ) {
            Log.d( TAG, "invalid nli" );
        } else if ( ! checkNotInFlight( nli ) ) {
            Log.e( TAG, "checkNotInFlight() => false" );
        } else {
            success = true;
        }

        if ( success ) {
            Map<Long, Integer> rowids = DBUtils.getRowIDsAndChannels( mContext, nli.gameID() );
            // Accept only if there isn't already a game with the channel
            for ( long rowid : rowids.keySet() ) {
                if ( rowids.get( rowid ) == nli.forceChannel ) {
                    if ( BuildConfig.DEBUG ) {
                        DbgUtils.showf( mContext, "Dropping duplicate invite" );
                    }
                    success = false;
                    break;
                }
            }

            if ( success ) {
                if ( DictLangCache.haveDict( mContext, nli.lang, nli.dict ) ) {
                    long rowid = GameUtils.makeNewMultiGame( mContext, nli,
                                                             getSink( 0 ),
                                                             getUtilCtxt() );

                    if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                        DBUtils.setName( mContext, rowid, nli.gameName );
                    }

                    postNotification( device, nli.gameID(), rowid );
                } else {
                    Intent intent = MultiService
                        .makeMissingDictIntent( mContext, nli, dfo );
                    MultiService.postMissingDictNotification( mContext, intent,
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
            m_utilCtxt = new UtilCtxtImpl( mContext );
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

/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
/*
 * Copyright 2010 - 2012 by Eric House (xwords@eehouse.org).  All
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
import android.os.IBinder;

import junit.framework.Assert;

import org.eehouse.android.xw4.MultiService.DictFetchOwner;
import org.eehouse.android.xw4.MultiService.MultiEvent;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.jni.UtilCtxtImpl;

import java.util.HashSet;
import java.util.Set;

abstract class XWService extends Service {
    private static final String TAG = XWService.class.getSimpleName();
    public static enum ReceiveResult { OK, GAME_GONE, UNCONSUMED };

    protected static MultiService s_srcMgr = null;
    private static Set<String> s_seen = new HashSet<String>();

    private UtilCtxt m_utilCtxt;

    @Override
    public IBinder onBind( Intent intent )
    {
        return null;
    }

    public final static void setListener( MultiService.MultiEventListener li )
    {
        if ( null == s_srcMgr ) {
            // DbgUtils.logf( "XWService.setListener: registering %s", li.getClass().getName() );
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

    // Check that we aren't already processing an invitation with this
    // inviteID.
    private boolean checkNotDupe( NetLaunchInfo nli )
    {
        String inviteID = nli.inviteID();
        boolean isDupe;
        synchronized( s_seen ) {
            isDupe = s_seen.contains( inviteID );
            if ( !isDupe ) {
                s_seen.add( inviteID );
            }
        }
        Log.d( TAG, "checkNotDupe('%s') => %b", inviteID, !isDupe );
        return !isDupe;
    }

    abstract void postNotification( String device, int gameID, long rowid );

    // Return true if able to start game only
    protected boolean handleInvitation( NetLaunchInfo nli, String device,
                                        DictFetchOwner dfo )
    {
        boolean success = false;
        long[] rowids = DBUtils.getRowIDsFor( this, nli.gameID() );
        if ( 0 == rowids.length
             || ( rowids.length < nli.nPlayersT // will break for two-per-device game
                  && XWPrefs.getSecondInviteAllowed( this ) ) ) {

            if ( nli.isValid() && checkNotDupe( nli ) ) {

                if ( DictLangCache.haveDict( this, nli.lang, nli.dict ) ) {
                    long rowid = GameUtils.makeNewMultiGame( this, nli,
                                                             getSink( 0 ),
                                                             getUtilCtxt() );

                    if ( null != nli.gameName && 0 < nli.gameName.length() ) {
                        DBUtils.setName( this, rowid, nli.gameName );
                    }

                    postNotification( device, nli.gameID(), rowid );
                    success = true;
                } else {
                    Intent intent = MultiService
                        .makeMissingDictIntent( this, nli, dfo );
                    MultiService.postMissingDictNotification( this, intent,
                                                              nli.gameID() );
                }
            }
        }
        Log.d( TAG, "handleInvitation() => %b", success );
        return success;
    }

    protected UtilCtxt getUtilCtxt()
    {
        if ( null == m_utilCtxt ) {
            m_utilCtxt = new UtilCtxtImpl( this );
        }
        return m_utilCtxt;
    }

    // Meant to be overridden
    abstract MultiMsgSink getSink( long rowid );

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
}

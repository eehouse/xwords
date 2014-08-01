/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2014 by Eric House (xwords@eehouse.org).  All
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
import android.net.Uri;
import android.os.Bundle;

import org.eehouse.android.xw4.jni.CurGameInfo;

import junit.framework.Assert;

public class GamesListActivity extends XWListActivity implements GamesListDelegator {

    // private static final String RELAYIDS_EXTRA = "relayids";
    private static final String ROWID_EXTRA = "rowid";
    private static final String GAMEID_EXTRA = "gameid";
    private static final String REMATCH_ROWID_EXTRA = "rowid_rm";
    private static final String ALERT_MSG = "alert_msg";

    private GamesListDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        m_dlgt = new GamesListDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );
    } // onCreate

    // called when we're brought to the front (probably as a result of
    // notification)
    @Override
    protected void onNewIntent( Intent intent )
    {
        super.onNewIntent( intent );
        m_dlgt.onNewIntent( intent );
    }

    //////////////////////////////////////////////////////////////////////
    // GamesListDelegator interface
    //////////////////////////////////////////////////////////////////////
    public void launchGame( long rowID, boolean invited )
    {
        GameUtils.launchGame( this, rowID, invited );
    }

    public static void onGameDictDownload( Context context, Intent intent )
    {
        intent.setClass( context, GamesListActivity.class );
        context.startActivity( intent );
    }

    private static Intent makeSelfIntent( Context context )
    {
        Intent intent = new Intent( context, GamesListActivity.class );
        intent.setFlags( Intent.FLAG_ACTIVITY_CLEAR_TOP
                         | Intent.FLAG_ACTIVITY_NEW_TASK );
        return intent;
    }

    public static Intent makeRowidIntent( Context context, long rowid )
    {
        Intent intent = makeSelfIntent( context );
        intent.putExtra( ROWID_EXTRA, rowid );
        return intent;
    }

    public static Intent makeGameIDIntent( Context context, int gameID )
    {
        Intent intent = makeSelfIntent( context );
        intent.putExtra( GAMEID_EXTRA, gameID );
        return intent;
    }

    public static Intent makeRematchIntent( Context context, CurGameInfo gi,
                                            long rowid )
    {
        Intent intent = null;
        
        if ( CurGameInfo.DeviceRole.SERVER_STANDALONE == gi.serverRole ) {
            intent = makeSelfIntent( context )
                .putExtra( REMATCH_ROWID_EXTRA, rowid );
        } else {
            Utils.notImpl( context );
        }

        return intent;
    }

    public static Intent makeAlertIntent( Context context, String msg )
    {
        Intent intent = makeSelfIntent( context );
        intent.putExtra( ALERT_MSG, msg );
        return intent;
    }

    public static void openGame( Context context, Uri data )
    {
        Intent intent = makeSelfIntent( context );
        intent.setData( data );
        context.startActivity( intent );
    }
}

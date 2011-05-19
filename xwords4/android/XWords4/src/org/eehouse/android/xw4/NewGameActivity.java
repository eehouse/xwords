/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import java.util.Random;

// import junit.framework.Assert;

import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.XwJNI;


public class NewGameActivity extends XWActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.new_game );

        Button button = (Button)findViewById( R.id.newgame_local );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    newAndLaunch();
                }
            } );
        button = (Button)findViewById( R.id.newgame_local_config );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    newAndConfigure( false );
                }
            } );

        button = (Button)findViewById( R.id.newgame_invite );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    newNetworkedAndLaunch();
                }
            } );

        button = (Button)findViewById( R.id.newgame_net_config );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    newAndConfigure( true );
                }
            } );

    }

    private void newAndLaunch()
    {
        String path = GameUtils.saveNew( this, new CurGameInfo( this ) );
        GameUtils.launchGame( this, path );
        finish();
    }

    // So: Query the user for the desired room name and player name,
    // providing defaults.
    private void newNetworkedAndLaunch()
    {
        Random random = new Random();
        String room = String.format( "%X", random.nextInt() );
        int[] lang = {0};
        String path = GameUtils.makeNewNetGame( this, room, lang, 2 );

        GameUtils.launchGame( this, path );

        // Remove this for now at least.  The game itself will suggest
        // an invite when it connects and is missing someone.
        // GameUtils.launchInviteActivity( this, room, lang[0] );

        finish();
    }

    private void newAndConfigure( boolean networked )
    {
        String path = GameUtils.saveNew( this, new CurGameInfo( this, 
                                                                networked ) );
        GameUtils.doConfig( this, path, GameConfig.class );
        finish();
    }

}
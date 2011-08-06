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
import android.widget.TextView;
import java.util.Random;

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

        TextView desc = (TextView)findViewById( R.id.newgame_local_desc );
        String fmt = getString( R.string.newgame_local_descf );
        String dict = CommonPrefs.getDefaultHumanDict( this );
        String lang = DictLangCache.getLangName( this, dict );
        desc.setText( String.format( fmt, lang ) );
        
        Button button = (Button)findViewById( R.id.newgame_local );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( false, true );
                }
            } );
        button = (Button)findViewById( R.id.newgame_local_config );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( false, false );
                }
            } );

        button = (Button)findViewById( R.id.newgame_invite );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( true, true );
                }
            } );

        button = (Button)findViewById( R.id.newgame_net_config );
        button.setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    makeNewGame( true, false );
                }
            } );

    }

    private void makeNewGame( boolean networked, boolean launch )
    {
        if ( launch && networked ) {
            // Let 'em cancel before we make the game
            showTextOrHtmlThen( new DlgDelegate.TextOrHtmlClicked() {
                    public void clicked( boolean choseText ) {
                        makeNewGame( true, true, choseText );
                    }
                } );
        } else {
            makeNewGame( networked, launch, false );
        }
    }

    private void makeNewGame( boolean networked, boolean launch,
                              boolean choseText )
    {
        String room = null;
        long rowid;
        int[] lang = {0};
        final int nPlayers = 2; // hard-coded for no-configure case

        if ( networked ) {
            Random random = new Random();
            room = String.format( "%X", random.nextInt() ).substring( 0, 4 );
            rowid = GameUtils.makeNewNetGame( this, room, lang, nPlayers, 1 );
        } else {
            rowid = GameUtils.saveNew( this, new CurGameInfo( this ) );
        }

        if ( launch ) {
            GameUtils.launchGame( this, rowid, networked );
            if ( networked ) {
                GameUtils.launchInviteActivity( this, choseText, room,
                                                lang[0], nPlayers );
            }
        } else {
            GameUtils.doConfig( this, rowid, GameConfig.class );
        }

        finish();
    }

}

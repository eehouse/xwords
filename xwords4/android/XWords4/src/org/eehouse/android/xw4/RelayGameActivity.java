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

// This activity is for newbies.  Bring it up when network game
// created.  It explains they need only a room name -- that everything
// else is derived from defaults and configurable via the main config
// dialog (which offer to launch)

package org.eehouse.android.xw4;

import android.app.Activity;
import java.io.File;
import android.os.Bundle;
import android.net.Uri;
import android.widget.Button;
import android.widget.TextView;
import android.view.View;
import android.content.Intent;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class RelayGameActivity extends XWActivity 
    implements View.OnClickListener {

    private String m_path;
    private CurGameInfo m_gi;
    private CommsAddrRec m_car;
    private Button m_playButton;
    private Button m_configButton;

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.relay_game_config );

        Uri uri = getIntent().getData();
        m_path = uri.getPath();
        if ( m_path.charAt(0) == '/' ) {
            m_path = m_path.substring( 1 );
        }

        int gamePtr = XwJNI.initJNI();
        m_gi = new CurGameInfo( this );
        GameUtils.loadMakeGame( this, gamePtr, m_gi, m_path );
        m_car = new CommsAddrRec( this );
        if ( XwJNI.game_hasComms( gamePtr ) ) {
            XwJNI.comms_getAddr( gamePtr, m_car );
        } else {
            Assert.fail();
            // String relayName = CommonPrefs.getDefaultRelayHost( this );
            // int relayPort = CommonPrefs.getDefaultRelayPort( this );
            // XwJNI.comms_getInitialAddr( m_carOrig, relayName, relayPort );
        }
        XwJNI.game_dispose( gamePtr );

        String lang = DictLangCache.getLangName( this, m_gi.dictName );
        String fmt = getString( R.string.relay_game_explainf );
        TextView text = (TextView)findViewById( R.id.explain );
        text.setText( String.format( fmt, lang ) );

        m_playButton = (Button)findViewById( R.id.play_button );
        m_playButton.setOnClickListener( this );

        m_configButton = (Button)findViewById( R.id.config_button );
        m_configButton.setOnClickListener( this );
    } // onCreate

    @Override
    public void onClick( View view ) 
    {
        if ( view == m_playButton ) {
            String room = Utils.getText( this, R.id.room_edit ).trim();
            if ( room.length() == 0 ) {
                showOKOnlyDialog( R.string.no_empty_rooms );
            } else {
                m_car.ip_relay_invite = room;
                String name = Utils.getText( this, R.id.local_name_edit );
                if ( name.length() > 0 ) {
                    m_gi.setFirstLocalName( name );
                }
                GameUtils.applyChanges( this, m_gi, m_car, m_path, false );
                GameUtils.launchGame( this, m_path );
            }
        } else if ( view == m_configButton ) {
            GameUtils.doConfig( this, m_path, GameConfig.class );
            finish();
        }
    }

    public static boolean isSimpleGame( GameSummary summary )
    {
        return summary.nPlayers == 2;
    }

} // class RelayGameActivity

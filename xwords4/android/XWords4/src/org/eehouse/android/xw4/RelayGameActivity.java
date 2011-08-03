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
import android.widget.Button;
import android.widget.TextView;
import android.view.View;
import android.content.Intent;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class RelayGameActivity extends XWActivity 
    implements View.OnClickListener {

    private long m_rowid;
    private CurGameInfo m_gi;
    private GameUtils.GameLock m_gameLock;
    private CommsAddrRec m_car;
    private Button m_playButton;
    private Button m_configButton;

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.relay_game_config );

        m_rowid = getIntent().getLongExtra( BoardActivity.INTENT_KEY_ROWID, -1 );

        m_playButton = (Button)findViewById( R.id.play_button );
        m_playButton.setOnClickListener( this );

        m_configButton = (Button)findViewById( R.id.config_button );
        m_configButton.setOnClickListener( this );
    } // onCreate

    @Override
    protected void onStart()
    {
        super.onStart();

        int gamePtr = XwJNI.initJNI();
        m_gi = new CurGameInfo( this );
        m_gameLock = new GameUtils.GameLock( m_rowid, true ).lock();
        GameUtils.loadMakeGame( this, gamePtr, m_gi, m_gameLock );
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

        String lang = DictLangCache.getLangName( this, m_gi.dictLang );
        String fmt = getString( R.string.relay_game_explainf );
        TextView text = (TextView)findViewById( R.id.explain );
        text.setText( String.format( fmt, lang ) );
    }

    @Override
    protected void onPause()
    {
        if ( null != m_gameLock ) {
            m_gameLock.unlock();
            m_gameLock = null;
        }
        super.onPause();
    }

    @Override
    public void onClick( View view ) 
    {
        String room = Utils.getText( this, R.id.room_edit ).trim();
        if ( view == m_playButton ) {
            if ( room.length() == 0 ) {
                showOKOnlyDialog( R.string.no_empty_rooms );
            } else {
                if ( saveRoomAndName( room ) ) {
                    GameUtils.launchGameAndFinish( this, m_rowid );
                }
            }
        } else if ( view == m_configButton ) {
            if ( saveRoomAndName( room ) ) {
                GameUtils.doConfig( this, m_rowid, GameConfig.class );
                finish();
            }
        }
    }

    public static boolean isSimpleGame( GameSummary summary )
    {
        return summary.nPlayers == 2;
    }

    private boolean saveRoomAndName( String room )
    {
        boolean canSave = null != m_gameLock;
        if ( canSave ) {
            String name = Utils.getText( this, R.id.local_name_edit );
            if ( name.length() > 0 ) { // don't wipe existing
                m_gi.setFirstLocalName( name );
            }
            m_car.ip_relay_invite = room;
            GameUtils.applyChanges( this, m_gi, m_car, m_gameLock, false );
            m_gameLock.unlock();
            m_gameLock = null;
        }
        return canSave;
    }

} // class RelayGameActivity

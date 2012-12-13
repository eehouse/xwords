/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
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
import android.os.AsyncTask;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.text.DateFormat;
import java.util.Date;

import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class GameListItem extends LinearLayout 
    implements View.OnClickListener {

    private Context m_context;
    private boolean m_loaded;
    private long m_rowid;
    private View m_hideable;
    private ExpiringTextView m_name;
    private boolean m_expanded, m_haveTurn, m_haveTurnLocal;
    private long m_lastMoveTime;
    private ImageButton m_expandButton;
    private Handler m_handler;
    private GameSummary m_summary;
    private GameListAdapter.LoadItemCB m_cb;
    private int m_fieldID;

    public GameListItem( Context cx, AttributeSet as ) 
    {
        super( cx, as );
        m_context = cx;
        m_loaded = false;
        m_rowid = DBUtils.ROWID_NOTFOUND;
        m_lastMoveTime = 0;
    }

    public void init( Handler handler, long rowid, int fieldID,
                      GameListAdapter.LoadItemCB cb )
    {
        m_handler = handler;
        m_rowid = rowid;
        m_fieldID = fieldID;
        m_cb = cb;

        forceReload();
    }

    public void forceReload()
    {
        m_summary = null;
        new LoadItemTask().execute();
    }

    public void invalName()
    {
        setName();
    }

    private void update( boolean expanded, long lastMoveTime, boolean haveTurn,
                         boolean haveTurnLocal )
    {
        m_expanded = expanded;
        m_lastMoveTime = lastMoveTime;
        m_haveTurn = haveTurn;
        m_haveTurnLocal = haveTurnLocal;
        m_hideable = (LinearLayout)findViewById( R.id.hideable );
        m_name = (ExpiringTextView)findViewById( R.id.game_name );
        m_expandButton = (ImageButton)findViewById( R.id.expander );
        m_expandButton.setOnClickListener( this );
        showHide();
    }

    public long getRowID()
    {
        return m_rowid;
    }

    // View.OnClickListener interface
    public void onClick( View view ) {
        m_expanded = !m_expanded;
        DBUtils.setExpanded( m_rowid, m_expanded );
        showHide();
    }

    private void setLoaded()
    {
        if ( !m_loaded ) {
            m_loaded = true;
            // This should be enough to invalidate
            findViewById( R.id.view_unloaded )
                .setVisibility( m_loaded ? View.GONE : View.VISIBLE );
            findViewById( R.id.view_loaded )
                .setVisibility( m_loaded ? View.VISIBLE : View.GONE );
        }
    }

    private void showHide()
    {
        m_expandButton.setImageResource( m_expanded ?
                                         R.drawable.expander_ic_maximized :
                                         R.drawable.expander_ic_minimized);
        m_hideable.setVisibility( m_expanded? View.VISIBLE : View.GONE );

        m_name.setBackgroundColor( android.R.color.transparent );
        m_name.setPct( m_handler, m_haveTurn && !m_expanded, 
                       m_haveTurnLocal, m_lastMoveTime );
    }

    private String setName()
    {
        String state = null;    // hack to avoid calling summarizeState twice
        if ( null != m_summary ) {
            state = m_summary.summarizeState();
            TextView view = (TextView)findViewById( R.id.game_name );
            String value = null;
            switch ( m_fieldID ) {
            case R.string.game_summary_field_empty:
                break;
            case R.string.game_summary_field_language:
                value = 
                    DictLangCache.getLangName( m_context, 
                                               m_summary.dictLang );
                break;
            case R.string.game_summary_field_opponents:
                value = m_summary.playerNames();
                break;
            case R.string.game_summary_field_state:
                value = state;
                break;
            }

            if ( null != value ) {
                String name = GameUtils.getName( m_context, m_rowid );
                value = m_context.getString( R.string.str_game_namef, 
                                             name, value );
            } else {
                value = GameUtils.getName( m_context, m_rowid );
            }
                        
            view.setText( value );
        }
        return state;
    }

    private void setData()
    {
        if ( null != m_summary ) {
            TextView view;
            String state = setName();

            setOnClickListener( new View.OnClickListener() {
                    @Override
                    public void onClick( View v ) {
                        m_cb.itemClicked( m_rowid, m_summary );
                    }
                } );

            LinearLayout list =
                (LinearLayout)findViewById( R.id.player_list );
            list.removeAllViews();
            boolean haveATurn = false;
            boolean haveALocalTurn = false;
            boolean[] isLocal = new boolean[1];
            for ( int ii = 0; ii < m_summary.nPlayers; ++ii ) {
                ExpiringLinearLayout tmp = (ExpiringLinearLayout)
                    Utils.inflate( m_context, R.layout.player_list_elem );
                view = (TextView)tmp.findViewById( R.id.item_name );
                view.setText( m_summary.summarizePlayer( ii ) );
                view = (TextView)tmp.findViewById( R.id.item_score );
                view.setText( String.format( "  %d", m_summary.scores[ii] ) );
                boolean thisHasTurn = m_summary.isNextToPlay( ii, isLocal );
                if ( thisHasTurn ) {
                    haveATurn = true;
                    if ( isLocal[0] ) {
                        haveALocalTurn = true;
                    }
                }
                tmp.setPct( m_handler, thisHasTurn, isLocal[0], 
                            m_summary.lastMoveTime );
                list.addView( tmp, ii );
            }

            view = (TextView)findViewById( R.id.state );
            view.setText( state );
            view = (TextView)findViewById( R.id.modtime );
            long lastMoveTime = m_summary.lastMoveTime;
            lastMoveTime *= 1000;

            DateFormat df = DateFormat.getDateTimeInstance( DateFormat.SHORT, 
                                                            DateFormat.SHORT );
            view.setText( df.format( new Date( lastMoveTime ) ) );

            int iconID;
            ImageView marker =
                (ImageView)findViewById( R.id.msg_marker );
            CommsConnType conType = m_summary.conType;
            if ( CommsConnType.COMMS_CONN_RELAY == conType ) {
                iconID = R.drawable.relaygame;
            } else if ( CommsConnType.COMMS_CONN_BT == conType ) {
                iconID = android.R.drawable.stat_sys_data_bluetooth;
            } else if ( CommsConnType.COMMS_CONN_SMS == conType ) {
                iconID = android.R.drawable.sym_action_chat;
            } else {
                iconID = R.drawable.sologame;
            }
            marker.setImageResource( iconID );

            view = (TextView)findViewById( R.id.role );
            String roleSummary = m_summary.summarizeRole();
            if ( null != roleSummary ) {
                view.setText( roleSummary );
            } else {
                view.setVisibility( View.GONE );
            }

            boolean expanded = DBUtils.getExpanded( m_context, m_rowid );

            update( expanded, m_summary.lastMoveTime, haveATurn, 
                    haveALocalTurn );
        }
    }

    private class LoadItemTask extends AsyncTask<Void, Void, GameSummary> {
        @Override
        protected GameSummary doInBackground( Void... unused ) 
        {
            return DBUtils.getSummary( m_context, m_rowid, 150 );
        } // doInBackground

        @Override
        protected void onPostExecute( GameSummary summary )
        {
            m_summary = summary;
            setData();
            // setLoaded( m_view.getRowID() );
            setLoaded();

            // DbgUtils.logf( "LoadItemTask for row %d finished", m_rowid );
        }
    } // class LoadItemTask

}

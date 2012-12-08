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
import android.database.DataSetObserver;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.TextView;
import java.io.FileInputStream;
import java.text.DateFormat;
import java.util.Date;
import java.util.HashSet;
import java.util.Random;

import junit.framework.Assert;


import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;

public class GameListAdapter extends XWListAdapter {
    private static final boolean s_isFire;
    private static Random s_random;
    static {
        s_isFire = Build.MANUFACTURER.equals( "Amazon" );
        if ( s_isFire ) {
            s_random = new Random();
        }
    }

    private Context m_context;
    private LayoutInflater m_factory;
    private int m_fieldID;
    private Handler m_handler;
    private DateFormat m_df;
    private LoadItemCB m_cb;
    // Track those rows known to be good.  If a rowid is not in this
    // set, assume it must be loaded.  Add rowids to this set as
    // they're loaded, and remove one when when it must be redrawn.
    private HashSet<Long> m_loadedRows;

    public interface LoadItemCB {
        public void itemClicked( long rowid );
    }

    private class LoadItemTask extends AsyncTask<Void, Void, GameSummary> {
        private GameListItem m_view;
        private Context m_context;
        // private int m_id;
        public LoadItemTask( Context context, GameListItem view )
        {
            DbgUtils.logf( "Creating LoadItemTask for row %d", 
                           view.getRowID() );
            m_context = context;
            m_view = view;
        }

        @Override
        protected GameSummary doInBackground( Void... unused ) 
        {
            // Without this, on the Fire only the last item in the
            // list it tappable.  Likely my fault, but this seems to
            // work around it.
            if ( s_isFire ) {
                try {
                    int sleepTime = 500 + (s_random.nextInt() % 500);
                    Thread.sleep( sleepTime );
                } catch ( Exception e ) {
                }
            }

            long rowid = m_view.getRowID();
            GameSummary summary = DBUtils.getSummary( m_context, rowid, 1500 );
            return summary;
        } // doInBackground

        @Override
        protected void onPostExecute( GameSummary summary )
        {
            setData( m_view, summary );
            setLoaded( m_view.getRowID() );
            m_view.setLoaded( true );

            DbgUtils.logf( "LoadItemTask for row %d finished", 
                           m_view.getRowID() );
        }
    } // class LoadItemTask

    public GameListAdapter( Context context, Handler handler, LoadItemCB cb,
                            String fieldName ) {
        super( DBUtils.gamesList(context).length );
        m_context = context;
        m_handler = handler;
        m_cb = cb;
        m_factory = LayoutInflater.from( context );
        m_df = DateFormat.getDateTimeInstance( DateFormat.SHORT, 
                                               DateFormat.SHORT );

        m_loadedRows = new HashSet<Long>();
        m_fieldID = fieldToID( fieldName );
    }

    @Override
    public int getCount() {
        return DBUtils.gamesList(m_context).length;
    }

    // Views.  A view depends on a summary, which takes time to load.
    // When one needs loading it's done via an async task.
    public View getView( int position, View convertView, ViewGroup parent ) 
    {
        GameListItem result;
        boolean mustLoad = false;
        if ( null == convertView ) {
            result = (GameListItem)
                m_factory.inflate( R.layout.game_list_item, null );
            result.setRowID( DBUtils.gamesList(m_context)[position] );
            mustLoad = true;
        } else {
            result = (GameListItem)convertView;
            long rowid = result.getRowID();
            if ( isDirty(rowid) || !result.isLoaded() ) {
                mustLoad = true;
            }
        }

        if ( mustLoad ) {
            new LoadItemTask( m_context, result ).execute();
        }

        return result;
    }

    public void inval( long rowid )
    {
        synchronized( m_loadedRows ) {
            m_loadedRows.remove( rowid );
        }
    }

    private void dirtyAll()
    {
        synchronized( m_loadedRows ) {
            m_loadedRows.clear();
        }
    }

    private boolean isDirty( long rowid )
    {
        synchronized( m_loadedRows ) {
            return ! m_loadedRows.contains( rowid );
        }
    }

    private void setLoaded( long rowid )
    {
        synchronized( m_loadedRows ) {
            m_loadedRows.add( rowid );
        }
    }

    public boolean setField( String fieldName )
    {
        boolean changed = false;
        int newID = fieldToID( fieldName );
        if ( -1 == newID ) {
            if ( XWApp.DEBUG ) {
                DbgUtils.logf( "GameListAdapter.setField(): unable to match"
                               + " fieldName %s", fieldName );
            }
        } else if ( m_fieldID != newID ) {
            if ( XWApp.DEBUG ) {
                DbgUtils.logf( "setField: clearing views cache for change"
                               + " from %d to %d", m_fieldID, newID );
            }
            m_fieldID = newID;
            dirtyAll();
            changed = true;
        }
        return changed;
    }

    private int fieldToID( String fieldName )
    {
        int[] ids = {
            R.string.game_summary_field_empty
            ,R.string.game_summary_field_language
            ,R.string.game_summary_field_opponents
            ,R.string.game_summary_field_state
        };
        int result = -1;
        for ( int id : ids ) {
            if ( m_context.getString( id ).equals( fieldName ) ) {
                result = id;
                break;
            }
        }
        return result;
    }

    private void setData( GameListItem layout, GameSummary summary )
    {
        if ( null != summary ) {
            final long rowid = layout.getRowID();
            String state = summary.summarizeState();

            TextView view = (TextView)layout.findViewById( R.id.game_name );
            String value = null;
            switch ( m_fieldID ) {
            case R.string.game_summary_field_empty:
                break;
            case R.string.game_summary_field_language:
                value = 
                    DictLangCache.getLangName( m_context, 
                                               summary.dictLang );
                break;
            case R.string.game_summary_field_opponents:
                value = summary.playerNames();
                break;
            case R.string.game_summary_field_state:
                value = state;
                break;
            }

            String name = GameUtils.getName( m_context, rowid );

            if ( null != value ) {
                value = m_context.getString( R.string.str_game_namef, 
                                             name, value );
            } else {
                value = name;
            }
                        
            view.setText( value );

            layout.setOnClickListener( new View.OnClickListener() {
                    @Override
                        public void onClick( View v ) {
                        m_cb.itemClicked( rowid );
                    }
                } );

            LinearLayout list =
                (LinearLayout)layout.findViewById( R.id.player_list );
            boolean haveATurn = false;
            boolean haveALocalTurn = false;
            boolean[] isLocal = new boolean[1];
            for ( int ii = 0; ii < summary.nPlayers; ++ii ) {
                ExpiringLinearLayout tmp = (ExpiringLinearLayout)
                    m_factory.inflate( R.layout.player_list_elem, null );
                view = (TextView)tmp.findViewById( R.id.item_name );
                view.setText( summary.summarizePlayer( ii ) );
                view = (TextView)tmp.findViewById( R.id.item_score );
                view.setText( String.format( "  %d", summary.scores[ii] ) );
                boolean thisHasTurn = summary.isNextToPlay( ii, isLocal );
                if ( thisHasTurn ) {
                    haveATurn = true;
                    if ( isLocal[0] ) {
                        haveALocalTurn = true;
                    }
                }
                tmp.setPct( m_handler, thisHasTurn, isLocal[0], 
                            summary.lastMoveTime );
                list.addView( tmp, ii );
            }

            view = (TextView)layout.findViewById( R.id.state );
            view.setText( state );
            view = (TextView)layout.findViewById( R.id.modtime );
            long lastMoveTime = summary.lastMoveTime;
            lastMoveTime *= 1000;
            view.setText( m_df.format( new Date( lastMoveTime ) ) );

            int iconID;
            ImageView marker =
                (ImageView)layout.findViewById( R.id.msg_marker );
            CommsConnType conType = summary.conType;
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

            view = (TextView)layout.findViewById( R.id.role );
            String roleSummary = summary.summarizeRole();
            if ( null != roleSummary ) {
                view.setText( roleSummary );
            } else {
                view.setVisibility( View.GONE );
            }

            boolean expanded = DBUtils.getExpanded( m_context, rowid );

            layout.update( m_handler, expanded, summary.lastMoveTime, 
                           haveATurn, haveALocalTurn );
        }
    }
}
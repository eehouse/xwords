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

import android.widget.ListAdapter;
import android.content.Context;
import android.os.AsyncTask;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.database.DataSetObserver;
import java.io.FileInputStream;
import java.util.Date;
import java.util.HashMap;       // class is not synchronized
import java.text.DateFormat;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import junit.framework.Assert;


import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameListAdapter extends XWListAdapter {
    private Context m_context;
    private LayoutInflater m_factory;
    private HashMap<String,View> m_viewsCache;
    private DateFormat m_df;
    private LoadItemCB m_cb;
    // private int m_taskCounter = 0;

    public interface LoadItemCB {
        public void itemLoaded( String path );
    }

    private class LoadItemTask extends AsyncTask<Void, Void, Void> {
        private String m_path;
        private Context m_context;
        // private int m_id;
        public LoadItemTask( Context context, String path/*, int id*/ )
        {
            m_context = context;
            m_path = path;
            // m_id = id;
        }

        @Override
        protected Void doInBackground( Void... unused ) 
        {
            // Utils.logf( "doInBackground(id=%d)", m_id );
            View layout = m_factory.inflate( R.layout.game_list_item, null );
            boolean hideTitle = false;//CommonPrefs.getHideTitleBar(m_context);
            GameSummary summary = DBUtils.getSummary( m_context, m_path, false );
            if ( null == summary ) {
                m_path = null;
            } else {
                //Assert.assertNotNull( summary );

                TextView view = (TextView)layout.findViewById( R.id.game_name );
                if ( hideTitle ) {
                    view.setVisibility( View.GONE );
                } else {
                    String name = GameUtils.gameName( m_context, m_path );
                    String format = 
                        m_context.getString( R.string.str_game_namef );
                    String lang = 
                        DictLangCache.getLangName( m_context, 
                                                   summary.dictLang );
                    view.setText( String.format( format, name, lang ) );
                }

                LinearLayout list =
                    (LinearLayout)layout.findViewById( R.id.player_list );
                for ( int ii = 0; ii < summary.nPlayers; ++ii ) {
                    View tmp = m_factory.inflate( R.layout.player_list_elem, null );
                    view = (TextView)tmp.findViewById( R.id.item_name );
                    view.setText( summary.summarizePlayer( m_context, ii ) );
                    view = (TextView)tmp.findViewById( R.id.item_score );
                    view.setText( String.format( "  %d", summary.scores[ii] ) );
                    if ( summary.isNextToPlay( ii ) ) {
                        tmp.setBackgroundColor( 0x7F00FF00 );
                    }
                    list.addView( tmp, ii );
                }

                view = (TextView)layout.findViewById( R.id.state );
                view.setText( summary.summarizeState( m_context ) );
                view = (TextView)layout.findViewById( R.id.modtime );
                view.setText( m_df.format( new Date( summary.modtime ) ) );

                view = (TextView)layout.findViewById( R.id.role );
                String roleSummary = summary.summarizeRole( m_context );
                if ( null != roleSummary ) {
                    view.setText( roleSummary );
                } else {
                    view.setVisibility( View.GONE );
                }

                if ( summary.pendingMsgLevel != GameSummary.MSG_FLAGS_NONE ) {
                    View marker = layout.findViewById( R.id.msg_marker );
                    marker.setVisibility( View.VISIBLE );
                }
                synchronized( m_viewsCache ) {
                    m_viewsCache.put( m_path, layout );
                }
            }
            return null;
        } // doInBackground

        @Override
        protected void onPostExecute( Void unused )
        {
            // Utils.logf( "onPostExecute(id=%d)", m_id );
            if ( null != m_path ) {
                m_cb.itemLoaded( m_path );
            }
        }
    } // class LoadItemTask

    public GameListAdapter( Context context, LoadItemCB cb ) {
        super( context, DBUtils.gamesList(context).length );
        m_context = context;
        m_cb = cb;
        m_factory = LayoutInflater.from( context );
        m_df = DateFormat.getDateTimeInstance( DateFormat.SHORT, 
                                               DateFormat.SHORT );

        int sdk_int = 0;
        try {
            sdk_int = Integer.decode( android.os.Build.VERSION.SDK );
        } catch ( Exception ex ) {}

        m_viewsCache = new HashMap<String,View>();
    }
    
    public int getCount() {
        return DBUtils.gamesList(m_context).length;
    }
    
    public Object getItem( int position ) 
    {
        final String path = DBUtils.gamesList(m_context)[position];
        View layout;
        synchronized( m_viewsCache ) {
            layout = m_viewsCache.get( path );
        }

        if ( null == layout ) {
            layout = m_factory.inflate( R.layout.game_list_item, null );
            final boolean hideTitle = 
                false;//CommonPrefs.getHideTitleBar( m_context );

            TextView view = (TextView)layout.findViewById( R.id.game_name );
            if ( hideTitle ) {
                view.setVisibility( View.GONE );
            } else {
                String text = GameUtils.gameName( m_context, path );
                view.setText( text );
            }

            new LoadItemTask( m_context, path/*, ++m_taskCounter*/ ).execute();
        }

        // this doesn't work.  Rather, it breaks highlighting because
        // the background, if we don't set it, is a more complicated
        // object like @android:drawable/list_selector_background.  I
        // tried calling getBackground(), expecting to get a Drawable
        // I could then clone and modify, but null comes back.  So
        // layout must be inheriting its background from elsewhere or
        // it gets set later, during layout.
        // if ( (position%2) == 0 ) {
        //     layout.setBackgroundColor( 0xFF3F3F3F );
        // }

        return layout;
    } // getItem

    public View getView( int position, View convertView, ViewGroup parent ) {
        return (View)getItem( position );
    }

    public void inval( String key ) 
    {
        synchronized( m_viewsCache ) {
            m_viewsCache.remove( key );
        }
    }
}
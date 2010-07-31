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
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.database.DataSetObserver;
import java.io.FileInputStream;
import java.util.HashMap;
import android.view.LayoutInflater;
import junit.framework.Assert;


import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameListAdapter extends XWListAdapter {
    private Context m_context;
    private LayoutInflater m_factory;
    private int m_layoutId;
    private HashMap<String,View> m_viewsCache;

    public GameListAdapter( Context context ) {
        super( context, GameUtils.gamesList(context).length );
        m_context = context;
        m_factory = LayoutInflater.from( context );

        int sdk_int = 0;
        try {
            sdk_int = Integer.decode( android.os.Build.VERSION.SDK );
        } catch ( Exception ex ) {}

        m_layoutId = sdk_int >= android.os.Build.VERSION_CODES.DONUT
            ? R.layout.game_list_item : R.layout.game_list_item_onefive;

        m_viewsCache = new HashMap<String,View>();
    }
    
    public int getCount() {
        return GameUtils.gamesList(m_context).length;
    }
    
    public Object getItem( int position ) 
    {
        final String path = GameUtils.gamesList(m_context)[position];
        View layout = m_viewsCache.get( path );

        if ( null == layout ) {
            Utils.logf( "creating new list elem for %s", path );
            layout = m_factory.inflate( m_layoutId, null );
            byte[] stream = GameUtils.savedGame( m_context, path );
            if ( null != stream ) {
                CurGameInfo gi = new CurGameInfo( m_context );
                XwJNI.gi_from_stream( gi, stream );

                GameSummary summary = DBUtils.getSummary( m_context, path );

                TextView view = (TextView)layout.findViewById( R.id.players );
                String gameName = GameUtils.gameName( m_context, path );
                view.setText( String.format( "%s: %s", gameName,
                                             gi.summarizePlayers( m_context, 
                                                                  summary ) ) );

                view = (TextView)layout.findViewById( R.id.state );
                view.setText( gi.summarizeState( m_context, summary ) );
                view = (TextView)layout.findViewById( R.id.dict );
                view.setText( gi.summarizeDict( m_context ) );

                view = (TextView)layout.findViewById( R.id.role );
                String roleSummary = gi.summarizeRole( m_context, summary );
                if ( null != roleSummary ) {
                    view.setText( roleSummary );
                } else {
                    view.setVisibility( View.GONE );
                }
            }
            m_viewsCache.put( path, layout );
        }
        return layout;
    }

    public View getView( int position, View convertView, ViewGroup parent ) {
        return (View)getItem( position );
    }

    public void inval( String key ) 
    {
        m_viewsCache.remove( key );
    }
}
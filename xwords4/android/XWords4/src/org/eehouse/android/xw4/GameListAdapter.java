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
import android.view.LayoutInflater;
import junit.framework.Assert;


import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameListAdapter implements ListAdapter {
    Context m_context;
    LayoutInflater m_factory;

    public GameListAdapter( Context context ) {
        m_context = context;
        m_factory = LayoutInflater.from( context );
    }

    public boolean areAllItemsEnabled() {
        return true;
    }

    public boolean isEnabled( int position ) {
        return true;
    }
    
    public int getCount() {
        return Utils.gamesList(m_context).length;
    }
    
    public Object getItem( int position ) 
    {
        final View layout = m_factory.inflate( R.layout.game_list_item, null );
        TextView view = (TextView)layout.findViewById( R.id.players );

        String path = Utils.gamesList(m_context)[position];
        byte[] stream = open( path );
        if ( null != stream ) {
            CurGameInfo gi = new CurGameInfo( m_context );
            XwJNI.gi_from_stream( gi, stream );

            GameSummary summary = Utils.getSummary( m_context, path );
            String summaryTxt = gi.summarize( m_context, summary );

            view.setText( summaryTxt );
        }
        return layout;
    }

    public long getItemId( int position ) {
        return position;
    }

    public int getItemViewType( int position ) {
        return 0;
    }

    public View getView( int position, View convertView, ViewGroup parent ) {
        return (View)getItem( position );
    }

    public int getViewTypeCount() {
        return 1;
    }

    public boolean hasStableIds() {
        return true;
    }

    public boolean isEmpty() {
        return getCount() == 0;
    }

    public void registerDataSetObserver(DataSetObserver observer) {}
    public void unregisterDataSetObserver(DataSetObserver observer) {}

    private byte[] open( String file )
    {
        byte[] stream = null;
        try {
            FileInputStream in = m_context.openFileInput( file );
            int len = in.available();
            stream = new byte[len];
            in.read( stream, 0, len );
            in.close();
        } catch ( java.io.FileNotFoundException ex ) {
            Utils.logf( "got FileNotFoundException: " + ex.toString() );
        } catch ( java.io.IOException ex ) {
            Utils.logf( "got IOException: " + ex.toString() );
        }
        return stream;
    }
}
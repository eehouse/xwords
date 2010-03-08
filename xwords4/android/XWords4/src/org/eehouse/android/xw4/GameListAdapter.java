/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

package org.eehouse.android.xw4;

import android.widget.ListAdapter;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.database.DataSetObserver;
import java.io.FileInputStream;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameListAdapter implements ListAdapter {
    Context m_context;

    public GameListAdapter( Context context ) {
        m_context = context;
    }

    public boolean areAllItemsEnabled() {
        return true;
    }

    public boolean isEnabled( int position ) {
        return true;
    }
    
    public int getCount() {
        int count = 0;
        for ( String file : m_context.fileList() ) {
            if ( ! file.endsWith(XWConstants.GAME_EXTN) ) {
                ++count;
            }
        }

        return count;
    }
    
    public Object getItem( int position ) {
        TextView view = new TextView(m_context);

        byte[] stream = open( m_context.fileList()[position] );
        if ( null != stream ) {
            CurGameInfo gi = new CurGameInfo( m_context );
            XwJNI.gi_from_stream( gi, stream );

            view.setText( gi.summary(m_context) );
        }
        return view;
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
/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.widget.Adapter;
import android.widget.ListAdapter;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.database.DataSetObserver;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.nio.CharBuffer;

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

            StringBuffer sb = new StringBuffer();
            int ii;
            for ( ii = 0; ; ) {
                sb.append( gi.players[ii].name );
                if ( ++ii >= gi.nPlayers ) {
                    break;
                }
                sb.append( " vs. " );
            }
            sb.append( "\nDictionary: " );
            sb.append( gi.dictName );

            DeviceRole role = gi.serverRole;
            if ( gi.serverRole != DeviceRole.SERVER_STANDALONE ) {
                sb.append( "\n" )
                    .append( m_context.getString( R.string.role_label ) )
                    .append( ": " );
                if ( role == DeviceRole.SERVER_ISSERVER ) {
                    sb.append( m_context.getString( R.string.role_host ) );
                } else {
                    sb.append( m_context.getString( R.string.role_guest ) );
                }
            }

            view.setText( sb.toString() );
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
        Utils.logf( "open(" + file + ")" );
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
        Utils.logf( "open done" );
        return stream;
    }
}
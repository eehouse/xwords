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
        view.setText( "game " + position );
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
}
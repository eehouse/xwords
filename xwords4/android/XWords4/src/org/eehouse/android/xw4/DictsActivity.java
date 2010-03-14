/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

package org.eehouse.android.xw4;

import android.app.ListActivity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.AdapterView;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;

import junit.framework.Assert;

public class DictsActivity extends ListActivity 
    implements View.OnClickListener {
    String[] m_dicts;

    private class DictListAdapter extends XWListAdapter {
        Context m_context;

        public DictListAdapter( Context context ) {
            super( context, m_dicts.length );
            m_context = context;
        }

        public Object getItem( int position) { return m_dicts[position]; }
        public View getView( final int position, View convertView, 
                             ViewGroup parent ) {
            LayoutInflater factory = LayoutInflater.from( DictsActivity.this );
            final XWListItem view
                = (XWListItem)factory.inflate( R.layout.list_item, null );
            view.setPosition( position );
            view.setText( m_dicts[position] );
            return view;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.dict_browse );
        registerForContextMenu( getListView() );

        Button download = (Button)findViewById( R.id.download );
        download.setOnClickListener( this );

        mkListAdapter();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        mkListAdapter();
    }

    public void onClick( View v ) {
        startActivity( Utils.mkDownloadActivity( this ) );
    }

    @Override
    public void onCreateContextMenu( ContextMenu menu, View view, 
                                     ContextMenuInfo menuInfo ) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate( R.menu.dicts_item_menu, menu );
    }
   
    @Override
    public boolean onContextItemSelected( MenuItem item ) 
    {
        boolean handled = false;
        AdapterView.AdapterContextMenuInfo info;
        try {
            info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        } catch (ClassCastException e) {
            Utils.logf( "bad menuInfo:" + e.toString() );
            return false;
        }
        
        int id = item.getItemId();
        switch( id ) {
        case R.id.dicts_item_delete:
            deleteFile( m_dicts[info.position] );
            mkListAdapter();
            handled = true;
            break;
        case R.id.dicts_item_details:
            Utils.notImpl( this );
            break;
        }

        return handled;
    }

    private void mkListAdapter()
    {
        m_dicts = Utils.dictList( this );
        setListAdapter( new DictListAdapter( this ) );
    }

}
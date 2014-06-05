/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.app.ListActivity;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import org.apache.http.client.methods.HttpPost;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import junit.framework.Assert;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.GameSummary;

public class RemoteDictsDelegate extends ListDelegateBase 
    implements GroupStateListener, SelectableItem,
               DwnldDelegate.DownloadFinishedListener {
    private ListActivity m_activity;
    private ListView m_listView;
    private boolean[] m_expanded;
    private String[] m_langNames;
    private HashMap<String, String[]> m_langInfo;
    private HashSet<XWListItem> m_selDicts = new HashSet<XWListItem>();
    private String m_origTitle;

    protected RemoteDictsDelegate( ListActivity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState, R.menu.remote_dicts );
        m_activity = activity;
    }

    protected void init( Bundle savedInstanceState ) 
    {
        setContentView( R.layout.remote_dicts );
        m_listView = getListView();
        JSONObject params = new JSONObject(); // empty for now
        m_origTitle = getTitle();

        new FetchListTask( m_activity, params ).execute();
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        int nSelected = m_selDicts.size();
        Utils.setItemVisible( menu, R.id.remote_dicts_download, 
                              0 < nSelected );
        Utils.setItemVisible( menu, R.id.remote_dicts_deselect_all, 
                              0 < nSelected );
        Utils.setItemVisible( menu, R.id.remote_dicts_getinfo, 1 == nSelected );
        return true;
    }

    @Override
    public boolean onOptionsItemSelected( MenuItem item ) 
    {
        boolean handled = true;
        switch ( item.getItemId() ) {
        case R.id.remote_dicts_deselect_all:
            clearSelection();
            break;
        case R.id.remote_dicts_download:
            String[] urls = new String[m_selDicts.size()];
            int count = 0;
            for ( Iterator<XWListItem> iter = m_selDicts.iterator(); 
                  iter.hasNext(); ) {
                XWListItem litm = iter.next();
                String langName = (String)litm.getCached();
                urls[count++] = Utils.makeDictUrl( m_activity, langName, 
                                                   litm.getText() );
            }
            DwnldDelegate.downloadDictsInBack( m_activity, urls, this );
            break;
        default:
            handled = false;
        }

        return handled;
    }

    @Override
    protected boolean onBackPressed() {
        boolean handled = 0 < m_selDicts.size();
        if ( handled ) {
            clearSelection();
        }
        return handled;
    }

    //////////////////////////////////////////////////////////////////////
    // DownloadFinishedListener interface
    //////////////////////////////////////////////////////////////////////
    public void downloadFinished( String name, boolean success )
    {
        if ( success ) {
            showToast( name );
        }
    }

    //////////////////////////////////////////////////////////////////////
    // GroupStateListener interface
    //////////////////////////////////////////////////////////////////////
    public void onGroupExpandedChanged( int groupPosition, boolean expanded )
    {
        m_expanded[groupPosition] = expanded;
        mkListAdapter();
    }

    //////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////
    public void itemClicked( LongClickHandler clicked, GameSummary summary )
    {
    }

    public void itemToggled( LongClickHandler toggled, boolean selected )
    {
        XWListItem item = (XWListItem)toggled;
        if ( selected ) {
            m_selDicts.add( item );
        } else {
            m_selDicts.remove( item );
        }
        invalidateOptionsMenuIf();
        setTitleBar();
    }

    public boolean getSelected( LongClickHandler obj )
    {
        XWListItem item = (XWListItem)obj;
        return m_selDicts.contains( item );
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    @Override
    public void dlgButtonClicked( Action action, int which, Object[] params )
    {
        Assert.assertTrue( Action.FINISH_ACTION == action ); 
        finish();
    }

    private void mkListAdapter()
    {
        RDListAdapter adapter = new RDListAdapter();
        setListAdapter( adapter );
    }

    private void setTitleBar()
    {
        int nSels = m_selDicts.size();
        if ( 0 == nSels ) {
            setTitle( m_origTitle );
        } else {
            setTitle( getString( R.string.sel_items_fmt, nSels ) );
        }
    }

    private boolean digestData( String jsonData )
    {
        boolean success = false;
        JSONArray langs = null;
        if ( null != jsonData ) {
            try {
                JSONObject obj = new JSONObject( jsonData );
                langs = obj.optJSONArray( "langs" );

                int nLangs = langs.length();
                m_langNames = new String[nLangs];
                m_expanded = new boolean[nLangs];
                m_langInfo = new HashMap<String, String[]>();
                for ( int ii = 0; ii < nLangs; ++ii ) {
                    JSONObject langObj = langs.getJSONObject( ii );
                    String langName = langObj.getString( "lang" );
                    m_langNames[ii] = langName;

                    JSONArray dicts = langObj.getJSONArray( "dicts" );
                    int nDicts = dicts.length();
                    String[] dictNames = new String[nDicts];
                    for ( int jj = 0; jj < nDicts; ++jj ) {
                        JSONObject dict = dicts.getJSONObject( jj );
                        dictNames[jj] = 
                            DictUtils.removeDictExtn( dict.getString( "xwd" ) );
                    }
                    m_langInfo.put( langName, dictNames );
                }

                Arrays.sort( m_langNames );
                success = true;
            } catch ( JSONException ex ) {
                DbgUtils.loge( ex );
            }
        }

        return success;
    }

    private void clearSelection()
    {
        m_selDicts.clear();
        mkListAdapter();
        invalidateOptionsMenuIf();
        setTitleBar();
    }

    private class FetchListTask extends AsyncTask<Void, Void, Boolean> {
        private Context m_context;
        private JSONObject m_params;

        public FetchListTask( Context context, JSONObject params )
        {
            m_context = context;
            m_params = params;
        }

        @Override 
        public Boolean doInBackground( Void... unused )
        {
            boolean success = false;
            HttpPost post = UpdateCheckReceiver.makePost( m_context, "listDicts" );
            if ( null != post ) {
                String json = null;
                json = UpdateCheckReceiver.runPost( post, m_params );
                // if ( null == json ) {
                //     json = s_fakeData;
                // }
                success = digestData( json );
            }
            return new Boolean( success );
        }
            
        @Override 
        protected void onPostExecute( Boolean success )
        {
            if ( success ) {
                mkListAdapter();
            } else {
                String msg = getString( R.string.remote_no_net );
                showOKOnlyDialogThen( msg, Action.FINISH_ACTION );
            }
        }
    }

    private class RDListAdapter extends XWListAdapter {
        private Integer m_count = null;

        public RDListAdapter() 
        {
            super( 0 );
        }

        @Override
        public int getCount() 
        {
            if ( null == m_count ) {
                int nLangs = m_langNames.length;
                int count = nLangs;
                for ( int ii = 0; ii < nLangs; ++ii ) {
                    if ( m_expanded[ii] ) {
                        count += m_langInfo.get( m_langNames[ii] ).length;
                    }
                }
                m_count = new Integer( count );
            }
            // DbgUtils.logf( "RemoteDictsDelegate.getCount() => %d", m_count );
            return m_count;
        }

        @Override
        public int getViewTypeCount() { return 2; }

        @Override
        public View getView( final int position, View convertView, ViewGroup parent )
        {
            // DbgUtils.logf( "RemoteDictsDelegate(position=%d)", position );
            View result = null;
            int indx = position;

            for ( int ii = 0; ii < m_langNames.length; ++ii ) {
                String langName = m_langNames[ii];
                boolean expanded = m_expanded[ii];
                if ( indx == 0 ) {
                    result = ListGroup.make( m_activity, RemoteDictsDelegate.this, 
                                             ii, langName, expanded );
                    break;
                } else {
                    String[] dicts = m_langInfo.get( langName );
                    int count = expanded ? dicts.length : 0;
                    if ( indx <= count ) {
                        XWListItem item = 
                            XWListItem.inflate( m_activity, RemoteDictsDelegate.this );
                        result = item;

                        item.setText( dicts[indx-1] );
                        item.cache( langName );

                        //item.setOnClickListener( RemoteDictsDelegate.this );
                        break;
                    }
                    indx -= 1 + count;
                }
            }

            Assert.assertNotNull( result );
            return result;
        }
    }
}

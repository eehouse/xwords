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
               DictImportDelegate.DownloadFinishedListener {
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
            for ( Iterator<XWListItem> iter = m_selDicts.iterator(); 
                  iter.hasNext(); ) {
                XWListItem litm = iter.next();
                String langName = (String)litm.getCached();
                String url = Utils.makeDictUrl( m_activity, langName, 
                                                litm.getText() );
                DictImportDelegate.downloadDictInBack( m_activity, url );
            }
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

    // For debugging when network is down
    // private static final String s_fakeData = "{\"langs\": [{\"lang\": \"Swedish\", \"dicts\": [{\"xwd\": \"Swedish_2to8.xwd\", \"md5sums\": [\"f3d235de4aa92a0c975361330c076ce6\", \"9e30bb5807204a61560fa9cae3bc4d37\"]}, {\"xwd\": \"Swedish_2to9.xwd\", \"md5sums\": [\"3e0f00e971d4a8dffb3476e512d37a98\", \"6b391ad2a06d94a757728cb187858e7b\"]}, {\"xwd\": \"Swedish_2to15.xwd\", \"md5sums\": [\"813eecdff13792007ee8b0c9c5e1f98b\", \"91a82698db86ff5a9892fe2021b4fd9f\"]}]}, {\"lang\": \"Portuguese\", \"dicts\": [{\"xwd\": \"PortuguesePT_2to8.xwd\", \"md5sums\": [\"0ac9278f5139a546c29b27ba4a0e21a5\", \"1ee181c1a6a4da71a08ebf1b7fb45520\"]}, {\"xwd\": \"PortugueseBR_2to8.xwd\", \"md5sums\": [\"a39ea7048ae74b75b6b20b2df580e864\", \"38db4fc25ef1613a79571c7c44c489e3\"]}, {\"xwd\": \"PortuguesePT_2to15.xwd\", \"md5sums\": [\"16495e0067d8a55652936ebda4982920\", \"f6d39b6f8d7737bfe57334b1f5d0e8f2\"]}, {\"xwd\": \"PortuguesePT_2to9.xwd\", \"md5sums\": [\"1470782527fe74608ad0fd6f5c15f74e\", \"abc7a0b722bb40a102bc1c5e15ada4e8\"]}, {\"xwd\": \"PortugueseBR_2to15.xwd\", \"md5sums\": [\"652105dd662d46b0d6c49b0f8cbb71f5\", \"e3335663d54a0461dccaf20df1e29515\"]}, {\"xwd\": \"PortugueseBR_2to9.xwd\", \"md5sums\": [\"bb32932c0a74fde156d839194eeb4834\", \"90e87b6577d70c90fe674032a2abf42d\"]}]}, {\"lang\": \"Danish\", \"dicts\": [{\"xwd\": \"Danish_2to8.xwd\", \"md5sums\": [\"ba41631091474d717faa4d47f25737f1\", \"a0aa9f4874e9e4d18b0bdf40cb3386af\"]}, {\"xwd\": \"Danish_2to15.xwd\", \"md5sums\": [\"565ed922d9a68760ae03d6ddd590e454\", \"613d6a9cae033487c7c946bbb88135d4\"]}, {\"xwd\": \"Danish_2to9.xwd\", \"md5sums\": [\"bb8150cadc8db778cef69e7593cfb513\", \"e25137525e2e437e0591df13c5d0440f\"]}]}, {\"lang\": \"Czech\", \"dicts\": [{\"xwd\": \"Czech_2to9.xwd\", \"md5sums\": [\"00794634cb13316e31412847cda3be99\", \"9de228bc6bc0d37f7fc341d0cd456668\"]}, {\"xwd\": \"Czech_2to15.xwd\", \"md5sums\": [\"bb5cc7654feadc39357bc9e6d21dc858\", \"193b9ca06feded3e6bf195cb5df12c69\"]}, {\"xwd\": \"Czech_2to8.xwd\", \"md5sums\": [\"823cdf4890c8a79ee36e622c1a76ba7b\", \"43567b60c9c5afb74c99fd0b367411e3\"]}]}, {\"lang\": \"Dutch\", \"dicts\": [{\"xwd\": \"Dutch_2to15.xwd\", \"md5sums\": [\"b8dcfab7b7a2b068803b400714217888\", \"ddca280eba4d734a8da852e1687291bf\"]}, {\"xwd\": \"Dutch_2to8.xwd\", \"md5sums\": [\"41a3e9f6bbf197c296e8f3ff98bfc101\", \"25ac0d3eca10bd714284f7a7784841eb\"]}, {\"xwd\": \"Dutch_2to9.xwd\", \"md5sums\": [\"74399839d815b0736e596b331090f2ac\", \"124e6070d6f58d4c38ee24f4a08408d1\"]}]}, {\"lang\": \"Hex\", \"dicts\": [{\"xwd\": \"Hex_2to8.xwd\", \"md5sums\": [\"eef9480c25aa4908e47d1e2bff090d67\", \"c8a007804476018ec78f77ea845cd4c9\"]}]}, {\"lang\": \"French\", \"dicts\": [{\"xwd\": \"ODS5_2to8.xwd\", \"md5sums\": [\"577c9d0c76d062829937cd51a1b0d333\", \"90a908584be4a40341ce0415d45ffea1\"]}, {\"xwd\": \"ODS5_2to15.xwd\", \"md5sums\": [\"aeaad6084faaba9635a70b0b87ba653f\", \"dcabd23fed80cee70b71c91a08fbab7a\"]}, {\"xwd\": \"ODS5_2to9.xwd\", \"md5sums\": [\"6ec776ff3f407db47cb319c98f8076c1\", \"818628c0607f78ac6aa95ea38fedb9f8\"]}]}, {\"lang\": \"German\", \"dicts\": [{\"xwd\": \"Deutsch_2to8.xwd\", \"md5sums\": [\"1102bff3b6d6bddee52b5f76ddcb898e\", \"2a9638946ff0008ff8309d391e817c2b\"]}, {\"xwd\": \"German_2to15.xwd\", \"md5sums\": [\"566563d5fcb1c8b1ebbb6f0c02336938\", \"41c9188c76e5c99751224d8ff704a4c7\"]}, {\"xwd\": \"Deutsch_2to15.xwd\", \"md5sums\": [\"60fca5f327a461e08e2ac33b7017f0c2\", \"71bbb856294742b58543d8b5ba8fee3f\"]}, {\"xwd\": \"Deutsch_2to9.xwd\", \"md5sums\": [\"6013767144d5d3450eb85e95a292f05e\", \"a659d44c20e80f9fa571e93cdad219b0\"]}, {\"xwd\": \"German_2to8.xwd\", \"md5sums\": [\"c188988fb4386edfd69c5e65624bbc38\", \"1a26bc3a505acb49e64eedcd38454af3\"]}, {\"xwd\": \"German_2to9.xwd\", \"md5sums\": [\"5ff32483a60ef0bb141ecd5fed078981\", \"1306f0c0f39094006532d609740f81d0\"]}]}]}";
}

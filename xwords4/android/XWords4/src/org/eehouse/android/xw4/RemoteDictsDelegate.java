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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
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
    private boolean[] m_expanded;
    private String[] m_langNames;
    private static enum DictState { AVAILABLE, INSTALLED, NEEDS_UPDATE };
    private static class DictInfo implements Comparable {
        public String m_name;
        public DictState m_state;
        public String m_lang;
        public DictInfo( String name, String lang ) { m_name = name; m_lang = lang; }
        public int compareTo( Object obj ) {
            DictInfo other = (DictInfo)obj;
            return m_name.compareTo( other.m_name );
        }
    }
    private HashMap<String, DictInfo[]> m_langInfo;
    private HashMap<String, XWListItem> m_selDicts = new HashMap<String, XWListItem>();
    private String m_origTitle;
    private String m_installed;
    private String m_needsUpdate;
    private HashMap<String, XWListItem> m_curDownloads;

    protected RemoteDictsDelegate( ListActivity activity, Bundle savedInstanceState )
    {
        super( activity, savedInstanceState, R.menu.remote_dicts );
        m_activity = activity;
    }

    protected void init( Bundle savedInstanceState ) 
    {
        m_installed = getString( R.string.dict_installed );
        m_needsUpdate = getString( R.string.dict_needs_update );

        setContentView( R.layout.remote_dicts );
        JSONObject params = new JSONObject(); // empty for now
        m_origTitle = getTitle();

        new FetchListTask( m_activity, params ).execute();
    }

    @Override
    public boolean onPrepareOptionsMenu( Menu menu ) 
    {
        int nSelected = m_selDicts.size();
        Utils.setItemVisible( menu, R.id.remote_dicts_download, 
                              0 < countNeedDownload() );
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
            String[] urls = new String[countNeedDownload()];
            int count = 0;
            m_curDownloads = new HashMap<String, XWListItem>();
            for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
                  iter.hasNext(); ) {
                XWListItem litm = iter.next();
                DictInfo info = (DictInfo)litm.getCached();
                if ( null == info.m_state || 
                     DictState.INSTALLED != info.m_state ) {
                    String url = Utils.makeDictUrl( m_activity, info.m_lang, 
                                                    litm.getText() );
                    urls[count++] = url;
                    m_curDownloads.put( url, litm );
                }
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
        XWListItem item = m_curDownloads.get( name );
        if ( null != item ) {
            if ( success ) {
                DictInfo info = (DictInfo)item.getCached();
                info.m_state = DictState.INSTALLED;
                item.setComment( m_installed );
            }
            m_curDownloads.remove( name );
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
        String name = item.getText();
        if ( selected ) {
            m_selDicts.put( name, item );
        } else {
            m_selDicts.remove( name );
        }
        invalidateOptionsMenuIf();
        setTitleBar();
    }

    public boolean getSelected( LongClickHandler obj )
    {
        XWListItem item = (XWListItem)obj;
        return m_selDicts.containsKey( item.getText() );
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

    private int countNeedDownload()
    {
        int result = 0;
        for ( Iterator<XWListItem> iter = m_selDicts.values().iterator(); 
              iter.hasNext(); ) {
            XWListItem litm = iter.next();
            DictInfo info = (DictInfo)litm.getCached();
            if ( null == info.m_state || DictState.INSTALLED != info.m_state ) {
                ++result;
            }
        }
        return result;
    }

    private void mkListAdapter()
    {
        setListAdapterKeepScroll( new RDListAdapter() );
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
            // DictLangCache hits the DB hundreds of times below. Fix!
            DbgUtils.logf( "Fix me I'm stupid" );
            try {
                JSONObject obj = new JSONObject( jsonData );
                langs = obj.optJSONArray( "langs" );

                int nLangs = langs.length();
                ArrayList<String> langNames = new ArrayList<String>();
                m_langInfo = new HashMap<String, DictInfo[]>();
                for ( int ii = 0; ii < nLangs; ++ii ) {
                    JSONObject langObj = langs.getJSONObject( ii );
                    String langName = langObj.getString( "lang" );

                    JSONArray dicts = langObj.getJSONArray( "dicts" );
                    int nDicts = dicts.length();
                    ArrayList<DictInfo> dictNames = new ArrayList<DictInfo>();
                    for ( int jj = 0; jj < nDicts; ++jj ) {
                        JSONObject dict = dicts.getJSONObject( jj );
                        String name = dict.getString( "xwd" );
                        name = DictUtils.removeDictExtn( name );
                        DictInfo info = new DictInfo( name, langName );
                        if ( DictLangCache.haveDict( m_activity, langName, name ) ) {
                            boolean matches = true;
                            String curSum = DictLangCache.getDictMD5Sum( m_activity, name );
                            if ( null != curSum ) {
                                JSONArray sums = dict.getJSONArray("md5sums");
                                if ( null != sums ) {
                                    matches = false;
                                    for ( int kk = 0; !matches && kk < sums.length(); ++kk ) {
                                        String sum = sums.getString( kk );
                                        matches = sum.equals( curSum );
                                    }
                                }
                            }
                            info.m_state = matches? DictState.INSTALLED 
                                : DictState.NEEDS_UPDATE;
                        }
                        dictNames.add( info );
                    }
                    if ( 0 < dictNames.size() ) {
                        DictInfo[] asArray = dictNames.toArray( new DictInfo[dictNames.size()] );
                        Arrays.sort( asArray );
                        m_langInfo.put( langName, asArray );

                        langNames.add( langName );
                    }
                }
                Collections.sort( langNames );
                m_langNames = langNames.toArray( new String[langNames.size()] );

                // Now start out with languages expanded that have an
                // installed dict.
                nLangs = m_langNames.length;
                m_expanded = new boolean[nLangs];
                for ( int ii = 0; ii < nLangs; ++ii ) {
                    DictInfo[] dicts = m_langInfo.get( m_langNames[ii] );
                    for ( DictInfo info : dicts ) {
                        if ( null != info.m_state ) {
                            m_expanded[ii] = true;
                            break;
                        }
                    }
                }

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
        private Object[] m_listInfo;

        public RDListAdapter() 
        {
            super( 0 );
        }

        @Override
        public int getCount() 
        {
            if ( null == m_listInfo ) {
                ArrayList<Object> alist = new ArrayList<Object>();
                int nLangs = m_langNames.length;
                for ( int ii = 0; ii < nLangs; ++ii ) {
                    alist.add( new Integer(ii) );
                    if ( m_expanded[ii] ) {
                        DictInfo[] dis = m_langInfo.get( m_langNames[ii] );
                        alist.addAll( Arrays.asList( dis ) );
                    }
                }
                m_listInfo = alist.toArray( new Object[alist.size()] );
            }
            // DbgUtils.logf( "RemoteDictsDelegate.getCount() => %d", m_listInfo.length );
            return m_listInfo.length;
        }

        @Override
        public int getViewTypeCount() { return 2; }

        @Override
        public View getView( final int position, View convertView, ViewGroup parent )
        {
            if ( null != convertView ) {
                DbgUtils.logf( "RemoteDictsDelegate(position=%d, convertView=%H)", 
                               position, convertView );
            }
            View result = null;

            Object obj = m_listInfo[position];
            if ( obj instanceof Integer ) {
                int ii = (Integer)obj;
                String langName = m_langNames[ii];
                boolean expanded = m_expanded[ii];
                result = ListGroup.make( m_activity, RemoteDictsDelegate.this, 
                                         ii, langName, expanded );
            } else if ( obj instanceof DictInfo ) {
                DictInfo info = (DictInfo)obj;
                XWListItem item = 
                    XWListItem.inflate( m_activity, RemoteDictsDelegate.this );
                result = item;

                String name = info.m_name;
                item.setText( name );

                if ( null != info.m_state ) {
                    String comment = null;
                    switch( info.m_state ) {
                    case INSTALLED:
                        comment = m_installed;
                        break;
                    case NEEDS_UPDATE:
                        comment = m_needsUpdate;
                        break;
                    }
                    item.setComment( comment );
                }
                item.cache( info );

                if ( m_selDicts.containsKey( name ) ) {
                    m_selDicts.put( name, item );
                    item.setSelected( true );
                }
            } else {
                Assert.fail();
            }

            Assert.assertNotNull( result );
            return result;
        }
    }
}

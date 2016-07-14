/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Parcelable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

import java.net.URI;
import java.net.URLConnection;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import junit.framework.Assert;

public class DwnldDelegate extends ListDelegateBase {

    // URIs coming in in intents
    private static final String APK_EXTRA = "APK";
    private static final String DICTS_EXTRA = "XWDS";

    private Activity m_activity;
    private ArrayList<LinearLayout> m_views;
    private ArrayList<DownloadFilesTask> m_dfts;

    public interface DownloadFinishedListener {
        void downloadFinished( String lang, String name, boolean success );
    }

    public interface OnGotLcDictListener {
        void gotDictInfo( boolean success, String lc, String name );
    }

    public DwnldDelegate( Delegator delegator, Bundle savedInstanceState )
    {
        super( delegator, savedInstanceState, R.layout.import_dict );
        m_activity = delegator.getActivity();
    }

    // Track callbacks for downloads.
    private static class ListenerData {
        public ListenerData( Uri uri, String name, DownloadFinishedListener lstnr )
        {
            m_uri = uri;
            m_name = name;
            m_lstnr = lstnr;
        }
        public Uri m_uri;
        public String m_name;
        public DownloadFinishedListener m_lstnr;
    }
    private static Map<Uri,ListenerData> s_listeners =
        new HashMap<Uri,ListenerData>();

    private class DownloadFilesTask extends AsyncTask<Void, Void, Void>
        implements DictUtils.DownProgListener {
        private String m_savedDict = null;
        private Uri m_uri = null;
        private boolean m_isApp = false;
        private File m_appFile = null;
        private int m_totalRead = 0;
        private LinearLayout m_listItem;
        private ProgressBar m_progressBar;

        public DownloadFilesTask( Uri uri, LinearLayout item, boolean isApp )
        {
            super();
            m_uri = uri;
            m_isApp = isApp;
            m_listItem = item;
            m_progressBar = (ProgressBar)
                item.findViewById( R.id.progress_bar );
        }

        public void setLabel( String text )
        {
            TextView tv = (TextView)m_listItem.findViewById( R.id.dwnld_message );
            tv.setText( text );
        }

        @Override
        protected Void doInBackground( Void... unused )
        {
            m_savedDict = null;
            m_appFile = null;

            try {
                URI jUri = new URI( m_uri.getScheme(),
                                    m_uri.getSchemeSpecificPart(),
                                    m_uri.getFragment() );
                URLConnection conn = jUri.toURL().openConnection();
                final int fileLen = conn.getContentLength();
                post( new Runnable() {
                        public void run() {
                            m_progressBar.setMax( fileLen );
                        }
                    });
                InputStream is = conn.getInputStream();
                String name = basename( m_uri.getPath() );
                if ( m_isApp ) {
                    m_appFile = saveToDownloads( is, name, this );
                } else {
                    m_savedDict = saveDict( is, name, this );
                }
                is.close();
            } catch ( java.net.URISyntaxException use ) {
                DbgUtils.loge( use );
            } catch ( java.net.MalformedURLException mue ) {
                DbgUtils.loge( mue );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }
            return null;
        }

        @Override
        protected void onCancelled()
        {
            callListener( m_uri, false );
        }

        @Override
        protected void onPostExecute( Void unused )
        {
            if ( null != m_savedDict ) {
                DictUtils.DictLoc loc =
                    XWPrefs.getDefaultLoc( m_activity );
                DictLangCache.inval( m_activity, m_savedDict,
                                     loc, true );
                callListener( m_uri, true );
            } else if ( null != m_appFile ) {
                // launch the installer
                Intent intent = Utils.makeInstallIntent( m_appFile );
                startActivity( intent );
            } else {
                // we failed at something....
                callListener( m_uri, false );
            }

            if ( 1 >= m_views.size() ) {
                finish();
            } else {
                m_views.remove( m_listItem );
                m_dfts.remove( this );
                mkListAdapter();
            }
        }

        //////////////////////////////////////////////////////////////////////
        // interface DictUtils.DownProgListener
        //////////////////////////////////////////////////////////////////////
        public void progressMade( int nBytes )
        {
            m_totalRead += nBytes;
            post( new Runnable() {
                    public void run() {
                        m_progressBar.setProgress( m_totalRead );
                    }
                });
        }

        private File saveToDownloads( InputStream is, String name,
                                      DictUtils.DownProgListener dpl )
        {
            boolean success = false;
            File appFile = new File( DictUtils.getDownloadDir( m_activity ), name );

            byte[] buf = new byte[1024*4];
            try {
                FileOutputStream fos = new FileOutputStream( appFile );
                boolean cancelled = false;
                for ( ; ; ) {
                    cancelled = isCancelled();
                    if ( cancelled ) {
                        break;
                    }
                    int nRead = is.read( buf, 0, buf.length );
                    if ( 0 > nRead ) {
                        break;
                    }
                    fos.write( buf, 0, nRead );
                    dpl.progressMade( nRead );
                }
                fos.close();
                success = !cancelled;
            } catch ( java.io.FileNotFoundException fnf ) {
                DbgUtils.loge( fnf );
            } catch ( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }

            if ( !success ) {
                appFile.delete();
                appFile = null;
            }
            return appFile;
        }
    } // class DownloadFilesTask

    private class ImportListAdapter extends XWListAdapter {
        public ImportListAdapter() { super( m_views.size() ); }
        public View getView( int position, View convertView, ViewGroup parent )
        {
            return m_views.get( position );
        }
    }

    protected void init( Bundle savedInstanceState )
    {
        m_dfts = new ArrayList<DownloadFilesTask>();
        DownloadFilesTask dft = null;
        Uri[] uris = null;
        LinearLayout item = null;

        Intent intent = getIntent();
        Uri uri = intent.getData(); // launched from Manifest case
        if ( null == uri ) {
            String appUrl = intent.getStringExtra( APK_EXTRA );
            boolean isApp = null != appUrl;
            if ( isApp ) {
                uris = new Uri[] { Uri.parse( appUrl ) };
            } else {
                Parcelable[] parcels = intent.getParcelableArrayExtra( DICTS_EXTRA );
                uris = new Uri[parcels.length];
                for ( int ii = 0; ii < parcels.length; ++ii ) {
                    uris[ii] = (Uri)(parcels[ii]);
                }
            }
            if ( null != uris ) {
                m_views = new ArrayList<LinearLayout>();
                for ( int ii = 0; ii < uris.length; ++ii ) {
                    item = (LinearLayout)inflate( R.layout.import_dict_item );
                    m_dfts.add( new DownloadFilesTask( uris[ii], item, isApp ));
                    m_views.add( item );
                }
            }
        } else if ( (null != intent.getType()
                     && intent.getType().equals( "application/x-xwordsdict" ))
                    || uri.toString().endsWith( XWConstants.DICT_EXTN ) ) {
            item = (LinearLayout)inflate( R.layout.import_dict_item );
            dft = new DownloadFilesTask( uri, item, false );
            uris = new Uri[] { uri };
        }

        if ( null != dft ) {
            Assert.assertTrue( 0 == m_dfts.size() );
            m_dfts.add( dft );
            m_views = new ArrayList<LinearLayout>( 1 );
            m_views.add( item );
            dft = null;
        }

        if ( 0 == m_dfts.size() ) {
            finish();
        } else {
            Assert.assertTrue( m_dfts.size() == uris.length );
            mkListAdapter();

            for ( int ii = 0; ii < uris.length; ++ii ) {
                String showName = basename( uris[ii].getPath() );
                showName = DictUtils.removeDictExtn( showName );
                String msg =
                    getString( R.string.downloading_dict_fmt, showName );

                dft = m_dfts.get( ii );
                dft.setLabel( msg );
                dft.execute();
            }
        }
    } // init

    @Override
    protected boolean handleBackPressed()
    {
        // cancel any tasks that remain
        for ( Iterator<DownloadFilesTask> iter = m_dfts.iterator();
              iter.hasNext(); ) {
            DownloadFilesTask dft = iter.next();
            dft.cancel( true );
        }
        return super.handleBackPressed();
    }

    private void mkListAdapter()
    {
        setListAdapter( new ImportListAdapter() );
    }

    private String saveDict( InputStream inputStream, String name,
                             DictUtils.DownProgListener dpl )
    {
        DictUtils.DictLoc loc = XWPrefs.getDefaultLoc( m_activity );
        if ( !DictUtils.saveDict( m_activity, inputStream, name, loc, dpl ) ) {
            name = null;
        }
        return name;
    }

    private String basename( String path )
    {
        return new File(path).getName();
    }

    private static String langFromUri( Uri uri )
    {
        List<String> segs = uri.getPathSegments();
        String result = segs.get( segs.size() - 2 );
        return result;
    }

    private static void rememberListener( Uri uri, String name,
                                          DownloadFinishedListener lstnr )
    {
        ListenerData ld = new ListenerData( uri, name, lstnr );
        synchronized( s_listeners ) {
            s_listeners.put( uri, ld );
        }
    }

    private static void callListener( Uri uri, boolean success )
    {
        if ( null != uri ) {
            ListenerData ld;
            synchronized( s_listeners ) {
                ld = s_listeners.get( uri );
                if ( null != ld ) {
                    s_listeners.remove( uri );
                }
            }
            if ( null != ld ) {
                String name = ld.m_name;
                String lang = langFromUri( uri );
                if ( null == name ) {
                    name = uri.toString();
                }
                ld.m_lstnr.downloadFinished( lang, name, success );
            }
        }
    }

    public static void downloadDictInBack( Context context, String langName,
                                           String name,
                                           DownloadFinishedListener lstnr )
    {
        Uri uri = Utils.makeDictUri( context, langName, name );
        downloadDictInBack( context, uri, name, lstnr );
    }

    public static void downloadDictInBack( Context context, int lang,
                                           String name,
                                           DownloadFinishedListener lstnr )
    {
        Uri uri = Utils.makeDictUri( context, lang, name );
        downloadDictInBack( context, uri, name, lstnr );
    }

    public static void downloadDictInBack( Context context, Uri uri,
                                           String name,
                                           DownloadFinishedListener lstnr )
    {
        Uri[] uris = new Uri[] { uri };
        String[] names = new String[] { name };
        downloadDictsInBack( context, uris, names, lstnr );
    }

    public static void downloadDictsInBack( Context context, Uri[] uris,
                                            String[] names,
                                            DownloadFinishedListener lstnr )
    {
        if ( null != lstnr ) {
            for ( int ii = 0; ii < uris.length; ++ii ) {
                rememberListener( uris[ii], names[ii], lstnr );
            }
        }

        Intent intent = new Intent( context, DwnldActivity.class );
        intent.putExtra( DICTS_EXTRA, uris ); // uris implement Parcelable
        context.startActivity( intent );
    }

    public static Intent makeAppDownloadIntent( Context context, String url )
    {
        Intent intent = new Intent( context, DwnldActivity.class );
        intent.putExtra( APK_EXTRA, url );
        return intent;
    }

}

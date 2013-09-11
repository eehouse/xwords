/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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
import android.os.Handler;
import android.view.Window;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

import java.net.URI;
import java.net.URLConnection;
import java.security.MessageDigest;
import java.util.HashMap;

import junit.framework.Assert;

public class DictImportActivity extends XWActivity {

    // URIs coming in in intents
    private static final String APK_EXTRA = "APK";
    private static final String DICT_EXTRA = "XWD";

    private ProgressBar m_progressBar;
    private Handler m_handler;

    public interface DownloadFinishedListener {
        void downloadFinished( String name, boolean success );
    }

    // Track callbacks for downloads.
    private static class ListenerData {
        public ListenerData( String dictName, DownloadFinishedListener lstnr )
        {
            m_dictName = dictName;
            m_lstnr = lstnr;
        }
        public String m_dictName;
        public DownloadFinishedListener m_lstnr;
    }
    private static HashMap<String,ListenerData> s_listeners =
        new HashMap<String,ListenerData>();

    private class DownloadFilesTask extends AsyncTask<Uri, Integer, Long> 
        implements DictUtils.DownProgListener {
        private String m_savedDict = null;
        private String m_url = null;
        private boolean m_isApp = false;
        private File m_appFile = null;
        private int m_totalRead = 0;

        public DownloadFilesTask( boolean isApp )
        {
            super();
            m_isApp = isApp;
        }

        public DownloadFilesTask( String url, boolean isApp )
        {
            this( isApp );
            m_url = url;
        }

        @Override
        protected Long doInBackground( Uri... uris )
        {
            m_savedDict = null;
            m_appFile = null;

            int count = uris.length;
            Assert.assertTrue( 1 == count );
            for ( int ii = 0; ii < count; ii++ ) {
                Uri uri = uris[ii];
                DbgUtils.logf( "trying %s", uri );

                try {
                    URI jUri = new URI( uri.getScheme(), 
                                        uri.getSchemeSpecificPart(), 
                                        uri.getFragment() );
                    URLConnection conn = jUri.toURL().openConnection();
                    final int fileLen = conn.getContentLength();
                    m_handler.post( new Runnable() {
                            public void run() {
                                m_progressBar.setMax( fileLen );
                            }
                        });
                    InputStream is = conn.getInputStream();
                    String name = basename( uri.getPath() );
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
            }
            return new Long(0);
        }

        @Override
        protected void onPostExecute( Long result )
        {
            DbgUtils.logf( "onPostExecute passed %d", result );
            if ( null != m_savedDict ) {
                DictUtils.DictLoc loc = 
                    XWPrefs.getDefaultLoc( DictImportActivity.this );
                DictLangCache.inval( DictImportActivity.this, m_savedDict, 
                                     loc, true );
                callListener( m_url, true );
            } else if ( null != m_appFile ) {
                // launch the installer
                Intent intent = Utils.makeInstallIntent( m_appFile );
                startActivity( intent );
            } else {
                // we failed at something....
                callListener( m_url, false );
            }
            finish();
        }

        // interface DictUtils.DownProgListener
        public void progressMade( int nBytes )
        {
            m_totalRead += nBytes;
            m_handler.post( new Runnable() {
                    public void run() {
                        m_progressBar.setProgress( m_totalRead );
                    }
                });
        }
    } // class DownloadFilesTask

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        DownloadFilesTask dft = null;

        m_handler = new Handler();

        requestWindowFeature( Window.FEATURE_LEFT_ICON );
        setContentView( R.layout.import_dict );
        getWindow().setFeatureDrawableResource( Window.FEATURE_LEFT_ICON,
                                                R.drawable.icon48x48 );

        m_progressBar = (ProgressBar)findViewById( R.id.progress_bar );

        Intent intent = getIntent();
        Uri uri = intent.getData();
        if ( null == uri ) {
            String url = intent.getStringExtra( APK_EXTRA );
            boolean isApp = null != url;
            if ( !isApp ) {
                url = intent.getStringExtra( DICT_EXTRA );
            }
            if ( null != url ) {
                dft = new DownloadFilesTask( url, isApp );
                uri = Uri.parse( url );
            }
        } else if ( null != intent.getType() 
                    && intent.getType().equals( "application/x-xwordsdict" ) ) {
            dft = new DownloadFilesTask( false );
        } else if ( uri.toString().endsWith( XWConstants.DICT_EXTN ) ) {
            dft = new DownloadFilesTask( uri.toString(), false );
        }

        if ( null == dft ) {
            finish();
        } else {
            String showName = basename( uri.getPath() );
            String msg = getString( R.string.downloading_dictf, showName );
            TextView view = (TextView)findViewById( R.id.dwnld_message );
            view.setText( msg );
        
            dft.execute( uri );
        }
    }

    private File saveToDownloads( InputStream is, String name, 
                                  DictUtils.DownProgListener dpl )
    {
        boolean success = false;
        File appFile = new File( DictUtils.getDownloadDir( this ), name );

        byte[] buf = new byte[1024*4];
        try {
            FileOutputStream fos = new FileOutputStream( appFile );
            int nRead;
            while ( 0 <= (nRead = is.read( buf, 0, buf.length )) ) {
                fos.write( buf, 0, nRead );
                dpl.progressMade( nRead );
            }
            fos.close();
            success = true;
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

    private String saveDict( InputStream inputStream, String name, 
                             DictUtils.DownProgListener dpl )
    {
        DictUtils.DictLoc loc = XWPrefs.getDefaultLoc( this );
        if ( !DictUtils.saveDict( this, inputStream, name, loc, dpl ) ) {
            name = null;
        }
        return name;
    }

    private String basename( String path )
    {
        return new File(path).getName();
    }

    private static void rememberListener( String url, String name, 
                                          DownloadFinishedListener lstnr )
    {
        ListenerData ld = new ListenerData( name, lstnr );
        synchronized( s_listeners ) {
            s_listeners.put( url, ld );
        }
    }

    private static void callListener( String url, boolean success ) 
    {
        if ( null != url ) {
            ListenerData ld;
            synchronized( s_listeners ) {
                ld = s_listeners.get( url );
                if ( null != ld ) {
                    s_listeners.remove( url );
                }
            }
            if ( null != ld ) {
                ld.m_lstnr.downloadFinished( ld.m_dictName, success );
            }
        }
    }

    public static void downloadDictInBack( Context context, int lang, 
                                           String name, 
                                           DownloadFinishedListener lstnr )
    {
        String url = Utils.makeDictUrl( context, lang, name );
        if ( null != lstnr ) {
            rememberListener( url, name, lstnr );
        }
        downloadDictInBack( context, url );
    }

    public static void downloadDictInBack( Context context, String url )
    {
        Intent intent = new Intent( context, DictImportActivity.class );
        intent.putExtra( DICT_EXTRA, url );
        context.startActivity( intent );
    }

    public static Intent makeAppDownloadIntent( Context context, String url )
    {
        Intent intent = new Intent( context, DictImportActivity.class );
        intent.putExtra( APK_EXTRA, url );
        return intent;
    }

}



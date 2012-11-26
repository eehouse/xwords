/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.Window;
import android.widget.ProgressBar;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.URI;

import junit.framework.Assert;

public class DictImportActivity extends XWActivity {

    public static final String APK_EXTRA = "APK";

    private class DownloadFilesTask extends AsyncTask<Uri, Integer, Long> {
        private String m_saved = null;
        private boolean m_isApp = false;
        private File m_appFile = null;

        public DownloadFilesTask( boolean isApp )
        {
            super();
            m_isApp = isApp;
        }

        @Override
        protected Long doInBackground( Uri... uris )
        {
            m_saved = null;
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
                    InputStream is = jUri.toURL().openStream();
                    String name = basename( uri.getPath() );
                    if ( m_isApp ) {
                        m_appFile = saveToDownloads( is, name );
                    } else {
                        m_saved = saveDict( is, name );
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
            if ( null != m_saved ) {
                DictUtils.DictLoc loc = 
                    XWPrefs.getDefaultLoc( DictImportActivity.this );
                DictLangCache.inval( DictImportActivity.this, m_saved, 
                                     loc, true );
            } else if ( null != m_appFile ) {
                // launch the installer
                Intent intent = Utils.makeInstallIntent( m_appFile );
                startActivity( intent );
            }
            finish();
        }
    } // class DownloadFilesTask

	@Override
	protected void onCreate( Bundle savedInstanceState ) 
    {
		super.onCreate( savedInstanceState );
        DownloadFilesTask dft = null;

		requestWindowFeature( Window.FEATURE_LEFT_ICON );
		setContentView( R.layout.import_dict );
		getWindow().setFeatureDrawableResource( Window.FEATURE_LEFT_ICON,
                                                R.drawable.icon48x48 );

		ProgressBar progressBar = (ProgressBar)findViewById( R.id.progress_bar );

		Intent intent = getIntent();
		Uri uri = intent.getData();
        if ( null == uri ) {
            String url = intent.getStringExtra( APK_EXTRA );
            if ( null != url ) {
                dft = new DownloadFilesTask( true );
                uri = Uri.parse(url);
            }
        } else if ( null != intent.getType() 
                    && intent.getType().equals( "application/x-xwordsdict" ) ) {
            dft = new DownloadFilesTask( false );
        } else if ( uri.toString().endsWith( XWConstants.DICT_EXTN ) ) {
            String txt = getString( R.string.downloading_dictf,
                                    basename( uri.getPath()) );
            TextView view = (TextView)findViewById( R.id.dwnld_message );
            view.setText( txt );
            dft = new DownloadFilesTask( false );
        }

        if ( null == dft ) {
            finish();
        } else {
            dft.execute( uri );
        }
	}

    private File saveToDownloads( InputStream is, String name )
    {
        boolean success = false;
        File appFile = new File( DictUtils.getDownloadDir( this ), name );
        Assert.assertNotNull( appFile );
        
        byte[] buf = new byte[1024*4];
        try {
            FileOutputStream fos = new FileOutputStream( appFile );
            int nRead;
            while ( 0 <= (nRead = is.read( buf, 0, buf.length )) ) {
                fos.write( buf, 0, nRead );
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

    private String saveDict( InputStream inputStream, String name )
    {
        DictUtils.DictLoc loc = XWPrefs.getDefaultLoc( this );
        if ( !DictUtils.saveDict( this, inputStream, name, loc ) ) {
            name = null;
        }
        return name;
    }

    private String basename( String path )
    {
        return new File(path).getName();
    }
}



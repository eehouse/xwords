/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Parcelable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;


import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.URI;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.Utils.ISOCode;

public class DwnldDelegate extends ListDelegateBase {
    private static final String TAG = DwnldDelegate.class.getSimpleName();
    private static final String APKS_DIR = "apks";

    // URIs coming in in intents
    private static final String APK_EXTRA = "APK";
    private static final String DICTS_EXTRA = "XWDS";
    private static final String NAMES_EXTRA = "NAMES";

    private Activity m_activity;
    private ArrayList<LinearLayout> m_views;
    private ArrayList<DownloadFilesTask> m_dfts;

    public interface DownloadFinishedListener {
        void downloadFinished( ISOCode isoCode, String name, boolean success );
    }

    public interface OnGotLcDictListener {
        void gotDictInfo( boolean success, ISOCode isoCode, String name );
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
        new HashMap<>();

    private class DownloadFilesTask extends AsyncTask<Void, Void, Void>
        implements DictUtils.DownProgListener {
        private String m_savedDict = null;
        private Uri m_uri = null;
        private String m_name;
        private boolean m_isApp = false;
        private File m_appFile = null;
        private int m_totalRead = 0;
        private LinearLayout m_listItem;
        private ProgressBar m_progressBar;

        public DownloadFilesTask( Uri uri, String name, LinearLayout item, boolean isApp )
        {
            super();
            m_uri = uri;
            m_name = name;
            m_isApp = isApp;
            m_listItem = item;
            m_progressBar = (ProgressBar)item.findViewById( R.id.progress_bar );

            if ( isApp ) {
                nukeOldApks();
            }
        }

        public DownloadFilesTask setLabel( String text )
        {
            TextView tv = (TextView)m_listItem.findViewById( R.id.dwnld_message );
            tv.setText( text );
            return this;
        }

        private boolean forApp() { return m_isApp; }

        // Nuke any .apk we downloaded more than 1 week ago
        private void nukeOldApks()
        {
            File apksDir = new File( m_activity.getFilesDir(), APKS_DIR );
            if ( apksDir.exists() ) {
                File[] files = apksDir.listFiles();
                if ( 0 < files.length ) {
                    // 1 week ago
                    final long LAST_MOD_MIN =
                        System.currentTimeMillis() - (1000*60*60*24*7);
                    int nDeleted = 0;
                    for ( File apk : files ) {
                        if ( apk.isFile() ) {
                            long lastMod = apk.lastModified();
                            if ( lastMod < LAST_MOD_MIN ) {
                                boolean gone = apk.delete();
                                Assert.assertTrueNR( gone );
                                if ( gone ) {
                                    ++nDeleted;
                                }
                            }
                        }
                    }
                    if ( BuildConfig.NON_RELEASE && 0 < nDeleted ) {
                        String msg = getString( R.string.old_apks_deleted_fmt,
                                                nDeleted );
                        Log.d( TAG, msg );
                        DbgUtils.showf( msg );
                    }
                }
            }
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
                        @Override
                        public void run() {
                            m_progressBar.setMax( fileLen );
                        }
                    });
                InputStream is = conn.getInputStream();
                String name = null == m_name
                    ? basename( m_uri.getPath() ) : m_name;
                if ( m_isApp ) {
                    Assert.assertTrueNR( null == m_name );
                    m_appFile = saveToPrivate( is, name, this );
                } else {
                    m_savedDict = saveDict( is, name, this );
                    // force a check so BYOD lists will show as custom
                    UpdateCheckReceiver.checkDictVersions( m_activity );
                }
                is.close();
            } catch ( java.net.URISyntaxException use ) {
                Log.ex( TAG, use );
            } catch ( java.net.MalformedURLException mue ) {
                Log.ex( TAG, mue );
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
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
                Intent intent = Utils.makeInstallIntent( m_activity, m_appFile );
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
                    @Override
                    public void run() {
                        m_progressBar.setProgress( m_totalRead );
                    }
                });
        }

        private File saveToPrivate( InputStream is, String name,
                                    DictUtils.DownProgListener dpl)
        {
            File appFile = null;
            boolean success = false;
            byte[] buf = new byte[1024*4];

            try {
                // directory first
                appFile = new File( m_activity.getFilesDir(), APKS_DIR );
                appFile.mkdirs();
                appFile = new File( appFile, name );
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
                Log.ex( TAG, fnf );
            } catch ( java.io.IOException ioe ) {
                Log.ex( TAG, ioe );
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

    @Override
    protected void init( Bundle savedInstanceState )
    {
        m_dfts = new ArrayList<>();
        DownloadFilesTask dft = null;
        Uri[] uris = null;
        LinearLayout item = null;

        Intent intent = getIntent();
        Uri uri = intent.getData(); // launched from Manifest case
        if ( null == uri ) {
            String appUrl = intent.getStringExtra( APK_EXTRA );
            String[] names = null;
            boolean isApp = null != appUrl;
            if ( isApp ) {
                uris = new Uri[] { Uri.parse( appUrl ) };
            } else {
                Parcelable[] parcels = intent.getParcelableArrayExtra( DICTS_EXTRA );
                names = intent.getStringArrayExtra( NAMES_EXTRA );
                uris = new Uri[parcels.length];
                for ( int ii = 0; ii < parcels.length; ++ii ) {
                    uris[ii] = (Uri)(parcels[ii]);
                }
            }
            if ( null != uris ) {
                m_views = new ArrayList<>();
                for ( int ii = 0; ii < uris.length; ++ii ) {
                    item = (LinearLayout)inflate( R.layout.import_dict_item );
                    String name = null == names ? null : names[ii];
                    m_dfts.add( new DownloadFilesTask( uris[ii], name, item, isApp ));
                    m_views.add( item );
                }
            }
        } else if ( (null != intent.getType()
                     && intent.getType().equals( "application/x-xwordsdict" ))
                    || uri.toString().endsWith( XWConstants.DICT_EXTN ) ) {
            item = (LinearLayout)inflate( R.layout.import_dict_item );
            dft = new DownloadFilesTask( uri, null, item, false );
            uris = new Uri[] { uri };
        }

        if ( null != dft ) {
            Assert.assertTrue( 0 == m_dfts.size() );
            m_dfts.add( dft );
            m_views = new ArrayList<>( 1 );
            m_views.add( item );
            dft = null;
        }

        if ( 0 == m_dfts.size() ) {
            finish();
        } else if ( !anyNeedsStorage() ) {
            doWithPermissions( uris );
        } else {
            tryGetPerms( Perm.STORAGE, R.string.download_rationale,
                         Action.STORAGE_CONFIRMED, (Object)uris );
        }
    }

    private void doWithPermissions( Uri[] uris )
    {
        Assert.assertTrue( m_dfts.size() == uris.length );
        mkListAdapter();

        for ( int ii = 0; ii < uris.length; ++ii ) {
            String showName = basename( uris[ii].getPath() );
            showName = DictUtils.removeDictExtn( showName );
            String msg =
                getString( R.string.downloading_dict_fmt, showName );

            m_dfts.get( ii )
                .setLabel( msg )
                .execute();
        }
    } // doWithPermissions

    private boolean anyNeedsStorage()
    {
        boolean result = false;
        DictUtils.DictLoc loc = XWPrefs.getDefaultLoc( m_activity );

        for ( DownloadFilesTask task : m_dfts ) {
            if ( task.forApp() ) {
                // Needn't do anything
            } else if ( DictUtils.DictLoc.DOWNLOAD == loc ) {
                result = true;
                break;
            }
        }
        return result;
    }

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

    @Override
    public boolean onPosButton( Action action, Object... params )
    {
        boolean handled = true;
        switch ( action ) {
        case STORAGE_CONFIRMED:
            doWithPermissions( (Uri[])params[0] );
            break;
        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
    }

    @Override
    public boolean onNegButton( Action action, Object... params )
    {
        boolean handled = true;
        switch ( action ) {
        case STORAGE_CONFIRMED:
            finish();
            break;
        default:
            handled = super.onPosButton( action, params );
        }
        return handled;
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

    private static ISOCode isoCodeFromUri( Uri uri )
    {
        List<String> segs = uri.getPathSegments();
        ISOCode result = new ISOCode(segs.get( segs.size() - 2 ));
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

    private void callListener( Uri uri, boolean success )
    {
        if ( null != uri ) {
            ListenerData ld;
            synchronized( s_listeners ) {
                ld = s_listeners.remove( uri );
            }
            if ( null != ld ) {
                String name = ld.m_name;
                ISOCode isoCode = isoCodeFromUri( uri );
                if ( null == name ) {
                    name = uri.toString();
                }
                ld.m_lstnr.downloadFinished( isoCode, name, success );
            }
        }
    }

    public static void downloadDictInBack( Context context, ISOCode isoCode,
                                           String dictName,
                                           DownloadFinishedListener lstnr )
    {
        Uri uri = Utils.makeDictUriFromCode( context, isoCode, dictName );
        downloadDictInBack( context, uri, dictName, lstnr );
    }

    public static void downloadDictInBack( Context context, Uri uri,
                                           String dictName,
                                           DownloadFinishedListener lstnr )
    {
        Uri[] uris = { uri };
        String[] names = { dictName };
        downloadDictsInBack( context, uris, names, lstnr );
    }

    public static void downloadDictsInBack( Context context, Uri[] uris,
                                            String[] names,
                                            DownloadFinishedListener lstnr )
    {
        // Convert to use http if necessary
        Uri withProto[] = new Uri[uris.length];
        for ( int ii = 0; ii < withProto.length; ++ii ) {
            withProto[ii] = NetUtils.ensureProto( uris[ii] );
        }

        if ( null != lstnr ) {
            for ( int ii = 0; ii < withProto.length; ++ii ) {
                rememberListener( withProto[ii], names[ii], lstnr );
            }
        }

        Intent intent = new Intent( context, DwnldActivity.class );
        intent.putExtra( DICTS_EXTRA, withProto ); // uris implement Parcelable
        intent.putExtra( NAMES_EXTRA, names );
        context.startActivity( intent );
    }

    public static Intent makeAppDownloadIntent( Context context, String url )
    {
        Intent intent = new Intent( context, DwnldActivity.class );
        intent.putExtra( APK_EXTRA, url );
        return intent;
    }
}

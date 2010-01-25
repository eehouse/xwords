/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.app.Activity;
import android.app.ListActivity;
import android.webkit.WebView;
import android.os.Bundle;
import android.webkit.DownloadListener;

import org.eehouse.android.xw4.jni.*;


public class DictActivity extends Activity {

    private WebView m_webview;

    @Override
    public void onCreate( Bundle savedInstanceState )
    {
        super.onCreate( savedInstanceState );

        setContentView( R.layout.dict_browse );

        m_webview = (WebView)findViewById( R.id.dict_web_view );

        m_webview.setDownloadListener( new DownloadListener() {
                public void onDownloadStart( String url, String userAgent, 
                                             String contentDisposition, 
                                             String mimetype, 
                                             long contentLength) {
                    Utils.logf( "url: " + url );
                    Utils.logf( "userAgent: " + userAgent );
                    Utils.logf( "contentDisposition: " + contentDisposition );
                    Utils.logf( "mimetype: " + mimetype );
                    Utils.logf( "contentLength: " + contentLength );
                }
            } );

        m_webview.loadUrl( getString( R.string.dict_url ) );
    }
}

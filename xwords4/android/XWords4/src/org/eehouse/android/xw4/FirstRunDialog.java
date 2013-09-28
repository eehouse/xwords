/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2010 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.AlertDialog;
import android.content.Context;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;

/* Put up a dialog greeting user after every upgrade.  Based on
 * similar feature in OpenSudoku, to whose author "Thanks".
 */

public class FirstRunDialog {
    public static void show( final Context context )
    {
        String page = null;
        InputStream inputStream = null;
        try {
            inputStream = context.getResources()
                .openRawResource(R.raw.changes);
   
            final char[] buf = new char[0x1000];
            StringBuilder stringBuilder = new StringBuilder();
            Reader reader = new InputStreamReader( inputStream, "UTF-8" );
            int nRead;
            do {
                nRead = reader.read( buf, 0, buf.length );
                if ( nRead > 0 ) {
                    stringBuilder.append( buf, 0, nRead );
                }
            } while ( nRead >= 0 );
   
            page = stringBuilder.toString();
        }
        catch ( IOException ioe ) {
            DbgUtils.loge( ioe );
        }
        finally {
            // could just catch NPE....
            if ( null != inputStream ) {
                try {
                    inputStream.close();
                } catch ( IOException ioe ) {
                    DbgUtils.loge( ioe );
                }
            }
        }
  
        // This won't support e.g mailto refs.  Probably want to
        // launch the browser with an intent eventually.
        WebView view = new WebView( context );
        view.setWebViewClient( new WebViewClient() {
                @Override
                public boolean shouldOverrideUrlLoading( WebView view, 
                                                         String url ) {
                    boolean result = false;
                    if ( url.startsWith("mailto:") ){
                        Utils.emailAuthor( context );
                        result = true;
                    }
                    return result;
                }
            });

        view.loadData( page, "text/html", "utf-8" );

        AlertDialog dialog = new AlertDialog.Builder( context )
            .setIcon(android.R.drawable.ic_menu_info_details)
            .setTitle( R.string.changes_title )
            .setView( view )
            .setPositiveButton( R.string.button_ok, null)
            .create();
        dialog.show();
    }
}

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

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.PackageInfo;
import android.app.AlertDialog;
import android.webkit.WebView;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;

/* Put up a dialog greeting user after every upgrade.  Based on
 * similar feature in OpenSudoku, to whose author "Thanks".
 */

public class FirstRunDialog {
	private static final String HIDDEN_PREFS = "xwprefs_hidden";
    private static final String SHOWN_VERSION_KEY = "SHOWN_VERSION_KEY";

    static boolean show( Context context, boolean skipCheck )
    {
        int thisVersion = 0;
        int shownVersion = 0;

        if ( !skipCheck ) {
            try {
                thisVersion = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), 0)
                    .versionCode;
                DbgUtils.logf( "versionCode: %d", thisVersion );
            } catch ( Exception e ) {
            }
        }

        SharedPreferences prefs = null;
        if ( thisVersion > 0 ) {
            prefs = context.getSharedPreferences( HIDDEN_PREFS, 
                                                  Context.MODE_PRIVATE );
            shownVersion = prefs.getInt( SHOWN_VERSION_KEY, 0 );
        }

        boolean isUpgrade = shownVersion < thisVersion;
        if ( skipCheck || isUpgrade ) {
            showDialog( context );
        
            if ( !skipCheck ) {
                Editor editor = prefs.edit();
                editor.putInt( SHOWN_VERSION_KEY, thisVersion );
                editor.commit();
            }
        }
        return isUpgrade;
    }

    private static void showDialog( Context context )
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
			DbgUtils.logf( ioe.toString() );
		}
		finally {
            // could just catch NPE....
			if ( null != inputStream ) {
				try {
					inputStream.close();
				} catch ( IOException ioe ) {
                    DbgUtils.logf( ioe.toString() );
				}
			}
		}
		
        // This won't support e.g mailto refs.  Probably want to
        // launch the browser with an intent eventually.
		WebView view = new WebView( context );
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

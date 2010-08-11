/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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

import android.util.Log;
import java.lang.Thread;
import android.widget.Toast;
import android.content.Context;
import android.content.Intent;
import android.content.DialogInterface;
import android.widget.CheckBox;
import android.app.Activity;
import android.app.Dialog;
import android.app.AlertDialog;
import android.widget.EditText;
import android.widget.TextView;
import android.view.View;
import android.text.format.Time;
import java.util.Formatter;
import android.view.LayoutInflater;
import android.net.Uri;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class Utils {
    static final String TAG = "XW4";

    static final int DIALOG_ABOUT = 1;
    static final int DIALOG_LAST = DIALOG_ABOUT;
    static final String DB_PATH = "XW_GAMES";

    private static Time s_time = new Time();

    private Utils() {}

    public static void logf( String msg ) 
    {
        s_time.setToNow();
        String time = s_time.format("[%H:%M:%S]");
        long id = Thread.currentThread().getId();
        Log.d( TAG, time + "-" + id + "-" + msg );
    } // logf

    public static void logf( String format, Object... args )
    {
        Formatter formatter = new Formatter();
        logf( formatter.format( format, args ).toString() );
    } // logf

    public static void notImpl( Context context ) 
    {
        CharSequence text = "Feature coming soon";
        Toast.makeText( context, text, Toast.LENGTH_SHORT).show();
    }

    static Dialog onCreateDialog( final Context context, int id )
    {
        Assert.assertTrue( DIALOG_ABOUT == id );
        LayoutInflater factory = LayoutInflater.from( context );
        final View view = factory.inflate( R.layout.about_dlg, null );
        TextView vers = (TextView)view.findViewById( R.id.version_string );
        vers.setText( String.format( context.getString(R.string.about_versf), 
                                     XWConstants.VERSION_STR, 
                                     GitVersion.VERS ) );

        TextView xlator = (TextView)view.findViewById( R.id.about_xlator );
        String str = context.getString( R.string.xlator );
        if ( str.length() > 0 ) {
            xlator.setText( str );
        } else {
            xlator.setVisibility( View.GONE );
        }

        return new AlertDialog.Builder( context )
            .setIcon( R.drawable.icon48x48 )
            .setTitle( R.string.app_name )
            .setView( view )
            .setPositiveButton( R.string.changes_button,
                                new DialogInterface.OnClickListener() {
                                    @Override
                                    public void onClick( DialogInterface dlg, 
                                                         int which )
                                    {
                                        FirstRunDialog.show( context, true );
                                    }
                                } )
            .create();
    }

    public static Intent mkDownloadActivity( Context context )
    {
        String dict_url = CommonPrefs.getDefaultDictURL( context );
        Uri uri = Uri.parse( dict_url );
        Intent intent = new Intent( Intent.ACTION_VIEW, uri );
        intent.setFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        return intent;
    }

    public static void setChecked( Activity activity, int id, boolean value )
    {
        CheckBox cbx = (CheckBox)activity.findViewById( id );
        cbx.setChecked( value );
    }

    public static void setChecked( Dialog dialog, int id, boolean value )
    {
        CheckBox cbx = (CheckBox)dialog.findViewById( id );
        cbx.setChecked( value );
    }

    public static void setText( Dialog dialog, int id, String value )
    {
        EditText editText = (EditText)dialog.findViewById( id );
        if ( null != editText ) {
            editText.setText( value, TextView.BufferType.EDITABLE   );
        }
    }

    public static void setText( Activity activity, int id, String value )
    {
        EditText editText = (EditText)activity.findViewById( id );
        if ( null != editText ) {
            editText.setText( value, TextView.BufferType.EDITABLE   );
        }
    }

    public static void setInt( Dialog dialog, int id, int value )
    {
        String str = Integer.toString(value);
        setText( dialog, id, str );
    }

    public static void setInt( Activity activity, int id, int value )
    {
        String str = Integer.toString(value);
        setText( activity, id, str );
    }

    public static boolean getChecked( Activity activity, int id )
    {
        CheckBox cbx = (CheckBox)activity.findViewById( id );
        return cbx.isChecked();
    }

    public static boolean getChecked( Dialog dialog, int id )
    {
        CheckBox cbx = (CheckBox)dialog.findViewById( id );
        return cbx.isChecked();
    }

    public static String getText( Dialog dialog, int id )
    {
        EditText editText = (EditText)dialog.findViewById( id );
        return editText.getText().toString();
    }

    public static String getText( Activity activity, int id )
    {
        EditText editText = (EditText)activity.findViewById( id );
        return editText.getText().toString();
    }

    public static int getInt( Dialog dialog, int id )
    {
        String str = getText( dialog, id );
        try {
            return Integer.parseInt( str );
        } catch ( NumberFormatException nfe ) {
            return 0;
        }
    }

    public static int getInt( Activity activity, int id )
    {
        String str = getText( activity, id );
        try {
            return Integer.parseInt( str );
        } catch ( NumberFormatException nfe ) {
            return 0;
        }
    }
}

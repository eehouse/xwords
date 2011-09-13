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
import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.widget.CheckBox;
import android.widget.Toast;
import android.widget.EditText;
import android.widget.TextView;
import android.view.LayoutInflater;
import android.view.View;
import android.text.format.Time;
import java.util.Formatter;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class Utils {
    static final String TAG = "XW4";

    static final String DB_PATH = "XW_GAMES";

    static boolean s_doLog = true;

    private static Time s_time = new Time();

    private Utils() {}

    public static void logEnable( boolean enable )
    {
        s_doLog = enable;
    }

    public static void logEnable( Context context )
    {
        SharedPreferences sp
            = PreferenceManager.getDefaultSharedPreferences( context );
        String key = context.getString( R.string.key_logging_on );
        boolean on = sp.getBoolean( key, false );
        logEnable( on );
    }

    public static void logf( String msg ) 
    {
        if ( s_doLog ) {
            s_time.setToNow();
            String time = s_time.format("[%H:%M:%S]");
            long id = Thread.currentThread().getId();
            Log.d( TAG, time + "-" + id + "-" + msg );
        }
    } // logf

    public static void logf( String format, Object... args )
    {
        if ( s_doLog ) {
            Formatter formatter = new Formatter();
            logf( formatter.format( format, args ).toString() );
        }
    } // logf

    public static void showf( Context context, String format, Object... args )
    {
        Formatter formatter = new Formatter();
        String msg = formatter.format( format, args ).toString();
        Toast.makeText( context, msg, Toast.LENGTH_SHORT ).show();
    } // showf

    public static void printStack( StackTraceElement[] trace )
    {
        if ( s_doLog ) {
            for ( int ii = 0; ii < trace.length; ++ii ) {
                Utils.logf( "ste %d: %s", ii, trace[ii].toString() );
            }
        }
    }

    public static void printStack()
    {
        if ( s_doLog ) {
            printStack( Thread.currentThread().getStackTrace() );
        }
    }

    public static void notImpl( Context context ) 
    {
        CharSequence text = "Feature coming soon";
        Toast.makeText( context, text, Toast.LENGTH_SHORT).show();
    }

    public static void setRemoveOnDismiss( final Activity activity, 
                                           Dialog dialog, final int id )
    {
        dialog.setOnDismissListener( new DialogInterface.OnDismissListener() {
                public void onDismiss( DialogInterface di ) {
                    activity.removeDialog( id );
                }
            } );
    }

    public static View inflate( Context context, int layoutId )
    {
        LayoutInflater factory = LayoutInflater.from( context );
        return factory.inflate( layoutId, null );
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

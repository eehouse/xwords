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
import android.app.Dialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.DialogInterface;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences.Editor;
import android.content.SharedPreferences;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class Utils {
    private static final String DB_PATH = "XW_GAMES";
	private static final String HIDDEN_PREFS = "xwprefs_hidden";
    private static final String SHOWN_VERSION_KEY = "SHOWN_VERSION_KEY";

    private static Boolean s_isFirstBootThisVersion = null;
    private static Boolean s_isFirstBootEver = null;

    private Utils() {}

    public static boolean firstBootEver( Context context )
    {
        if ( null == s_isFirstBootEver ) {
            setFirstBootStatics( context );
        }
        return s_isFirstBootEver;
    }

    public static boolean firstBootThisVersion( Context context )
    {
        if ( null == s_isFirstBootThisVersion ) {
            setFirstBootStatics( context );
        }
        return s_isFirstBootThisVersion;
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

    public static void emailAuthor( Context context )
    {
        Intent intent = new Intent( Intent.ACTION_SEND );
        intent.setType( "message/rfc822" ); // force email
        intent.putExtra( Intent.EXTRA_SUBJECT,
                         context.getString( R.string.email_author_subject ) );
        String[] addrs = { context.getString( R.string.email_author_email ) };
        intent.putExtra( Intent.EXTRA_EMAIL, addrs );
        String body = format( context, R.string.email_body_revf,
                              GitVersion.VERS );
        intent.putExtra( Intent.EXTRA_TEXT, body );
        String chooserMsg = context.getString( R.string.email_author_chooser );
        context.startActivity( Intent.createChooser( intent, chooserMsg ) );
    }

    public static void postNotification( Context context, Intent intent, 
                                         int titleID, int bodyID, int id )
    {
        postNotification( context, intent, titleID, 
                          context.getString( bodyID ), id );
    }

    public static void postNotification( Context context, Intent intent, 
                                         int titleID, String body, int id )
    {
        PendingIntent pi = PendingIntent.
            getActivity( context, 0, intent, 
                         PendingIntent.FLAG_UPDATE_CURRENT );

        String title = context.getString( titleID );
        Notification notification = 
            new Notification( R.drawable.icon48x48, title,
                              System.currentTimeMillis() );

        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        if ( CommonPrefs.getSoundNotify( context ) ) {
            notification.defaults |= Notification.DEFAULT_SOUND;
        }
        if ( CommonPrefs.getVibrateNotify( context ) ) {
            notification.defaults |= Notification.DEFAULT_VIBRATE;
        }

        notification.setLatestEventInfo( context, title, body, pi );

        NotificationManager nm = (NotificationManager)
            context.getSystemService( Context.NOTIFICATION_SERVICE );
        nm.notify( id, notification );
    }

    public static void cancelNotification( Context context, int id )
    {
        NotificationManager nm = (NotificationManager)
            context.getSystemService( Context.NOTIFICATION_SERVICE );
        nm.cancel( id );
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

    public static void setEnabled( Dialog dialog, int id, boolean enabled )
    {
        View view = dialog.findViewById( id );
        view.setEnabled( enabled );
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

    public static String format( Context context, int id, Object... args )
    {
        String fmt = context.getString( id );
        return String.format( fmt, args );
    }

    private static void setFirstBootStatics( Context context )
    {
        int thisVersion = 0;
        int prevVersion = 0;

        try {
            thisVersion = context.getPackageManager()
                .getPackageInfo(context.getPackageName(), 0)
                .versionCode;
        } catch ( Exception e ) {
        }

        SharedPreferences prefs = null;
        if ( 0 < thisVersion ) {
            prefs = context.getSharedPreferences( HIDDEN_PREFS, 
                                                  Context.MODE_PRIVATE );
            prevVersion = prefs.getInt( SHOWN_VERSION_KEY, -1 );
        }
        boolean newVersion = prevVersion != thisVersion;
        
        s_isFirstBootThisVersion = new Boolean( newVersion );
        s_isFirstBootEver = new Boolean( -1 == prevVersion );

        if ( newVersion ) {
            Editor editor = prefs.edit();
            editor.putInt( SHOWN_VERSION_KEY, thisVersion );
            editor.commit();
        }
    }

}

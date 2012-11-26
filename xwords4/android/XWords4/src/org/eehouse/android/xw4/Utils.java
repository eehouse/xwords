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
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences.Editor;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract.PhoneLookup;
import android.telephony.TelephonyManager;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import java.io.File;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Random;
import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;

public class Utils {
    public static final int TURN_COLOR = 0x7F00FF00;

    private static final String DB_PATH = "XW_GAMES";
	private static final String HIDDEN_PREFS = "xwprefs_hidden";
    private static final String SHOWN_VERSION_KEY = "SHOWN_VERSION_KEY";

    private static Boolean s_isFirstBootThisVersion = null;
    private static Boolean s_deviceSupportSMS = null;
    private static Boolean s_isFirstBootEver = null;
    private static Integer s_appVersion = null;
    private static HashMap<String,String> s_phonesHash = 
        new HashMap<String,String>();
    private static int s_nextCode = 0; // keep PendingIntents unique
    private static Boolean s_hasSmallScreen = null;
    private static Random s_random = new Random();

    private Utils() {}

    public static int nextRandomInt()
    {
        return s_random.nextInt();
    }

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

    // Does the device have ability to send SMS -- e.g. is it a phone
    // and not a Kindle Fire.  Not related to XWApp.SMSSUPPORTED
    public static boolean deviceSupportsSMS( Context context )
    {
        if ( null == s_deviceSupportSMS ) {
            boolean doesSMS = false;
            TelephonyManager tm = (TelephonyManager)
                context.getSystemService(Context.TELEPHONY_SERVICE);
            if ( null != tm ) {
                int type = tm.getPhoneType();
                doesSMS = TelephonyManager.PHONE_TYPE_NONE != type;
            }
            s_deviceSupportSMS = new Boolean( doesSMS );
        }
        return s_deviceSupportSMS;
    }

    public static void notImpl( Context context ) 
    {
        String text = "Feature coming soon";
        showToast( context, text );
    }

    public static void showToast( Context context, String msg )
    {
        Toast.makeText( context, msg, Toast.LENGTH_SHORT).show();
    }

    public static void showToast( Context context, int id )
    {
        String msg = context.getString( id );
        showToast( context, msg );
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

    public static void launchSettings( Context context )
    {
        Intent intent = new Intent( context, PrefsActivity.class );
        context.startActivity( intent );
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
        String title = context.getString( titleID );
        postNotification( context, intent, title, body, id );
    }

    public static void postNotification( Context context, Intent intent, 
                                         String title, String body, int id )
    {
        /* s_nextCode: per this link
           http://stackoverflow.com/questions/10561419/scheduling-more-than-one-pendingintent-to-same-activity-using-alarmmanager
           one way to avoid getting the same PendingIntent for similar
           Intents is to send a different second param each time,
           though the docs say that param's ignored.
        */
        PendingIntent pi = 
            PendingIntent.getActivity( context, ++s_nextCode, intent, 
                                       PendingIntent.FLAG_ONE_SHOT );

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

    // adapted from
    // http://stackoverflow.com/questions/2174048/how-to-look-up-a-contacts-name-from-their-phone-number-on-android
    public static String phoneToContact( Context context, String phone, 
                                         boolean phoneStandsIn )
    {
        // I'm assuming that since context is passed this needn't
        // worry about synchronization -- will always be called from
        // UI thread.
        String name;
        synchronized ( s_phonesHash ) {
            if ( s_phonesHash.containsKey( phone ) ) {
                name = s_phonesHash.get( phone );
            } else {
                name = null;
                ContentResolver contentResolver = context.getContentResolver();
                Cursor cursor =
                    contentResolver
                    .query( Uri.withAppendedPath( PhoneLookup.CONTENT_FILTER_URI, 
                                                  Uri.encode( phone )), 
                            new String[] { PhoneLookup.DISPLAY_NAME }, 
                            null, null, null );
                if ( cursor.moveToNext() ) {
                    int indx = cursor.getColumnIndex( PhoneLookup.DISPLAY_NAME );
                    name = cursor.getString( indx );
                }
                cursor.close();

                s_phonesHash.put( phone, name );
            }
        }
        if ( null == name && phoneStandsIn ) {
            name = phone;
        }
        return name;
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

    public static boolean hasSmallScreen( Context context )
    {
        if ( null == s_hasSmallScreen ) {
            int screenLayout = context.getResources().
                getConfiguration().screenLayout;
            boolean hasSmallScreen = 
                (screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK)
                == Configuration.SCREENLAYOUT_SIZE_SMALL;
            s_hasSmallScreen = new Boolean( hasSmallScreen );
        }
        return s_hasSmallScreen;
    }

    public static String format( Context context, int id, Object... args )
    {
        return context.getString( id, args );
    }

    public static String digestToString( byte[] digest )
    {
        String result = null;
        if ( null != digest ) {
            final char[] hexArray = {'0','1','2','3','4','5','6','7','8','9',
                                     'a','b','c','d','e','f'};
            char[] chars = new char[digest.length * 2];
            for ( int ii = 0; ii < digest.length; ii++ ) {
                int byt = digest[ii] & 0xFF;
                chars[ii * 2] = hexArray[byt >> 4];
                chars[ii * 2 + 1] = hexArray[byt & 0x0F];
            }
            result = new String(chars);
        }
        return result;
    }

    public static long getCurSeconds()
    {
        long millis = new Date().getTime();
        int result = (int)(millis / 1000);
        return result;
    }

    public static String dictFromURL( Context context, String url )
    {
        String result = null;
        int indx = url.lastIndexOf( "/" );
        if ( 0 <= indx ) {
            result = url.substring( indx + 1 );
        }
        return result;
    }

    public static String makeDictUrl( Context context, int lang, String name )
    {
        String dict_url = CommonPrefs.getDefaultDictURL( context );
        if ( 0 != lang ) {
            dict_url += "/" + DictLangCache.getLangName( context, lang );
        }
        if ( null != name ) {
            dict_url += "/" + name + XWConstants.DICT_EXTN;
        }
        return dict_url;
    }

    public static int getAppVersion( Context context )
    {
        if ( null == s_appVersion ) {
            try {
                int version = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), 0)
                    .versionCode;
                s_appVersion = new Integer( version );
            } catch ( Exception e ) {
                DbgUtils.loge( e );
            }
        }
        return null == s_appVersion? 0 : s_appVersion;
    }

    public static Intent makeInstallIntent( File file )
    {
        Uri uri = Uri.parse( "file:/" + file.getPath() );
        Intent intent = new Intent( Intent.ACTION_VIEW );
        intent.setDataAndType( uri, XWConstants.APK_TYPE );
        intent.addFlags( Intent.FLAG_ACTIVITY_NEW_TASK );
        return intent;
    }

    // Return whether there's an app installed that can install
    public static boolean canInstall( Context context, File path )
    {
        boolean result = false;
        PackageManager pm = context.getPackageManager();
        Intent intent = makeInstallIntent( path );
        List<ResolveInfo> doers =
            pm.queryIntentActivities( intent, 
                                      PackageManager.MATCH_DEFAULT_ONLY );
        result = 0 < doers.size();
        DbgUtils.logf( "canInstall()=>%b", result );
        return result;
    }

    private static void setFirstBootStatics( Context context )
    {
        int thisVersion = getAppVersion( context );
        int prevVersion = 0;

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

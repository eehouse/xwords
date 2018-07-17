/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.database.Cursor;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;
import android.provider.ContactsContract.PhoneLookup;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.FileProvider;
import android.telephony.PhoneNumberUtils;
import android.telephony.TelephonyManager;
import android.text.ClipboardManager;
import android.util.Base64;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Random;

import junit.framework.Assert;

import org.eehouse.android.xw4.Perms23.Perm;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.loc.LocUtils;

public class Utils {
    private static final String TAG = Utils.class.getSimpleName();
    public static final int TURN_COLOR = 0x7F00FF00;

    private static final String DB_PATH = "XW_GAMES";
    private static final String HIDDEN_PREFS = "xwprefs_hidden";
    private static final String FIRST_VERSION_KEY = "FIRST_VERSION_KEY";
    private static final String SHOWN_VERSION_KEY = "SHOWN_VERSION_KEY";

    private static Boolean s_isFirstBootThisVersion = null;
    private static Boolean s_firstVersion = null;
    private static Boolean s_isFirstBootEver = null;
    private static Integer s_appVersion = null;
    private static HashMap<String,String> s_phonesHash =
        new HashMap<String,String>();
    private static Boolean s_hasSmallScreen = null;
    private static Random s_random = new Random();

    private Utils() {}

    public static int nextRandomInt()
    {
        return s_random.nextInt();
    }

    public static boolean onFirstVersion( Context context )
    {
        setFirstBootStatics( context );
        return s_firstVersion;
    }

    public static boolean firstBootEver( Context context )
    {
        setFirstBootStatics( context );
        return s_isFirstBootEver;
    }

    public static boolean firstBootThisVersion( Context context )
    {
        setFirstBootStatics( context );
        return s_isFirstBootThisVersion;
    }

    public static boolean isGSMPhone( Context context )
    {
        boolean result = false;
        if ( Perms23.havePermission( Perm.READ_PHONE_STATE ) ) {
            SMSService.SMSPhoneInfo info = SMSService.getPhoneInfo( context );
            result = null != info && info.isPhone && info.isGSM;
        }
        Log.d( TAG, "isGSMPhone() => %b", result );
        return result;
    }

    // Does the device have ability to send SMS -- e.g. is it a phone and not
    // a Kindle Fire.  Not related to XWApp.SMSSUPPORTED.  Note that as a
    // temporary workaround for KitKat having broken use of non-data messages,
    // we only support SMS on kitkat if data messages have been turned on (and
    // that's not allowed except on GSM phones.)
    public static boolean deviceSupportsSMS( Context context )
    {
        boolean result = false;
        if ( Perms23.havePermission( Perm.READ_PHONE_STATE ) ) {
            TelephonyManager tm = (TelephonyManager)
                context.getSystemService( Context.TELEPHONY_SERVICE );
            if ( null != tm ) {
                int type = tm.getPhoneType();
                result = TelephonyManager.PHONE_TYPE_GSM == type;
            }
        }
        Log.d( TAG, "deviceSupportsSMS() => %b", result );
        return result;
    }

    public static void notImpl( Context context )
    {
        String text = "Feature coming soon";
        showToast( context, text );
    }

    public static void showToast( Context context, String msg )
    {
        // Make this safe to call from non-looper threads
        try {
            Toast.makeText( context, msg, Toast.LENGTH_SHORT).show();
        } catch ( java.lang.RuntimeException re ) {
            Log.ex( TAG, re );
        }
    }

    public static void showToast( Context context, int id )
    {
        String msg = LocUtils.getString( context, id );
        showToast( context, msg );
    }

    public static void emailAuthor( Context context )
    {
        Intent intent = new Intent( Intent.ACTION_SEND );
        intent.setType( "message/rfc822" ); // force email
        intent.putExtra( Intent.EXTRA_SUBJECT,
                         LocUtils.getString( context,
                                             R.string.email_author_subject ) );
        String[] addrs = { LocUtils.getString( context,
                                               R.string.email_author_email ) };
        intent.putExtra( Intent.EXTRA_EMAIL, addrs );
        String body = LocUtils.getString( context, R.string.email_body_rev_fmt,
                                          BuildConfig.GIT_REV );
        intent.putExtra( Intent.EXTRA_TEXT, body );
        String chooserMsg = LocUtils.getString( context,
                                                R.string.email_author_chooser );
        context.startActivity( Intent.createChooser( intent, chooserMsg ) );
    }

    static void gitInfoToClip( Context context )
    {
        StringBuilder sb;
        try {
            InputStream is = context.getAssets().open( BuildConfig.BUILD_INFO_NAME,
                                                       AssetManager.ACCESS_BUFFER );
            BufferedReader reader = new BufferedReader(new InputStreamReader(is));
            sb = new StringBuilder();
            for ( ; ; ) {
                String line = reader.readLine();
                if ( null == line ) {
                    break;
                }
                sb.append( line ).append( "\n" );
            }
            reader.close();
        } catch ( Exception ex ) {
            sb = null;
        }

        if ( null != sb ) {
            ClipboardManager clipboard = (ClipboardManager)
                context.getSystemService(Context.CLIPBOARD_SERVICE);
            clipboard.setText( sb.toString() );
        }
    }

    public static void postNotification( Context context, Intent intent,
                                         int titleID, int bodyID, int id )
    {
        postNotification( context, intent, titleID,
                          LocUtils.getString( context, bodyID ), id );
    }

    public static void postNotification( Context context, Intent intent,
                                         int titleID, String body, int id )
    {
        String title = LocUtils.getString( context, titleID );
        postNotification( context, intent, title, body, id );
    }

    public static void postNotification( Context context, Intent intent,
                                         String title, String body,
                                         int id )
    {
        /* nextRandomInt: per this link
           http://stackoverflow.com/questions/10561419/scheduling-more-than-one-pendingintent-to-same-activity-using-alarmmanager
           one way to avoid getting the same PendingIntent for similar
           Intents is to send a different second param each time,
           though the docs say that param's ignored.
        */
        PendingIntent pi = null == intent ? null
            : PendingIntent.getActivity( context, Utils.nextRandomInt(), intent,
                                         PendingIntent.FLAG_ONE_SHOT );

        int defaults = Notification.FLAG_AUTO_CANCEL;
        if ( CommonPrefs.getSoundNotify( context ) ) {
            defaults |= Notification.DEFAULT_SOUND;
        }
        if ( CommonPrefs.getVibrateNotify( context ) ) {
            defaults |= Notification.DEFAULT_VIBRATE;
        }

        Notification notification = new NotificationCompat.Builder( context )
            .setContentIntent( pi )
            .setSmallIcon( R.drawable.notify )
            //.setTicker(body)
            //.setWhen(time)
            .setAutoCancel( true )
            .setDefaults( defaults )
            .setContentTitle( title )
            .setContentText( body )
            .build();

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

    public static void playNotificationSound( Context context )
    {
        if ( CommonPrefs.getSoundNotify( context ) ) {
            Uri uri = RingtoneManager
                .getDefaultUri( RingtoneManager.TYPE_NOTIFICATION );
            Ringtone ringtone = RingtoneManager.getRingtone( context, uri );
            if ( null != ringtone ) {
                ringtone.play();
            }
        }
    }

    // adapted from
    // http://stackoverflow.com/questions/2174048/how-to-look-up-a-contacts-name-from-their-phone-number-on-android
    public static String phoneToContact( Context context, String phone,
                                         boolean phoneStandsIn )
    {
        // I'm assuming that since context is passed this needn't
        // worry about synchronization -- will always be called from
        // UI thread.
        String name = null;
        synchronized ( s_phonesHash ) {
            if ( s_phonesHash.containsKey( phone ) ) {
                name = s_phonesHash.get( phone );
            } else if ( Perms23.havePermission( Perm.READ_CONTACTS ) ) {
                try {
                    ContentResolver contentResolver = context
                        .getContentResolver();
                    Cursor cursor = contentResolver
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
                } catch ( Exception ex ) {
                    // could just be lack of permsisions
                    name = null;
                }
            } else {
                JSONObject phones = XWPrefs.getSMSPhones( context );
                for ( Iterator<String> iter = phones.keys(); iter.hasNext(); ) {
                    String key = iter.next();
                    if ( PhoneNumberUtils.compare( key, phone ) ) {
                        name = phones.optString( key, phone );
                        s_phonesHash.put( phone, name );
                        break;
                    }
                }
            }
        }
        if ( null == name && phoneStandsIn ) {
            name = phone;
        }
        return name;
    }

    public static String capitalize( String str )
    {
        if ( null != str && 0 < str.length() ) {
            str = str.substring( 0, 1 ).toUpperCase() + str.substring( 1 );
        }
        return str;
    }

    public static void setChecked( View parent, int id, boolean value )
    {
        CheckBox cbx = (CheckBox)parent.findViewById( id );
        cbx.setChecked( value );
    }

    public static void setText( View parent, int id, String value )
    {
        EditText editText = (EditText)parent.findViewById( id );
        if ( null != editText ) {
            editText.setText( value, TextView.BufferType.EDITABLE   );
        }
    }

    public static void setInt( View parent, int id, int value )
    {
        String str = Integer.toString(value);
        setText( parent, id, str );
    }

    public static void setEnabled( View parent, int id, boolean enabled )
    {
        View view = parent.findViewById( id );
        parent.setEnabled( enabled );
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

    public static int getInt( Dialog dialog, int id )
    {
        String str = getText( dialog, id );
        try {
            return Integer.parseInt( str );
        } catch ( NumberFormatException nfe ) {
            return 0;
        }
    }

    public static void setItemVisible( Menu menu, int id, boolean enabled )
    {
        MenuItem item = menu.findItem( id );
        if ( null != item ) {
            item.setVisible( enabled );
        }
    }

    public static void setItemEnabled( Menu menu, int id, boolean enabled )
    {
        MenuItem item = menu.findItem( id );
        item.setEnabled( enabled );
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
        // Note: an int is big enough for *seconds* (not milliseconds) since 1970
        // until 2038
        long millis = System.currentTimeMillis();
        int result = (int)(millis / 1000);
        return result;
    }

    public static Uri makeDictUri( Context context, String langName, String name )
    {
        String dictUrl = CommonPrefs.getDefaultDictURL( context );
        Uri.Builder builder = Uri.parse( dictUrl ).buildUpon();
        if ( null != langName ) {
            builder.appendPath( langName );
        }
        if ( null != name ) {
            Assert.assertNotNull( langName );
            builder.appendPath( DictUtils.addDictExtn( name ) );
        }
        return builder.build();
    }

    public static Uri makeDictUri( Context context, int lang, String name )
    {
        String langName = null;
        if ( 0 < lang ) {
            langName = DictLangCache.getLangName( context, lang );
        }
        return makeDictUri( context, langName, name );
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
                Log.ex( TAG, e );
            }
        }
        return null == s_appVersion? 0 : s_appVersion;
    }

    public static Intent makeInstallIntent( Context context, File file )
    {
        Uri uri = FileProvider
            .getUriForFile( context,
                            BuildConfig.APPLICATION_ID + ".provider",
                            file );
        Intent intent = new Intent( Intent.ACTION_VIEW );
        intent.setDataAndType( uri, XWConstants.APK_TYPE );
        intent.addFlags( Intent.FLAG_ACTIVITY_NEW_TASK
                         | Intent.FLAG_GRANT_READ_URI_PERMISSION );
        return intent;
    }

    // Return whether there's an app installed that can install
    public static boolean canInstall( Context context, File path )
    {
        boolean result = false;
        PackageManager pm = context.getPackageManager();
        Intent intent = makeInstallIntent( context, path );
        List<ResolveInfo> doers =
            pm.queryIntentActivities( intent,
                                      PackageManager.MATCH_DEFAULT_ONLY );
        result = 0 < doers.size();
        return result;
    }

    public static View getContentView( Activity activity )
    {
        return activity.findViewById( android.R.id.content );
    }

    public static boolean isGooglePlayApp( Context context )
    {
        PackageManager pm = context.getPackageManager();
        String packageName = context.getPackageName();
        String installer = pm.getInstallerPackageName( packageName );
        boolean result = "com.google.android.feedback".equals( installer )
            || "com.android.vending".equals( installer );
        return result;
    }

    public static String base64Encode( byte[] in )
    {
        return Base64.encodeToString( in, Base64.NO_WRAP );
    }

    public static byte[] base64Decode( String in )
    {
        return Base64.decode( in, Base64.NO_WRAP );
    }

    public static Object string64ToSerializable( String str64 )
    {
        Object result = null;
        byte[] bytes = base64Decode( str64 );
        try {
            ObjectInputStream ois =
                new ObjectInputStream( new ByteArrayInputStream(bytes) );
            result = ois.readObject();
        } catch ( Exception ex ) {
            Log.d( TAG, ex.getMessage() );
        }
        return result;
    }

    public static String serializableToString64( Serializable obj )
    {
        String result = null;
        ByteArrayOutputStream bas = new ByteArrayOutputStream();
        try {
            ObjectOutputStream out = new ObjectOutputStream( bas );
            out.writeObject( obj );
            out.flush();
            result = base64Encode( bas.toByteArray() );
        } catch ( Exception ex ) {
            Log.ex( TAG, ex );
            Assert.assertFalse( BuildConfig.DEBUG );
        }
        return result;
    }

    public static void testSerialization( Serializable obj )
    {
        if ( BuildConfig.DEBUG ) {
            String as64 = serializableToString64( obj );
            Object other = string64ToSerializable( as64 );
            Assert.assertTrue( other.equals( obj ) );
            Log.d( TAG, "testSerialization(%s) worked!!!", obj );
        }
    }

    private static void setFirstBootStatics( Context context )
    {
        if ( null == s_isFirstBootThisVersion ) {
            final int thisVersion = getAppVersion( context );
            int prevVersion = 0;
            SharedPreferences prefs =
                context.getSharedPreferences( HIDDEN_PREFS,
                                              Context.MODE_PRIVATE );


            if ( 0 < thisVersion ) {
                prefs = context.getSharedPreferences( HIDDEN_PREFS,
                                                      Context.MODE_PRIVATE );
                prevVersion = prefs.getInt( SHOWN_VERSION_KEY, -1 );
            }
            boolean newVersion = prevVersion != thisVersion;

            s_isFirstBootThisVersion = new Boolean( newVersion );
            s_isFirstBootEver = new Boolean( -1 == prevVersion );

            int firstVersion = prefs.getInt( FIRST_VERSION_KEY, Integer.MAX_VALUE );
            s_firstVersion = new Boolean( firstVersion >= thisVersion );
            if ( newVersion || Integer.MAX_VALUE == firstVersion ) {
                SharedPreferences.Editor editor = prefs.edit();
                if ( newVersion ) {
                    editor.putInt( SHOWN_VERSION_KEY, thisVersion );
                }
                if ( Integer.MAX_VALUE == firstVersion ) {
                    editor.putInt( FIRST_VERSION_KEY, thisVersion );
                }
                editor.commit();
            }
        }
    }

}

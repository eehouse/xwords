/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

package org.eehouse.android.xw4.loc;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.TextView;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONObject;

import junit.framework.Assert;

import org.eehouse.android.xw4.DBUtils;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Utils;
import org.eehouse.android.xw4.XWPrefs;

public class LocUtils {
    // Keep this in sync with gen_loc_ids.py and what's used in the menu.xml
    // files to mark me-localized strings.
    private static final int FMT_LEN = 4;
    private static final String k_LOCALE = "locale";
    private static final String k_XLATPROTO = "proto";
    private static final int XLATE_CUR_VERSION = 1;
    private static final String k_XLATEVERS = "xlatevers";

    private static Map<String, String> s_xlationsLocal = null;
    private static Map<String, String> s_xlationsBlessed = null;
    private static HashMap<Integer, String> s_idsToKeys = null;
    private static Boolean s_enabled = null;
    private static Boolean UPPER_CASE = false;
    private static String s_curLocale;

    public interface LocIface {
        void setText( CharSequence text );
    }

    // public static void loadStrings( Context context, AttributeSet as, LocIface view )
    // {
    //     // There should be a way to look up the index of "strid" but I don't
    //     // have it yet.  This got things working.
    //     int count = as.getAttributeCount();
    //     for ( int ii = 0; ii < count; ++ii ) {
    //         if ( "strid".equals( as.getAttributeName(ii) ) ) {
    //             String value = as.getAttributeValue(ii);
    //             Assert.assertTrue( '@' == value.charAt(0) );
    //             int id = Integer.parseInt( value.substring(1) );
    //             view.setText( getString( context, id ) );
    //             break;
    //         }
    //     }
    // }

    public static void localeChanged( Context context, String newLocale )
    {
        saveLocalData( context );
        s_curLocale = newLocale;
        s_xlationsLocal = null;
        s_xlationsBlessed = null;
        s_enabled = null;
    }

    public static View inflate( Context context, int resID )
    {
        LayoutInflater factory = LayoutInflater.from( context );
        View view = factory.inflate( resID, null );
        xlateView( context, view );
        return view;
    }

    public static void xlateTitle( Activity activity )
    {
        String title = activity.getTitle().toString();
        String xlated = xlateString( activity, title );
        if ( ! title.equals(xlated) ) {
            activity.setTitle( xlated );
        }
    }

    public static void xlateView( Activity activity )
    {
        xlateView( activity, Utils.getContentView( activity ) );
    }

    public static void xlateView( Context context, View view )
    {
        DbgUtils.logf( "xlateView() top level" );
        xlateView( context, view, 0 );
    }

    public static void xlateMenu( Activity activity, Menu menu )
    {
        xlateMenu( activity, menu, 0 );
    }

    public static String xlateString( Context context, String str )
    {
        if ( LocIDs.getS_MAP( context ).containsKey( str ) ) {
            String xlation = getXlation( context, true, str );
            if ( null != xlation ) {
                str = xlation;
            }
        }
        return str;
    }

    public static CharSequence[] xlateStrings( Context context, CharSequence[] strs )
    {
        CharSequence[] result = new CharSequence[strs.length];
        for ( int ii = 0; ii < strs.length; ++ii ) {
            result[ii] = xlateString( context, strs[ii].toString() );
        }
        return result;
    }

    public static String[] getStringArray( Context context, int resID )
    {
        Resources res = context.getResources();
        String[] arr = res.getStringArray( resID );
        return xlateStrings( context, arr );
    }

    public static String[] xlateStrings( Context context, String[] strs )
    {
        String[] result = new String[strs.length];
        for ( int ii = 0; ii < strs.length; ++ii ) {
            result[ii] = xlateString( context, strs[ii].toString() );
        }
        return result;
    }

    public static String getString( Context context, int id ) 
    {
        return  getString( context, true, id );
    }

    public static String getString( Context context, boolean canUseDB, int id )
    {
        String result = null;
        String key = keyForID( context, id );
        if ( null != key ) {
            result = getXlation( context, canUseDB, key );
        }
        
        if ( null == result ) {
            result = context.getString( id );
        }

        return result;
    }

    public static String getString( Context context, int id, Object... params )
    {
        Assert.assertNotNull( params );
        String result = getString( context, id );
        if ( null != result ) {
            result = String.format( result, params );
        }
        return result;
    }

    public static void setXlation( Context context, String key, String txt )
    {
        loadXlations( context );
        s_xlationsLocal.put( key, txt );
    }

    public static String getXlation( Context context, boolean canUseDB, 
                                     String key )
    {
        if ( canUseDB ) {
            loadXlations( context );
        }
        String result = null;
        if ( null != s_xlationsLocal ) {
            result = s_xlationsLocal.get( key );
        }
        if ( null == result && null != s_xlationsBlessed ) {
            result = s_xlationsBlessed.get( key );
        }
        if ( UPPER_CASE && null == result ) {
            result = toUpperCase( key );
        }
        return result;
    }

    public static void saveLocalData( Context context )
    {
        DBUtils.saveXlations( context, s_curLocale, s_xlationsLocal, false );
    }

    public static JSONObject makeForXlationUpdate( Context context )
    {
        JSONObject result = null;
        if ( null != s_curLocale && 0 < s_curLocale.length() ) {
            try {
                String version = DBUtils.getStringFor( context, k_XLATEVERS, "0" );
                result = new JSONObject()
                    .put( k_XLATPROTO, XLATE_CUR_VERSION )
                    .put( k_LOCALE, s_curLocale )
                    .put( k_XLATEVERS, version );
            } catch ( org.json.JSONException jse ) {
                DbgUtils.loge( jse );
            }
        }
        return result;
    }

    private static final String k_OLD = "old";
    private static final String k_NEW = "new";
    private static final String k_PAIRS = "pairs";

    public static int addXlation( Context context, JSONObject data )
    {
        int nAdded = 0;
        try {
            int newVersion = data.getInt( k_NEW );
            JSONArray pairs = data.getJSONArray( k_PAIRS );
            DbgUtils.logf( "got pairs of len %d, version %d", pairs.length(), 
                           newVersion );

            int len = pairs.length();
            Map<String,String> newXlations = new HashMap<String,String>( len );
            for ( int ii = 0; ii < len; ++ii ) {
                JSONObject pair = pairs.getJSONObject( ii );
                String key = pair.getString( "en" );
                String txt = pair.getString( "loc" );
                newXlations.put( key, txt );
            }

            DBUtils.saveXlations( context, s_curLocale, newXlations, true );
            DBUtils.setStringFor( context, k_XLATEVERS, 
                                  String.format( "%d", newVersion ) );

            s_xlationsBlessed = null;
            loadXlations( context );
            nAdded = len;
        } catch ( org.json.JSONException jse ) {
            DbgUtils.loge( jse );
        }
        return nAdded;
    }

    protected static LocSearcher.Pair[] makePairs( Context context )
    {
        loadXlations( context );

        Map<String,Integer> map = LocIDs.getS_MAP( context );
        int siz = map.size();
        LocSearcher.Pair[] result = new LocSearcher.Pair[siz];
        Iterator<String> iter = map.keySet().iterator();

        for ( int ii = 0; iter.hasNext(); ++ii ) {
            String key = iter.next();
            String english = context.getString( map.get( key ) );
            Assert.assertTrue( english.equals( key ) );
            String xlation = getXlation( context, true, key );
            result[ii] = new LocSearcher.Pair( key, english, xlation );
        }
        return result;
    }

    private static void xlateMenu( final Activity activity, Menu menu, 
                                   int depth )
    {
        int count = menu.size();
        for ( int ii = 0; ii < count; ++ii ) {
            MenuItem item = menu.getItem( ii );
            CharSequence ts = item.getTitle();
            if ( null != ts ) {
                String title = ts.toString();
                if ( LocIDs.getS_MAP( activity ).containsKey(title) ) {
                    title = xlateString( activity, title );
                    item.setTitle( title );
                }
            }

            if ( item.hasSubMenu() ) {
                xlateMenu( activity, item.getSubMenu(), 1 + depth );
            }
        }

        // The caller is loc-aware, so add our menu -- at the top level!
        if ( 0 == depth ) {
            String title = getString( activity, R.string.loc_menu_xlate );
            menu.add( title )
                .setOnMenuItemClickListener( new OnMenuItemClickListener() {
                        public boolean onMenuItemClick( MenuItem item ) {
                            Intent intent = 
                                new Intent( activity, LocActivity.class );
                            activity.startActivity( intent );
                            return true;
                        } 
                    });
        }
    }

    private static void loadXlations( Context context )
    {
        if ( null == s_curLocale ) {
            s_curLocale = XWPrefs.getLocale( context );
        }
        if ( null == s_xlationsLocal || null == s_xlationsBlessed ) {
            Object[] asObjs = DBUtils.getXlations( context, s_curLocale );
            s_xlationsLocal = (Map<String,String>)asObjs[0];
            s_xlationsBlessed = (Map<String,String>)asObjs[1];
            DbgUtils.logf( "loadXlations: got %d local strings, %d blessed strings",
                           s_xlationsLocal.size(),
                           s_xlationsBlessed.size() );
        }
    }

    private static String keyForID( Context context, int id )
    {
        if ( null == s_idsToKeys ) {
            Map<String,Integer> map = LocIDs.getS_MAP( context );
            HashMap<Integer, String> idsToKeys =
                new HashMap<Integer, String>( map.size() );

            Iterator<String> iter = map.keySet().iterator();
            while ( iter.hasNext() ) {
                String key = iter.next();
                idsToKeys.put( map.get( key ), key );
            }
            s_idsToKeys = idsToKeys;
        }

        return s_idsToKeys.get( id );
    }

    private static boolean isEnabled( Context context )
    {
        if ( null == s_enabled ) {
            s_curLocale = XWPrefs.getLocale( context );
            s_enabled = new Boolean( null != s_curLocale && 
                                     0 < s_curLocale.length() );
        }
        return s_enabled;
    }

    private static void xlateView( Context context, View view, int depth )
    {
        // DbgUtils.logf( "xlateView(depth=%d, view=%s, canRecurse=%b)", depth, 
        //                view.getClass().getName(), view instanceof ViewGroup );
        if ( view instanceof Button ) {
            Button button = (Button)view;
            String str = xlateString( context, button.getText().toString() );
            button.setText( str );
        } else if ( view instanceof TextView ) {
            TextView tv = (TextView)view;
            tv.setText( xlateString( context, tv.getText().toString() ) );
        // } else if ( view instanceof CheckBox ) {
        //     CheckBox box = (CheckBox)view;
        //     String str = box.getText().toString();
        //     str = xlateString( context, str );
        //     box.setText( str );
        } else if ( view instanceof Spinner ) {
            Spinner sp = (Spinner)view;
            CharSequence prompt = sp.getPrompt();
            if ( null != prompt ) {
                String xlation = xlateString( context, prompt.toString() );
                if ( null != xlation ) {
                    sp.setPrompt( xlation );
                }
            }
        }

        // A Spinner, for instance, ISA ViewGroup, so this is a separate test.
        if ( view instanceof ViewGroup ) { 
            ViewGroup asGroup = (ViewGroup)view;
            int count =	asGroup.getChildCount();
            for ( int ii = 0; ii < count; ++ii ) {
                View child = asGroup.getChildAt( ii );
                xlateView( context, child, depth + 1 );
            }
        }
    }

    // This is for testing, but the ability to pull the formatters will be
    // critical for validating local transations of strings containing
    // formatters.
    private static String toUpperCase( String str )
    {
        String[] parts = str.split( "%[\\d]\\$[ds]" );
        StringBuilder sb = new StringBuilder();
        int offset = 0;
        for ( String part : parts ) {
            sb.append( part.toUpperCase() );
            offset += part.length();
            if ( offset < str.length() ) {
                sb.append( str.substring( offset, offset + FMT_LEN ) );
                offset += FMT_LEN;
            }
        }
        return sb.toString();
    }

    public static AlertBuilder makeAlertBuilder( Context context )
    {
        return new AlertBuilder( context );
    }

    public static class AlertBuilder extends AlertDialog.Builder {
        Context m_context;

        private AlertBuilder( Context context )
        {
            super( context );
            m_context = context;
        }

        @Override
        public AlertDialog.Builder setTitle( int id )
        {
            String str = getString( m_context, id );
            return setTitle( str );
        }

        public AlertDialog.Builder setMessage( int textId )
        {
            String str = getString( m_context, textId );
            return setMessage( str );
        }

        @Override
        public AlertDialog.Builder setPositiveButton( int textId, 
                                                      OnClickListener listener )
        {
            String str = getString( m_context, textId );
            return setPositiveButton( str, listener );
        }
        @Override
        public AlertDialog.Builder setNeutralButton( int textId, 
                                                     OnClickListener listener )
        {
            String str = getString( m_context, textId );
            return setNeutralButton( str, listener );
        }

        @Override
        public AlertDialog.Builder setNegativeButton( int textId, 
                                                      OnClickListener listener )
        {
            String str = getString( m_context, textId );
            return setNegativeButton( str, listener );
        }
    } 

}

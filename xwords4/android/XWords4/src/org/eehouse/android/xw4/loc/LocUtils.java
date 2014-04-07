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
import android.content.Context;
import android.content.Intent;
import android.util.AttributeSet;
import android.view.Menu;
import android.view.MenuItem.OnMenuItemClickListener;
import android.view.MenuItem;

import java.util.Iterator;
import java.util.HashMap;
import java.util.Map;

import junit.framework.Assert;

import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.DbgUtils;
import org.eehouse.android.xw4.DBUtils;

public class LocUtils {
    // Keep this in sync with gen_loc_ids.py and what's used in the menu.xml
    // files to mark me-localized strings.
    private static final String LOC_PREFIX = "loc:";
    private static HashMap<String, String>s_xlations = null;
    private static HashMap<Integer, String> s_idsToKeys = null;

    public interface LocIface {
        void setText( CharSequence text );
    }

    public static void loadStrings( Context context, AttributeSet as, LocIface view )
    {
        // There should be a way to look up the index of "strid" but I don't
        // have it yet.  This got things working.
        int count = as.getAttributeCount();
        for ( int ii = 0; ii < count; ++ii ) {
            if ( "strid".equals( as.getAttributeName(ii) ) ) {
                String value = as.getAttributeValue(ii);
                Assert.assertTrue( '@' == value.charAt(0) );
                int id = Integer.parseInt( value.substring(1) );
                view.setText( getString( context, id ) );
                break;
            }
        }
    }

    public static void xlateMenu( Activity activity, Menu menu )
    {
        xlateMenu( activity, menu, 0 );
    }

    public static String xlateString( Context context, String str )
    {
        if ( str.startsWith( LOC_PREFIX ) ) {
            str = str.substring( LOC_PREFIX.length() );
            int id = LocIDs.getID( str );
            if ( LocIDs.NOT_FOUND != id ) {
                str = getString( context, id );
            } else {
                DbgUtils.logf( "nothing for %s", str );
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

    public static String getString( Context context, int id )
    {
        return getString( context, id, (Object)null );
    }

    public static String getString( Context context, int id, Object... params )
    {
        String result = null;
        String key = keyForID( id );
        if ( null != key ) {
            result = getXlation( context, key );
        }
        
        if ( null == result ) {
            result = context.getString( id );
        }

        if ( null != result && null != params ) {
            result = String.format( result, params );
        }
        return result;
    }

    public static void setXlation( Context context, String key, String txt )
    {
        loadXlations( context );
        s_xlations.put( key, txt );
    }

    public static String getXlation( Context context, String key )
    {
        loadXlations( context );
        String result = s_xlations.get( key );
        return result;
    }

    public static void saveData( Context context )
    {
        DBUtils.saveXlations( context, "te_ST", s_xlations );
    }


    protected static LocSearcher.Pair[] makePairs( Context context )
    {
        loadXlations( context );

        Map<String,Integer> map = LocIDsData.S_MAP;
        int siz = map.size();
        LocSearcher.Pair[] result = new LocSearcher.Pair[siz];
        Iterator<String> iter = map.keySet().iterator();

        for ( int ii = 0; iter.hasNext(); ++ii ) {
            String key = iter.next();
            String english = context.getString( map.get( key ) );
            String xlation = s_xlations.get( key );
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
                if ( title.startsWith( LOC_PREFIX ) ) {
                    String asKey = title.substring( LOC_PREFIX.length() );
                    int id = LocIDs.getID( asKey );
                    if ( LocIDs.NOT_FOUND != id ) {
                        asKey = getString( activity, id );
                    } else {
                        DbgUtils.logf( "nothing for %s", asKey );
                    }
                    item.setTitle( asKey );
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
        if ( null == s_xlations ) {
            s_xlations = DBUtils.getXlations( context, "te_ST" );
        }
    }

    private static String keyForID( int id )
    {
        if ( null == s_idsToKeys ) {
            Map<String,Integer> map = LocIDsData.S_MAP;
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
}

/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2016 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import junit.framework.Assert;
    
public class Perms23 {
    private static final String TAG = Perms23.class.getSimpleName();
    
    public static enum Perm {
        READ_PHONE_STATE("android.permission.READ_PHONE_STATE"),
        STORAGE("android.permission.WRITE_EXTERNAL_STORAGE"),
        SEND_SMS("android.permission.SEND_SMS"),
        READ_CONTACTS("android.permission.READ_CONTACTS")
        ;

        private String m_str;
        private Perm(String str) { m_str = str; }
        public String getString() { return m_str; }
        public static Perm getFor( String str ) {
            Perm result = null;
            for ( Perm one : Perm.values() ) {
                if ( one.getString().equals( str ) ) {
                    result = one;
                    break;
                }
            }
            return result;
        }
    }

    public interface PermCbck {
        void onPermissionResult( Map<Perm, Boolean> perms );
    }
    public interface OnShowRationale {
        void onShouldShowRationale( Set<Perms23.Perm> perms );
    }

    public static class Builder {
        private Set<Perm> m_perms = new HashSet<Perm>();
        private OnShowRationale m_onShow;

        public Builder(Set<Perm> perms) {
            m_perms.addAll( perms );
        }

        public Builder( Perm perm ) {
            m_perms.add( perm );
        }

        public Builder add( Perm perm ) {
            m_perms.add( perm );
            return this;
        }

        public Builder setOnShowRationale( OnShowRationale onShow )
        {
            m_onShow = onShow;
            return this;
        }

        public void asyncQuery( Activity activity )
        {
            asyncQuery( activity, null );
        }

        public void asyncQuery( Activity activity, PermCbck cbck )
        {
            DbgUtils.logd( TAG, "asyncQuery(%s)", m_perms.toString() );
            boolean haveAll = true;
            boolean shouldShow = false;
            Set<Perm> needShow = new HashSet<Perm>();

            ArrayList<String> askStrings = new ArrayList<String>();
            for ( Perm perm : m_perms ) {
                String permStr = perm.getString();
                boolean haveIt = PackageManager.PERMISSION_GRANTED
                    == ContextCompat.checkSelfPermission( activity, permStr );

                // For research: ask the OS if we should be printing a rationale
                if ( !haveIt ) {
                    askStrings.add( permStr );

                    if ( ActivityCompat
                         .shouldShowRequestPermissionRationale( activity,
                                                                permStr ) ) {
                        needShow.add( perm );
                    }
                }

                haveAll = haveAll && haveIt;
            }

            if ( 0 < needShow.size() && null != m_onShow ) {
                m_onShow.onShouldShowRationale( needShow );
            } else if ( haveAll ) {
                if ( null != cbck ) {
                    Map<Perm, Boolean> map = new HashMap<Perm, Boolean>();
                    for ( Perm perm : m_perms ) {
                        map.put( perm, true );
                    }
                    cbck.onPermissionResult( map );
                }
            } else {
                String[] permsArray = askStrings.toArray( new String[askStrings.size()] );
                int code = register( cbck );
                ActivityCompat.requestPermissions( activity, permsArray, code );
            }

            DbgUtils.logd( TAG, "asyncQuery(%s) DONE", m_perms.toString() );
        }
    }

    private static Map<Integer, PermCbck> s_map = new HashMap<Integer, PermCbck>();
    public static void gotPermissionResult( int code, String[] perms, int[] granteds )
    {
        PermCbck cbck = s_map.get( code );
        if ( null != cbck ) {
            Map<Perm, Boolean> result = new HashMap<Perm, Boolean>();
            for ( int ii = 0; ii < perms.length; ++ii ) {
                Perm perm = Perm.getFor( perms[ii] );
                boolean granted = PackageManager.PERMISSION_GRANTED == granteds[ii];
                result.put( perm, granted );
                // DbgUtils.logd( TAG, "calling %s.onPermissionResult(%s, %b)",
                //                record.cbck.getClass().getSimpleName(), perm.toString(),
                //                granted );
            }
            cbck.onPermissionResult( result );
        }
    }

    public static boolean havePermission( Perm perm )
    {
        String permString = perm.getString();
        boolean result = PackageManager.PERMISSION_GRANTED
            == ContextCompat.checkSelfPermission( XWApp.getContext(), permString );
        DbgUtils.logd( TAG, "havePermission(%s) => %b", perm.toString(), result );
        return result;
    }

    private static int s_nextRecord;
    private static int register( PermCbck cbck )
    {
        DbgUtils.assertOnUIThread();
        int code = ++s_nextRecord;
        s_map.put( code, cbck );
        return code;
    }
}

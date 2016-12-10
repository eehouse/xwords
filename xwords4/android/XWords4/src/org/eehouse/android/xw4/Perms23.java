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

import java.util.HashMap;
import java.util.Map;
    
public class Perms23 {
    private static final String TAG = Perms23.class.getSimpleName();
    
    public static enum Perm {
        READ_PHONE_STATE("android.permission.READ_PHONE_STATE"),
        STORAGE("android.permission.WRITE_EXTERNAL_STORAGE")
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
        void onPermissionResult( Perm perm, boolean granted );
    }

    public static void gotPermissionResult( int code, String[] perms, int[] granteds )
    {
        CbckRecord record = s_map.get( code );
        for ( int ii = 0; ii < perms.length; ++ii ) {
            Perm perm = Perm.getFor( perms[ii] );
            boolean granted = PackageManager.PERMISSION_GRANTED == granteds[ii];
            DbgUtils.logd( TAG, "calling %s.onPermissionResult(%s, %b)",
                           record.cbck.getClass().getSimpleName(), perm.toString(),
                           granted );
            record.cbck.onPermissionResult( perm, granted );
        }
    }

    public static void doWithPermission( Activity activity, Perm perm, PermCbck cbck )
    {
        DbgUtils.logd( TAG, "doWithPermission()" );
        String permStr = perm.getString();

        if ( PackageManager.PERMISSION_GRANTED == ContextCompat.checkSelfPermission( activity, permStr ) ) {
            DbgUtils.logd( TAG, "doWithPermission(): already have it" );
            cbck.onPermissionResult( perm, true );
        } else {
            // Should we show an explanation?
            boolean shouldShow = ActivityCompat
                .shouldShowRequestPermissionRationale( activity, permStr );
            if ( shouldShow && BuildConfig.DEBUG ) {
                DbgUtils.logd( TAG, "should show rationalle!!!" );
            }
            //  Show an explanation to the user *asynchronously* -- don't
            //                 block // this thread waiting for the user's
            //                 response! After the user // sees the
            //                 explanation, try again to request the
            //                 permission.

            String[] perms = new String[]{ permStr };
            int code = register( perms, cbck );
            ActivityCompat.requestPermissions( activity, perms, code );
        }
        
        // int check = ContextCompat.checkSelfPermission( activity, permStr );
        // if ( PackageManager.PERMISSION_GRANTED == check ) {
        //     if ( null != onSuccess ) {
        //         onSuccess.run();
        //     }
        // } else {
        //     Assert.assertTrue( PackageManager.PERMISSION_DENIED == check );
        // }
    }

    public static boolean havePermission( Perm perm )
    {
        String permString = perm.getString();
        boolean result = PackageManager.PERMISSION_GRANTED
            == ContextCompat.checkSelfPermission( XWApp.getContext(), permString );
        DbgUtils.logd( TAG, "havePermission(%s) => %b", perm.toString(), result );
        return result;
    }

    private static class CbckRecord {
        public PermCbck cbck;
        public String[] perms;
        public CbckRecord( String[] perms, PermCbck cbck ) {
            this.perms = perms;
            this.cbck = cbck;
        }
    }

    private static int s_nextRecord;
    private static Map<Integer, CbckRecord> s_map = new HashMap<Integer, CbckRecord>();
    private static int register( String[] perms, PermCbck cbck )
    {
        DbgUtils.assertOnUIThread();
        int code = ++s_nextRecord;
        CbckRecord record = new CbckRecord( perms, cbck );
        s_map.put( code, record );
        return code;
    }
}

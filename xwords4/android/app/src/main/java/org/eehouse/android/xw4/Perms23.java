/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.loc.LocUtils;

    
public class Perms23 {
    private static final String TAG = Perms23.class.getSimpleName();
    
    public static enum Perm {
        READ_PHONE_STATE(Manifest.permission.READ_PHONE_STATE),
        STORAGE(Manifest.permission.WRITE_EXTERNAL_STORAGE),
        SEND_SMS(Manifest.permission.SEND_SMS, BuildConfig.SMS_BANNED_EXPL),
        RECEIVE_SMS(Manifest.permission.RECEIVE_SMS, BuildConfig.SMS_BANNED_EXPL),
        READ_CONTACTS(Manifest.permission.READ_CONTACTS);

        private String m_str;
        private int[] m_expl;
        private Perm(String str) { this(str, null); }
        private Perm(String str, int[] bannedExpl) {
            m_str = str;
            m_expl = bannedExpl;
        }

        public String getString() { return m_str; }
        public boolean isBanned() { return m_expl != null; }
        public int[] getExpl() { Assert.assertTrue(isBanned()); return m_expl; }
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
        void onShouldShowRationale( Set<Perm> perms );
    }

    public static class Builder {
        private Set<Perm> m_perms = new HashSet<Perm>();
        private OnShowRationale m_onShow;

        public Builder(Set<Perm> perms) {
            m_perms.addAll( perms );
        }

        public Builder( Perm[] perms ) {
            for ( Perm perm : perms ) {
                m_perms.add( perm );
            }
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
            Log.d( TAG, "asyncQuery(%s)", m_perms.toString() );
            boolean haveAll = true;
            boolean shouldShow = false;
            Set<Perm> needShow = new HashSet<Perm>();

            ArrayList<String> askStrings = new ArrayList<String>();
            for ( Perm perm : m_perms ) {
                String permStr = perm.getString();
                boolean haveIt = PackageManager.PERMISSION_GRANTED
                    == ContextCompat.checkSelfPermission( activity, permStr );

                if ( !haveIt ) {
                    askStrings.add( permStr );

                    if ( null != m_onShow && ActivityCompat
                         .shouldShowRequestPermissionRationale( activity,
                                                                permStr ) ) {
                        needShow.add( perm );
                    }
                }

                haveAll = haveAll && haveIt;
            }

            if ( haveAll ) {
                if ( null != cbck ) {
                    Map<Perm, Boolean> map = new HashMap<>();
                    for ( Perm perm : m_perms ) {
                        map.put( perm, true );
                    }
                    cbck.onPermissionResult( map );
                }
            } else if ( 0 < needShow.size() && null != m_onShow ) {
                // Log.d( TAG, "calling onShouldShowRationale()" );
                m_onShow.onShouldShowRationale( needShow );
            } else {
                String[] permsArray = askStrings.toArray( new String[askStrings.size()] );
                int code = register( cbck );
                // Log.d( TAG, "calling requestPermissions on %s",
                //                activity.getClass().getSimpleName() );
                ActivityCompat.requestPermissions( activity, permsArray, code );
            }
        }
    }

    private static class QueryInfo {
        private Action m_action;
        private Perm[] m_perms;
        private DelegateBase m_delegate;
        private String m_rationaleMsg;
        private Object[] m_params;

        private QueryInfo( DelegateBase delegate, Action action,
                           Perm[] perms, String msg, Object[] params ) {
            m_delegate = delegate;
            m_action = action;
            m_perms = perms;
            m_rationaleMsg = msg;
            m_params = params;
        }

        private QueryInfo( DelegateBase delegate, Object[] params )
        {
            this( delegate, (Action)params[0], (Perm[])params[1], (String)params[2],
                  (Object[])params[3] );
        }

        private Object[] getParams()
        {
            return new Object[] { m_action, m_perms, m_rationaleMsg, m_params };
        }

        private void doIt( boolean showRationale )
        {
            Set<Perm> validPerms = new HashSet<>();
            Set<Perm> bannedPerms = new HashSet<>();
            for ( Perm perm : m_perms ) {
                if ( perm.isBanned() ) {
                    bannedPerms.add( perm );
                } else {
                    validPerms.add( perm );
                }
            }

            if ( 0 < validPerms.size() ) {
                doItAsk( validPerms, showRationale );
            }
            if ( 0 < bannedPerms.size() ) {
                doItFail( bannedPerms );
            }
        }

        private void doItAsk( Set<Perm> perms, boolean showRationale )
        {
            Builder builder = new Builder( perms );
            if ( showRationale && null != m_rationaleMsg ) {
                builder.setOnShowRationale( new OnShowRationale() {
                        @Override
                        public void onShouldShowRationale( Set<Perm> perms ) {
                            m_delegate.makeConfirmThenBuilder( m_rationaleMsg,
                                                               Action.PERMS_QUERY )
                                .setTitle( R.string.perms_rationale_title )
                                .setPosButton( R.string.button_ask_again )
                                .setNegButton( R.string.button_skip )
                                .setParams( QueryInfo.this.getParams() )
                                .show();
                        }
                    } );
            }
            builder.asyncQuery( m_delegate.getActivity(), new PermCbck() {
                    @Override
                    public void onPermissionResult( Map<Perm, Boolean> permsMap ) {
                        if ( Action.SKIP_CALLBACK != m_action ) {

                            // We need all the sought perms to have been granted
                            boolean allGranted = true;
                            Iterator<Perm> iter = permsMap.keySet().iterator();
                            while ( allGranted && iter.hasNext() ) {
                                Perm perm = iter.next();
                                allGranted = allGranted && permsMap.get( perm );
                            }

                            if ( allGranted ) {
                                m_delegate.onPosButton( m_action, m_params );
                            } else {
                                m_delegate.onNegButton( m_action, m_params );
                            }
                        }
                    }
                } );
        }

        // Cons up a call with a "no" answer, and post it.
        private void doItFail( Set<Perm> bannedPerms )
        {
            int resID = 0;

            final Perm[] perms = bannedPerms.toArray( new Perm[bannedPerms.size()] );
            int[] expls = perms[0].getExpl();
            m_delegate.makeNotAgainBuilder(expls[1], expls[0]).show();

            m_delegate.post( new Runnable() {
                    @Override
                    public void run() {
                        m_delegate.onNegButton( m_action, perms );
                    }
                } );
        }

        // Post this in case we're called from inside dialog dismiss
        // code. Better to unwind the stack...
        private void handleButton( final boolean positive )
        {
            m_delegate.post( new Runnable() {
                    public void run() {
                        if ( positive ) {
                            doIt( false );
                        } else {
                            m_delegate.onNegButton( m_action, m_params );
                        }
                    }
                } );
        }
    }

    // Is the OS supporting runtime permission natively, i.e. version 23/M or
    // later.
    public static boolean haveNativePerms()
    {
        boolean result = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
        return result;
    }

    /**
     * Request permissions, giving rationale once, then call with action and
     * either positive or negative, the former if permission granted.
     */
    public static void tryGetPerms( DelegateBase delegate, Perm[] perms, int rationaleId,
                                    final Action action, Object... params )
    {
        // Log.d( TAG, "tryGetPerms(%s)", perm.toString() );
        Context context = XWApp.getContext();
        String msg = rationaleId == 0
            ? null : LocUtils.getString( context, rationaleId );
        tryGetPerms( delegate, perms, msg, action, params );
    }

    public static void tryGetPerms( DelegateBase delegate, Perm[] perms,
                                    String rationaleMsg, final Action action,
                                    Object... params )
    {
        // Log.d( TAG, "tryGetPerms(%s)", perm.toString() );
        new QueryInfo( delegate, action, perms, rationaleMsg, params )
            .doIt( true );
    }

    public static void tryGetPerms( DelegateBase delegate, Perm perm,
                                    String rationaleMsg, final Action action,
                                    Object... params )
    {
        tryGetPerms( delegate, new Perm[]{ perm }, rationaleMsg, action, params );
    }

    public static void tryGetPerms( DelegateBase delegate, Perm perm, int rationaleId,
                                    final Action action, Object... params )
    {
        tryGetPerms( delegate, new Perm[]{perm}, rationaleId, action, params );
    }

    public static void onGotPermsAction( DelegateBase delegate, boolean positive,
                                         Object[] params )
    {
        QueryInfo info = new QueryInfo( delegate, params );
        info.handleButton( positive );
    }

    private static Map<Integer, PermCbck> s_map = new HashMap<Integer, PermCbck>();
    public static void gotPermissionResult( Context context, int code,
                                            String[] perms, int[] granteds )
    {
        // Log.d( TAG, "gotPermissionResult(%s)", perms.toString() );
        Map<Perm, Boolean> result = new HashMap<Perm, Boolean>();
        boolean shouldResend = false;
        for ( int ii = 0; ii < perms.length; ++ii ) {
            Perm perm = Perm.getFor( perms[ii] );
            boolean granted = PackageManager.PERMISSION_GRANTED == granteds[ii];
            result.put( perm, granted );

            // Hack. If SMS has been granted, resend all moves. This should be
            // replaced with an api allowing listeners to register
            // Perm-by-Perm, but I'm in a hurry.
            if ( granted && (perm == Perm.SEND_SMS || perm == Perm.RECEIVE_SMS) ) {
                shouldResend = true;
            }

            // Log.d( TAG, "calling %s.onPermissionResult(%s, %b)",
            //                record.cbck.getClass().getSimpleName(), perm.toString(),
            //                granted );
        }

        if ( shouldResend ) {
            GameUtils.resendAllIf( context, CommsConnType.COMMS_CONN_SMS,
                                   true, true );
        }

        PermCbck cbck = s_map.remove( code );
        if ( null != cbck ) {
            cbck.onPermissionResult( result );
        }
    }

    public static boolean havePermission( Perm perm )
    {
        String permString = perm.getString();
        boolean result = PackageManager.PERMISSION_GRANTED
            == ContextCompat.checkSelfPermission( XWApp.getContext(), permString );
        // Log.d( TAG, "havePermission(%s) => %b", permString, result );
        return result;
    }

    // If two permission requests are made in a row the map may contain more
    // than one entry.
    private static int s_nextRecord = 0;
    private static int register( PermCbck cbck )
    {
        DbgUtils.assertOnUIThread();
        int code = ++s_nextRecord;
        s_map.put( code, cbck );
        return code;
    }
}

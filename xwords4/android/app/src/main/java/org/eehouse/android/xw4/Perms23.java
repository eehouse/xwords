/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eehouse.android.xw4.DlgDelegate.Action;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.loc.LocUtils;

    
public class Perms23 {
    private static final String TAG = Perms23.class.getSimpleName();
    
    public static enum Perm {
        READ_PHONE_STATE(Manifest.permission.READ_PHONE_STATE),
        STORAGE(Manifest.permission.WRITE_EXTERNAL_STORAGE),
        SEND_SMS(Manifest.permission.SEND_SMS),
        RECEIVE_SMS(Manifest.permission.RECEIVE_SMS),
        READ_CONTACTS(Manifest.permission.READ_CONTACTS),
        BLUETOOTH_CONNECT(BuildConfig.BLUETOOTH_CONNECT),
        BLUETOOTH_SCAN(BuildConfig.BLUETOOTH_SCAN);

        private String m_str;
        private boolean m_granted;
        private Perm(String str) { m_str = str; }
        private Perm(boolean granted) { m_granted = granted; }

        public String getString() { return m_str; }
        public boolean isBanned( Context context )
        {
            return !permInManifest( context, this );
        }
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

    private static Map<Perm, Boolean> sManifestMap = new HashMap<>();
    private static boolean permInManifest( Context context, Perm perm )
    {
        boolean result = false;
        if ( sManifestMap.containsKey( perm ) ) {
            result = sManifestMap.get( perm );
        } else {
            PackageManager pm = context.getPackageManager();
            try {
                String[] pis = pm
                    .getPackageInfo( BuildConfig.APPLICATION_ID,
                                     PackageManager.GET_PERMISSIONS )
                    .requestedPermissions;
                if ( pis == null ) {
                    Assert.failDbg();
                } else {
                    String manifestName = perm.getString();
                    for ( int ii = 0; !result && ii < pis.length; ++ii ) {
                        result = pis[ii].equals( manifestName );
                    }
                }
            } catch( PackageManager.NameNotFoundException nnfe ) {
                Log.e(TAG, "permInManifest() nnfe: %s", nnfe.getMessage());
            }
            sManifestMap.put( perm, result );
        }
        return result;
    }

    public interface PermCbck {
        void onPermissionResult( boolean allGood, Map<Perm, Boolean> perms );
    }
    public interface OnShowRationale {
        void onShouldShowRationale( Set<Perm> perms );
    }

    public static class Builder {
        private Set<Perm> m_perms = new HashSet<>();
        private OnShowRationale m_onShow;

        public Builder(Set<Perm> perms) {
            m_perms.addAll( perms );
        }

        public Builder( Perm... perms ) {
            for ( Perm perm : perms ) {
                m_perms.add( perm );
            }
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

        // We have set of permissions. For any of them that needs asking (not
        // granted AND not banned) start an ask.
        //
        // PENDING: I suspect this'll crash if I ask for a banned and
        // non-banned at the same time (and don't have either)
        public void asyncQuery( Activity activity, PermCbck cbck )
        {
            Log.d( TAG, "asyncQuery(%s)", m_perms );
            boolean haveAll = true;
            boolean shouldShow = false;
            Set<Perm> needShow = new HashSet<>();

            List<String> askStrings = new ArrayList<>();
            for ( Perm perm : m_perms ) {
                String permStr = perm.getString();
                boolean haveIt = perm.isBanned(activity) || PackageManager.PERMISSION_GRANTED
                    == ContextCompat.checkSelfPermission( activity, permStr );

                if ( !haveIt ) {
                    // do not pass banned perms to the OS! They're not in
                    // AndroidManifest.xml so may crash on some devices
                    Assert.assertFalse( perm.isBanned(activity) );
                    askStrings.add( permStr );

                    if ( null != m_onShow ) {
                        needShow.add( perm );
                    }
                }

                haveAll = haveAll && haveIt;
            }

            if ( haveAll ) {
                if ( null != cbck ) {
                    Map<Perm, Boolean> map = new HashMap<>();
                    boolean allGood = true;
                    for ( Perm perm : m_perms ) {
                        boolean banned = perm.isBanned(activity);
                        map.put( perm, !banned );
                        allGood = allGood & !banned;
                    }
                    callOPR( cbck, allGood, map );
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
        private int mNAKey;
        private DelegateBase m_delegate;
        private String m_rationaleMsg;
        private Object[] m_params;

        private QueryInfo( DelegateBase delegate, Action action,
                           Perm[] perms, String msg, int naKey,
                           Object[] params ) {
            m_delegate = delegate;
            m_action = action;
            m_perms = perms;
            m_rationaleMsg = msg;
            mNAKey = naKey;
            m_params = params;
        }

        private QueryInfo( DelegateBase delegate, Object[] params )
        {
            this( delegate, (Action)params[0], (Perm[])params[1], (String)params[2],
                  0, (Object[])params[3] );
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
                if ( perm.isBanned(m_delegate.getActivity()) ) {
                    bannedPerms.add( perm );
                } else {
                    validPerms.add( perm );
                }
            }

            if ( 0 < validPerms.size() ) {
                doItAsk( validPerms, showRationale );
            }
            if ( 0 < bannedPerms.size() ) {
                postNeg();
            }
        }

        private void doItAsk( Set<Perm> perms, boolean showRationale )
        {
            Builder builder = new Builder( perms );
            if ( showRationale && null != m_rationaleMsg ) {
                builder.setOnShowRationale( new OnShowRationale() {
                        @Override
                        public void onShouldShowRationale( Set<Perm> perms ) {
                            m_delegate.makeConfirmThenBuilder( Action.PERMS_QUERY,
                                                               m_rationaleMsg )
                                .setTitle( R.string.perms_rationale_title )
                                .setPosButton( R.string.button_ask )
                                .setNegButton( R.string.button_deny )
                                .setParams( QueryInfo.this.getParams() )
                                .setNAKey( mNAKey )
                                .show();
                        }
                    } );
            }
            builder.asyncQuery( m_delegate.getActivity(), new PermCbck() {
                    @Override
                    public void onPermissionResult( boolean allGood,
                                                    Map<Perm, Boolean> permsMap ) {
                        if ( Action.SKIP_CALLBACK != m_action ) {
                            if ( allGood ) {
                                m_delegate.onPosButton( m_action, m_params );
                            } else {
                                m_delegate.onNegButton( m_action, m_params );
                            }
                        }
                    }
                } );
        }

        // Post this in case we're called from inside dialog dismiss
        // code. Better to unwind the stack...
        private void handleButton( final boolean positive )
        {
            if ( positive ) {
                m_delegate.post( new Runnable() {
                        @Override
                        public void run() {
                            doIt( false );
                        }
                    } );
            } else {
                postNeg();
            }
        }

        private void postNeg()
        {
            Perms23.postNeg( m_delegate, m_action, m_params );
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
    private static void tryGetPermsImpl( final DelegateBase delegate, Perm[] perms,
                                         String rationaleMsg, int naKey,
                                         final Action action, Object... params )
    {
        // Log.d( TAG, "tryGetPerms(%s)", perm.toString() );

        if ( 0 != naKey &&
             XWPrefs.getPrefsBoolean( delegate.getActivity(), naKey, false ) ) {
            postNeg( delegate, action, params );
        } else {
            new QueryInfo( delegate, action, perms, rationaleMsg,
                           naKey, params )
                .doIt( true );
        }
    }

    private static void postNeg( final DelegateBase delegate,
                                 final Action action, final Object[] params )
    {
        delegate.post( new Runnable() {
                @Override
                public void run() {
                    delegate.onNegButton( action, params );
                }
            } );
    }

    public static void tryGetPerms( DelegateBase delegate, Perm[] perms, int rationaleId,
                                    final Action action, Object... params )
    {
        // Log.d( TAG, "tryGetPerms(%s)", perm.toString() );
        String msg = LocUtils.getStringOrNull( rationaleId );
        tryGetPermsImpl( delegate, perms, msg, 0, action, params );
    }

    public static void tryGetPerms( DelegateBase delegate, Perm[] perms,
                                    String rationaleMsg, final Action action,
                                    Object... params )
    {
        tryGetPermsImpl( delegate, perms, rationaleMsg, 0, action, params );
    }

    public static void tryGetPerms( DelegateBase delegate, Perm perm,
                                    String rationaleMsg, final Action action,
                                    Object... params )
    {
        tryGetPermsImpl( delegate, new Perm[]{ perm }, rationaleMsg, 0,
                         action, params );
    }

    public static void tryGetPerms( DelegateBase delegate, Perm perm, int rationaleId,
                                    final Action action, Object... params )
    {
        tryGetPerms( delegate, new Perm[]{perm}, rationaleId, action, params );
    }

    public static void tryGetPermsNA( DelegateBase delegate, Perm perm,
                                      int rationaleId, int naKey,
                                      Action action, Object... params )
    {
        tryGetPermsImpl( delegate, new Perm[] {perm},
                         LocUtils.getStringOrNull( rationaleId ), naKey,
                         action, params );
    }

    public static void onGotPermsAction( DelegateBase delegate, boolean positive,
                                         Object[] params )
    {
        QueryInfo info = new QueryInfo( delegate, params );
        info.handleButton( positive );
    }

    private static Map<Integer, PermCbck> s_map = new HashMap<>();
    public static void gotPermissionResult( Context context, int code,
                                            String[] perms, int[] granteds )
    {
        // Log.d( TAG, "gotPermissionResult(%s)", perms.toString() );
        Map<Perm, Boolean> result = new HashMap<>();
        boolean shouldResend = false;
        boolean allGood = true;
        for ( int ii = 0; ii < perms.length; ++ii ) {
            Perm perm = Perm.getFor( perms[ii] );
            Assert.assertTrue( !perm.isBanned(context) || ! BuildConfig.DEBUG );
            boolean granted = PackageManager.PERMISSION_GRANTED == granteds[ii];
            allGood = allGood && granted;
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
            callOPR( cbck, allGood, result );
        }
    }

    public static boolean havePermissions( Context context, Perm... perms )
    {
        boolean result = true;
        for ( int ii = 0; result && ii < perms.length; ++ii ) {
            Perm perm = perms[ii];
            boolean thisResult;
            if ( perm.isBanned(context) ) {
                thisResult = bannedWithWorkaround( context, perm );
            } else {
                thisResult = PackageManager.PERMISSION_GRANTED
                    == ContextCompat.checkSelfPermission( XWApp.getContext(),
                                                          perm.getString() );
            }
            result = result && thisResult;
        }
        return result;
    }

    static boolean anyBanned( Context context, Perms23.Perm... perms )
    {
        boolean anyBanned = false;
        for ( int ii = 0; !anyBanned && ii < perms.length; ++ii ) {
            anyBanned = perms[ii].isBanned( context );
        }
        return anyBanned;
    }

    static boolean bannedWithWorkaround( Context context, Perms23.Perm... perms )
    {
        boolean allBanned = true;
        boolean workaroundKnown = true;
        for ( Perms23.Perm perm : perms ) {
            allBanned = allBanned && perm.isBanned(context);

            switch ( perm ) {
            case SEND_SMS:
            case RECEIVE_SMS:
                workaroundKnown = false;
                break;
            default:
                Log.e( TAG, "bannedWithWorkaround(): unexpected perm %s", perm );
                Assert.failDbg();
                break;
            }
        }

        boolean result = allBanned && workaroundKnown;
        Log.d( TAG, "bannedWithWorkaround() => %b", result );
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

    private static void callOPR( PermCbck cbck, boolean allGood,
                                 Map<Perm, Boolean> map )
    {
        Log.d( TAG, "callOPR(): passing %s to %s", map, cbck );
        cbck.onPermissionResult( allGood, map );
    }

}

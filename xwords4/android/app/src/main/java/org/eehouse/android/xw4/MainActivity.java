/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2016 by Eric House (xwords@eehouse.org).  All
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

import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import android.text.TextUtils;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;
import android.widget.LinearLayout;


import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class MainActivity extends XWActivity
    implements FragmentManager.OnBackStackChangedListener {
    private static final String TAG = MainActivity.class.getSimpleName();
    private static final int MAX_PANES_LANDSCAPE = 2;
    private static final boolean LOG_IDS = true;

    private DelegateBase m_dlgt;
    private LinearLayout m_root;
    private boolean m_safeToCommit;
    private ArrayList<Runnable> m_runWhenSafe = new ArrayList<>();
    private Intent m_newIntent; // work in progress...

    // for tracking launchForResult callback recipients
    private Map<RequestCode, WeakReference<DelegateBase>> m_pendingCodes
        = new HashMap<RequestCode, WeakReference<DelegateBase>>();

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        if ( BuildConfig.NON_RELEASE && !isTaskRoot() ) {
            Log.e( TAG, "isTaskRoot() => false!!! What to do?" );
        }

        m_dlgt = new DualpaneDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );

        m_root = (LinearLayout)findViewById( R.id.main_container );
        getSupportFragmentManager().addOnBackStackChangedListener( this );

        // Nothing to do if we're restarting
        if ( savedInstanceState == null ) {
            // In case this activity was started with special instructions from an Intent,
            // pass the Intent's extras to the fragment as arguments
            addFragmentImpl( GamesListFrag.newInstance(),
                             getIntent().getExtras(), null );
        }

        setSafeToRun();
    } // onCreate

    @Override
    protected void onSaveInstanceState( Bundle outState )
    {
        m_safeToCommit = false;
        super.onSaveInstanceState( outState );
    }

    @Override
    protected void onPostResume()
    {
        setSafeToRun();
        super.onPostResume();
        setVisiblePanes();
        logPaneFragments();
    }

    // called when we're brought to the front (probably as a result of
    // notification)
    @Override
    protected void onNewIntent( Intent intent )
    {
        super.onNewIntent( intent );

        m_dlgt.handleNewIntent( intent );
    }

    /* Sometimes I'm getting crashes because views don't have fragments
     * associated yet. I suspect that's because adding them's been postponed
     * via the m_runWhenSafe mechanism. So: postpone handling intents too.
     *
     * This postponing thing won't scale, and makes me suspect there's
     * something I'm doing wrong w.r.t. fragments. Should be revisited. In
     * this particular case there might be a better way to get to the Delegate
     * on which I need to call handleNewIntent().
     */
    protected boolean dispatchNewIntent( final Intent intent )
    {
        boolean handled;
        if ( m_safeToCommit ) {
            handled = dispatchNewIntentImpl( intent );
        } else {
            DbgUtils.assertOnUIThread();
            m_runWhenSafe.add( new Runnable() {
                    @Override
                    public void run() {
                        dispatchNewIntentImpl( intent );
                    }
                } );
            if ( BuildConfig.DEBUG ) {
                Log.d( TAG, "Putting off handling intent; %d waiting",
                       m_runWhenSafe.size() );
            }
            handled = true;
        }
        return handled;
    }

    private void popIntoView( XWFragment newTopFrag )
    {
        FragmentManager fm = getSupportFragmentManager();
        for ( ; ; ) {
            int top = m_root.getChildCount() - 1;
            if ( top < 0 ) {
                break;
            }
            View child = m_root.getChildAt( top );
            XWFragment frag = findFragment( child );
            if ( frag == newTopFrag ) {
                break;
            }
            String name = frag.getClass().getSimpleName();
            Log.d( TAG, "popIntoView(): popping %d: %s", top, name );
            fm.popBackStackImmediate();
            Log.d( TAG, "popIntoView(): DONE popping %s", name );
        }
    }

    /**
     * Run down the list of fragments until one can handle the intent. If
     * necessary, pop fragments above it until it comes into view. Then send
     * it the event.
     */
    private boolean dispatchNewIntentImpl( Intent intent )
    {
        boolean handled = false;

        for ( int ii = m_root.getChildCount() - 1; !handled && ii >= 0; --ii ) {
            View child = m_root.getChildAt( ii );
            XWFragment frag = findFragment( child );
            if ( null != frag ) {
                handled = frag.getDelegate().canHandleNewIntent( intent );
                if ( handled ) {
                    popIntoView( frag );
                    frag.getDelegate().handleNewIntent( intent );
                }
            } else {
                Log.d( TAG, "no fragment for child %s indx %d",
                       child.getClass().getSimpleName(), ii );
            }
        }

        if ( BuildConfig.DEBUG && !handled ) {
            // DbgUtils.showf( this, "dropping intent %s", intent.toString() );
            Log.d( TAG, "dropping intent %s", intent.toString() );
            // DbgUtils.printStack();
            // setIntent( intent ); -- look at handling this in onPostResume()?
            m_newIntent = intent;
        }
        return handled;
    }

    /**
     * The right-most pane only gets a chance to handle on-back-pressed.
     */
    protected boolean dispatchBackPressed()
    {
        XWFragment frag = getTopFragment();
        boolean handled = null != frag
            && frag.getDelegate().handleBackPressed();
        return handled;
    }

    protected void dispatchOnActivityResult( RequestCode requestCode,
                                             int resultCode, Intent data )
    {
        XWFragment frag = getTopFragment();

        if ( null != frag ) {
            frag.onActivityResult( requestCode.ordinal(), resultCode, data );
        } else {
            Log.d( TAG, "dispatchOnActivityResult(): can't dispatch %s",
                   requestCode.toString() );
        }
    }

    protected void dispatchOnCreateContextMenu( ContextMenu menu, View view,
                                                ContextMenuInfo menuInfo )
    {
        XWFragment[] frags = getVisibleFragments();
        for ( XWFragment frag : frags ) {
            frag.getDelegate().onCreateContextMenu( menu, view, menuInfo );
        }
    }

    protected boolean dispatchOnContextItemSelected( MenuItem item )
    {
        boolean handled = false;
        XWFragment[] frags = getVisibleFragments();
        for ( XWFragment frag : frags ) {
            handled = frag.getDelegate().onContextItemSelected( item );
            if ( handled ) {
                break;
            }
        }
        return handled;
    }

    //////////////////////////////////////////////////////////////////////
    // Delegator interface
    //////////////////////////////////////////////////////////////////////
    @Override
    public void addFragment( XWFragment fragment, Bundle extras )
    {
        addFragmentImpl( fragment, extras, fragment.getParentName() );
    }

    private class PendingResultCache {
        private WeakReference<Fragment> m_frag;
        public int m_request;
        public int m_result;
        public Intent m_data;
        public PendingResultCache( Fragment target, int request,
                                   int result, Intent data ) {
            m_frag = new WeakReference<>(target);
            m_request = request;
            m_result = result;
            m_data = data;
        }

        public Fragment getTarget() { return m_frag.get(); }
    }
    private PendingResultCache m_pendingResult;

    public void addFragmentForResult( XWFragment fragment, Bundle extras,
                                      RequestCode requestCode, XWFragment registrant )
    {
        DbgUtils.assertOnUIThread();

        fragment.setTargetFragment( registrant, requestCode.ordinal() );

        addFragmentImpl( fragment, extras, fragment.getParentName() );
    }

    protected void setFragmentResult( XWFragment fragment, int resultCode,
                                      Intent data )
    {
        Fragment target = fragment.getTargetFragment();
        int requestCode = fragment.getTargetRequestCode();

        Assert.assertNull( m_pendingResult );
        m_pendingResult = new PendingResultCache( target, requestCode,
                                                  resultCode, data );
    }

    protected void finishFragment( XWFragment fragment )
    {
        // Log.d( TAG, "finishFragment()" );
        int ID = fragment.getCommitID();
        getSupportFragmentManager()
            .popBackStack( ID, FragmentManager.POP_BACK_STACK_INCLUSIVE );
    }

    //////////////////////////////////////////////////////////////////////
    // FragmentManager.OnBackStackChangedListener
    //////////////////////////////////////////////////////////////////////
    public void onBackStackChanged()
    {
        // make sure the right-most are visible
        int fragCount = getSupportFragmentManager().getBackStackEntryCount();
        Log.i( TAG, "onBackStackChanged(); count now %d", fragCount );
        if ( 0 == fragCount ) {
            finish();
        } else {
            if ( fragCount == m_root.getChildCount() - 1 ) {
                View child = m_root.getChildAt( fragCount );
                if ( LOG_IDS ) {
                    Log.i( TAG, "onBackStackChanged(): removing view with id %x",
                           child.getId() );
                }
                m_root.removeView( child );
            }
            setVisiblePanes();

            // If there's a pending on-result call, make it.
            if ( null != m_pendingResult ) {
                Fragment target = m_pendingResult.getTarget();
                if ( null != target ) {
                    Log.i( TAG,"onBackStackChanged(): calling onActivityResult()" );
                    target.onActivityResult( m_pendingResult.m_request,
                                             m_pendingResult.m_result,
                                             m_pendingResult.m_data );
                }
                m_pendingResult = null;
            }
        }

        logPaneFragments();
    }

    ////////////////////////////////////////////////////////////////////////
    // Dualpane mode stuff
    ////////////////////////////////////////////////////////////////////////

    private XWFragment getTopFragment()
    {
        XWFragment frag = null;
        View child = m_root.getChildAt( m_root.getChildCount() - 1 );
        if ( null != child ) {
            frag = findFragment( child );
        }
        return frag;
    }

    private void logPaneFragments()
    {
        if ( BuildConfig.DEBUG ) {
            List<String> panePairs = new ArrayList<>();
            if ( null != m_root ) {
                int childCount = m_root.getChildCount();
                for ( int ii = 0; ii < childCount; ++ii ) {
                    View child = m_root.getChildAt( ii );
                    String name = findFragment( child ).getClass().getSimpleName();
                    String pair = String.format("%d:%s", ii, name );
                    panePairs.add( pair );
                }
            }

            FragmentManager fm = getSupportFragmentManager();
            List<String> fragPairs = new ArrayList<>();
            int fragCount = fm.getBackStackEntryCount();
            for ( int ii = 0; ii < fragCount; ++ii ) {
                FragmentManager.BackStackEntry entry = fm.getBackStackEntryAt( ii );
                String name = entry.getName();
                String pair = String.format("%d:%s", ii, name );
                fragPairs.add( pair );
            }
            Log.d( TAG, "panes: [%s]; frags: [%s]", TextUtils.join(",", panePairs),
                   TextUtils.join(",", fragPairs) );
        }
    }

    protected XWFragment[] getVisibleFragments()
    {
        return getFragments( true );
    }

    protected XWFragment[] getFragments( boolean visibleOnly )
    {
        int childCount = m_root.getChildCount();
        int count = visibleOnly
            ? Math.min( maxPanes(), childCount )
            : childCount;
        XWFragment[] result = new XWFragment[count];
        for ( int ii = 0; ii < count; ++ii ) {
            View child = m_root.getChildAt( childCount - 1 - ii );
            result[ii] = findFragment( child );
        }

        return result;
    }

    private int maxPanes()
    {
        int result;
        int orientation = getResources().getConfiguration().orientation;
        if ( XWPrefs.getIsTablet( this )
             && Configuration.ORIENTATION_LANDSCAPE == orientation ) {
            result = MAX_PANES_LANDSCAPE;
        } else {
            result = 1;
        }
        // Log.d( TAG, "maxPanes() => %d", result );
        return result;
    }

    private void setVisiblePanes()
    {
        // hide all but the right-most m_maxPanes children
        int nPanes = m_root.getChildCount();
        int maxPanes = maxPanes();
        for ( int ii = 0; ii < nPanes; ++ii ) {
            View child = m_root.getChildAt( ii );
            boolean visible = ii >= nPanes - maxPanes;
            child.setVisibility( visible ? View.VISIBLE : View.GONE );
            setMenuVisibility( child, visible );
            if ( visible ) {
                trySetTitle( child );
            }
        }

        logPaneFragments();
    }

    private void trySetTitle( View view )
    {
        XWFragment frag = findFragment( view );
        if ( null != frag ) {
            frag.setTitle();
        } else {
            Log.d( TAG, "trySetTitle(): no fragment found" );
        }
    }

    private void setMenuVisibility( View cont, boolean visible )
    {
        Fragment frag = findFragment( cont );
        if ( null != frag ) {   // hasn't been popped?
            frag.setMenuVisibility( visible );
        }
    }

    private XWFragment findFragment( View view )
    {
        XWFragment frag = XWFragment.findOwnsView( view );
        return frag;
    }

    private void addFragmentImpl( XWFragment fragment, Bundle bundle,
                                  String parentName )
    {
        fragment.setArguments( bundle );
        addFragmentImpl( fragment, parentName );
    }

    private void addFragmentImpl( final XWFragment fragment,
                                  final String parentName )
    {
        if ( m_safeToCommit ) {
            safeAddFragment( fragment, parentName );
        } else {
            DbgUtils.assertOnUIThread();
            m_runWhenSafe.add( new Runnable() {
                    @Override
                    public void run() {
                        safeAddFragment( fragment, parentName );
                    }
                } );
        }
    }

    private void popUnneeded( FragmentManager fm, String newName, String parentName )
    {
        final int fragCount = fm.getBackStackEntryCount();
        int lastKept = fragCount;
        for ( int ii = 0; ii < fragCount; ++ii ) {
            FragmentManager.BackStackEntry entry = fm.getBackStackEntryAt( ii );
            String entryName = entry.getName();
            if ( entryName.equals( newName ) ) {
                lastKept = ii - 1; // keep only my parent; kill my same-class sibling!
                break;
            } else if ( entryName.equals(parentName) ) {
                lastKept = ii;
                break;
            }
        }

        for ( int ii = fragCount - 1; ii > lastKept; --ii ) {
            fm.popBackStack();
        }
    }
    
    private void safeAddFragment( XWFragment fragment, String parentName )
    {
        Assert.assertTrue( m_safeToCommit );
        String newName = fragment.getClass().getSimpleName();
        FragmentManager fm = getSupportFragmentManager();

        popUnneeded( fm, newName, parentName );

        int ID = fm.beginTransaction()
            .add( R.id.main_container, fragment, newName )
            .addToBackStack( newName )
            .commit();
        fragment.setCommitID( ID );
        // Don't do this. It causes an exception if e.g. from fragment.start()
        // I wind up launching another fragment and calling into this code
        // again. If I need executePendingTransactions() I'm doing something
        // else wrong.
        // fm.executePendingTransactions();
    }

    private void setSafeToRun()
    {
        DbgUtils.assertOnUIThread();
        m_safeToCommit = true;
        for ( Runnable proc : m_runWhenSafe ) {
            proc.run();
        }
        m_runWhenSafe.clear();
    }
}

/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import junit.framework.Assert;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

public class MainActivity extends XWActivity
    implements FragmentManager.OnBackStackChangedListener {
    private static final int MAX_PANES_LANDSCAPE = 2;
    private static final boolean LOG_IDS = true;

    private DelegateBase m_dlgt;
    private boolean m_dpEnabled;

    // Used only if m_dpEnabled is true
    private LinearLayout m_root;
    private int m_maxPanes;
    private int m_nextID = 0x00FFFFFF;
    private Boolean m_isPortrait;
    private boolean m_safeToCommit;
    private ArrayList<Runnable> m_runWhenSafe = new ArrayList<Runnable>();
    private Intent m_newIntent; // work in progress...

    // for tracking launchForResult callback recipients
    private Map<RequestCode, WeakReference<DelegateBase>> m_pendingCodes
        = new HashMap<RequestCode, WeakReference<DelegateBase>>();

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        m_dpEnabled = XWPrefs.dualpaneEnabled( this );

        m_dlgt = m_dpEnabled ? new DualpaneDelegate( this, savedInstanceState )
            : new GamesListDelegate( this, savedInstanceState );
        super.onCreate( savedInstanceState, m_dlgt );

        if ( m_dpEnabled ) {
            m_root = (LinearLayout)findViewById( R.id.main_container );
            getSupportFragmentManager().addOnBackStackChangedListener( this );

            m_maxPanes = maxPanes();

            // Nothing to do if we're restarting
            if ( savedInstanceState == null ) {
                // In case this activity was started with special instructions from an Intent,
                // pass the Intent's extras to the fragment as arguments
                addFragmentImpl( GamesListFrag.newInstance(),
                                 getIntent().getExtras(), null );
            }
        }

        setSafeToRun();
    } // onCreate

    @Override
    protected void onSaveInstanceState( Bundle outState )
    {
        super.onSaveInstanceState( outState );
        m_safeToCommit = false;
    }

    @Override
    protected void onPostResume()
    {
        setSafeToRun();
        super.onPostResume();
    }

    // called when we're brought to the front (probably as a result of
    // notification)
    @Override
    protected void onNewIntent( Intent intent )
    {
        super.onNewIntent( intent );

        m_dlgt.handleNewIntent( intent );
    }

    @Override
    public void onConfigurationChanged( Configuration newConfig )
    {
        if ( m_dpEnabled ) {
            Rect rect = new Rect();
            m_root.getWindowVisibleDisplayFrame( rect );

            boolean isPortrait
                = Configuration.ORIENTATION_PORTRAIT == newConfig.orientation;
            DbgUtils.logi( getClass(), "onConfigurationChanged(isPortrait=%b)",
                           isPortrait );
            m_isPortrait = isPortrait;
            if ( isPortrait != (rect.width() <= rect.height()) ) {
                DbgUtils.logd( getClass(), "onConfigurationChanged(): isPortrait:"
                               + " %b; width: %d; height: %d",
                               isPortrait, rect.width(), rect.height() );
            }
            int maxPanes = isPortrait? 1 : MAX_PANES_LANDSCAPE;
            if ( m_maxPanes != maxPanes ) {
                m_maxPanes = maxPanes;
                setVisiblePanes();
            }
            tellOrientationChanged();
        }
        super.onConfigurationChanged( newConfig );
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
            m_runWhenSafe.add( new Runnable() {
                    @Override
                    public void run() {
                        dispatchNewIntentImpl( intent );
                    }
                } );
            if ( BuildConfig.DEBUG ) {
                DbgUtils.showf( this, "Putting off handling intent; %d waiting",
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
            XWFragment frag = (XWFragment)fm.findFragmentById( child.getId() );
            if ( frag == newTopFrag ) {
                break;
            }
            String name = frag.getClass().getSimpleName();
            DbgUtils.logd( getClass(), "popIntoView(): popping %d: %s", top, name );
            fm.popBackStackImmediate();
            DbgUtils.logd( getClass(), "popIntoView(): DONE popping %s",
                           name );
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
        FragmentManager fm = getSupportFragmentManager();

        for ( int ii = m_root.getChildCount() - 1; !handled && ii >= 0; --ii ) {
            View child = m_root.getChildAt( ii );
            XWFragment frag = (XWFragment)fm.findFragmentById( child.getId() );
            if ( null != frag ) {
                handled = frag.getDelegate().canHandleNewIntent( intent );
                if ( handled ) {
                    popIntoView( frag );
                    frag.getDelegate().handleNewIntent( intent );
                }
            } else {
                DbgUtils.logd( getClass(), "no fragment for child %s indx %d",
                                child.getClass().getSimpleName(), ii );
            }
        }

        if ( BuildConfig.DEBUG && !handled ) {
            // DbgUtils.showf( this, "dropping intent %s", intent.toString() );
            DbgUtils.logd( getClass(), "dropping intent %s", intent.toString() );
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
            DbgUtils.logd( getClass(), "dispatchOnActivityResult(): can't dispatch %s",
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
    public boolean inDPMode() {
        return m_dpEnabled;
    }

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
        public PendingResultCache( Fragment target, int request, int result, Intent data ) {
            m_frag = new WeakReference<Fragment>(target);
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

    protected void finishFragment()
    {
        // Assert.assertTrue( fragment instanceof XWFragment );
        // DbgUtils.logf( "MainActivity.finishFragment(%s)", fragment.toString() );
        getSupportFragmentManager().popBackStack/*Immediate*/();
    }

    //////////////////////////////////////////////////////////////////////
    // FragmentManager.OnBackStackChangedListener
    //////////////////////////////////////////////////////////////////////
    public void onBackStackChanged()
    {
        // make sure the right-most are visible
        int fragCount = getSupportFragmentManager().getBackStackEntryCount();
        DbgUtils.logi( getClass(), "onBackStackChanged(); count now %d", fragCount );
        if ( 0 == fragCount ) {
            finish();
        } else {
            if ( fragCount == m_root.getChildCount() - 1 ) {
                View child = m_root.getChildAt( fragCount );
                if ( LOG_IDS ) {
                    DbgUtils.logi( getClass(), "onBackStackChanged(): removing view with id %x",
                                   child.getId() );
                }
                m_root.removeView( child );
                setVisiblePanes();
            }

            // If there's a pending on-result call, make it.
            if ( null != m_pendingResult ) {
                Fragment target = m_pendingResult.getTarget();
                if ( null != target ) {
                    DbgUtils.logi( getClass(),"onBackStackChanged(): calling onActivityResult()" );
                    target.onActivityResult( m_pendingResult.m_request,
                                             m_pendingResult.m_result,
                                             m_pendingResult.m_data );
                }
                m_pendingResult = null;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////
    // Dualpane mode stuff
    ////////////////////////////////////////////////////////////////////////

    private XWFragment getTopFragment()
    {
        XWFragment frag = null;
        View child = m_root.getChildAt( m_root.getChildCount() - 1 );
        if ( null != child ) {
            frag = (XWFragment)getSupportFragmentManager()
                .findFragmentById( child.getId() );
        }
        return frag;
    }

    private XWFragment[] getVisibleFragments()
    {
        int childCount = m_root.getChildCount();
        int count = Math.min( m_maxPanes, childCount );
        XWFragment[] result = new XWFragment[count];
        for ( int ii = 0; ii < count; ++ii ) {
            View child = m_root.getChildAt( childCount - 1 - ii );
            result[ii] = (XWFragment)getSupportFragmentManager()
                .findFragmentById( child.getId() );
        }

        return result;
    }

    // Walk all Fragment children and if they care notify of change.
    private void tellOrientationChanged()
    {
        FragmentManager fm = getSupportFragmentManager();
        int nPanes = m_root.getChildCount();
        for ( int ii = 0; ii < nPanes; ++ii ) {
            FrameLayout frame = (FrameLayout)m_root.getChildAt( ii );
            int id = frame.getId();
            Fragment frag = fm.findFragmentById( id );
            if ( null == frag ) {
                DbgUtils.logw( getClass(),"tellOrienationChanged: NO FRAG at %d, id=%d", ii, id );
            } else if ( frag instanceof XWFragment ) {
                ((XWFragment)frag).getDelegate().orientationChanged();
            }
        }
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
        // DbgUtils.logf( "maxPanes() => %d", result );
        return result;
    }

    private void setVisiblePanes()
    {
        // hide all but the right-most m_maxPanes children
        int nPanes = m_root.getChildCount();
        for ( int ii = 0; ii < nPanes; ++ii ) {
            View child = m_root.getChildAt( ii );
            boolean visible = ii >= nPanes - m_maxPanes;
            DbgUtils.logi( getClass(), "pane %d: visible=%b", ii, visible );
            child.setVisibility( visible ? View.VISIBLE : View.GONE );
            setMenuVisibility( child, visible );
            if ( visible ) {
                trySetTitle( child );
            }
        }
    }

    private void trySetTitle( View view )
    {
        XWFragment frag = (XWFragment)
            getSupportFragmentManager().findFragmentById( view.getId() );
        if ( null != frag ) {
            frag.setTitle();
        } else {
            DbgUtils.logd( getClass(), "trySetTitle(): no fragment for id %x",
                            view.getId() );
        }
    }

    private void setMenuVisibility( View cont, boolean visible )
    {
        FrameLayout layout = (FrameLayout)cont;
        FragmentManager fm = getSupportFragmentManager();
        int hidingId = layout.getId();
        Fragment frag = fm.findFragmentById( hidingId );
        if ( null != frag ) {   // hasn't been popped?
            frag.setMenuVisibility( visible );
        }
    }

    private void addFragmentImpl( Fragment fragment, Bundle bundle,
                                  String parentName )
    {
        fragment.setArguments( bundle );
        addFragmentImpl( fragment, parentName );
    }

    private void addFragmentImpl( final Fragment fragment,
                                  final String parentName )
    {
        if ( m_safeToCommit ) {
            safeAddFragment( fragment, parentName );
        } else {
            m_runWhenSafe.add( new Runnable() {
                    @Override
                    public void run() {
                        safeAddFragment( fragment, parentName );
                    }
                } );
            if ( BuildConfig.DEBUG ) {
                DbgUtils.showf( this, "Putting off fragment construction; %d waiting",
                                m_runWhenSafe.size() );
            }
        }
    }

    private void safeAddFragment( Fragment fragment, String parentName )
    {
        Assert.assertTrue( m_safeToCommit );
        String newName = fragment.getClass().getSimpleName();
        boolean replace = false;
        FragmentManager fm = getSupportFragmentManager();
        int fragCount = fm.getBackStackEntryCount();
        int containerCount = m_root.getChildCount();
        DbgUtils.logi( getClass(), "fragCount: %d; containerCount: %d", fragCount, containerCount );
        // Assert.assertTrue( fragCount == containerCount );

        // Replace IF we're adding something of the same class at right OR if
        // we're adding something with the existing left pane as its parent
        // (delegator)
        if ( 1 < fragCount ) {
            Assert.assertTrue( MAX_PANES_LANDSCAPE == 2 ); // otherwise FIXME
            FragmentManager.BackStackEntry entry
                = fm.getBackStackEntryAt( fragCount - 2 );
            String curName = entry.getName();
            // DbgUtils.logf( "comparing %s, %s", curName, parentName );
            replace = curName.equals( parentName );
        }

        if ( replace ) {
            fm.popBackStack();
        }

        // Replace doesn't seem to work with generated IDs, so we'll create a
        // new FrameLayout each time.  If we're replacing, we'll replace the
        // current rightmost FrameLayout.  Otherwise we'll add a new one.
        LinearLayout.LayoutParams lp
            = new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams
                                            .MATCH_PARENT, 1.0f);
        FrameLayout cont = new FrameLayout( this );
        cont.setLayoutParams( lp );
        int id = --m_nextID;
        cont.setId( id );
        if ( LOG_IDS ) {
            DbgUtils.logi( getClass(), "assigning id %x to view with name %s", id, newName );
        }
        m_root.addView( cont, replace ? containerCount - 1 : containerCount );

        if ( !replace && containerCount >= m_maxPanes ) {
            int indx = containerCount - m_maxPanes;
            View child = m_root.getChildAt( indx );
            child.setVisibility( View.GONE );

            setMenuVisibility( child, false );

            DbgUtils.logi( getClass(), "hiding %dth container", indx );
        }

        fm.beginTransaction()
            .add( id, fragment )
            .addToBackStack( newName )
            .commit();
        // Don't do this. It causes an exception if e.g. from fragment.start()
        // I wind up launching another fragment and calling into this code
        // again. If I need executePendingTransactions() I'm doing something
        // else wrong.
        // fm.executePendingTransactions();
    }

    private void setSafeToRun()
    {
        m_safeToCommit = true;
        for ( Runnable proc : m_runWhenSafe ) {
            proc.run();
        }
        m_runWhenSafe.clear();
    }
}

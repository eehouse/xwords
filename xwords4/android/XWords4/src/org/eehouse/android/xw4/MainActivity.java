/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009 - 2014 by Eric House (xwords@eehouse.org).  All
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

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Point;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager.BackStackEntry;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.LinearLayout;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.eehouse.android.xw4.jni.CurGameInfo;

import junit.framework.Assert;

public class MainActivity extends XWActivity
    implements FragmentManager.OnBackStackChangedListener {
    private static final int MAX_PANES_LANDSCAPE = 2;
    private static final boolean LOG_IDS = false;

    private DelegateBase m_dlgt;
    private boolean m_dpEnabled;

    // Used only if m_dpEnabled is true
    private LinearLayout m_root;
    private int m_maxPanes;
    private int m_nextID = 0x00FFFFFF;
    private Boolean m_isPortrait;

    // for tracking launchForResult callback recipients
    private Map<RequestCode, WeakReference<DelegateBase>> m_pendingCodes
        = new HashMap<RequestCode, WeakReference<DelegateBase>>();

    @Override
    protected void onCreate( Bundle savedInstanceState )
    {
        m_dpEnabled = XWPrefs.dualpaneEnabled( this );
        if ( BuildConfig.DEBUG ) {
            Utils.showToast( this, "dualpane mode: " + m_dpEnabled );
        }

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
                addFragmentImpl( new GamesListFrag(null),
                                 getIntent().getExtras(), null );
            }
        }
    } // onCreate

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
            DbgUtils.logf( "MainActivity.onConfigurationChanged(isPortrait=%b)",
                           isPortrait );
            m_isPortrait = isPortrait;
            if ( isPortrait != (rect.width() <= rect.height()) ) {
                DbgUtils.logdf( "FragActivity.onConfigurationChanged(): isPortrait:"
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

    /**
     * Run down the list of fragments until one handles the intent. If no
     * visible one does, pop hidden ones into view until one of them
     * does. Yes, this will take us back to GamesList being visible even if
     * nothing handles the intent, but at least now all are handled by
     * GamesList anyway.
     */
    protected boolean dispatchNewIntent( Intent intent )
    {
        boolean handled = false;
        FragmentManager fm = getSupportFragmentManager();

        // First try non-left-most fragments, if any. Once we've eliminated
        // them we can just iterate on the leftmost fragment.
        int nNonLeft = m_maxPanes - 1;
        // include paged-to-left invisible views
        int viewCount = m_root.getChildCount();
        for ( int ii = nNonLeft; !handled && 0 < ii; --ii ) {
            View child = m_root.getChildAt( viewCount - ii );
            Fragment frag = fm.findFragmentById( child.getId() );
            handled = ((XWFragment)frag).getDelegate().handleNewIntent( intent );
        }

        while ( !handled ) {
            // Now iterate on the leftmost, popping if necessary to page new
            // ones into place
            int childCount = m_root.getChildCount();
            int hiddenCount = Math.max( 0, childCount - m_maxPanes );
            for ( int ii = hiddenCount; ii >= 0; --ii ) {
                View child = m_root.getChildAt( ii );
                Fragment frag = fm.findFragmentById( child.getId() );
                // DbgUtils.logf( "left-most case (child %d): %s", hiddenCount,
                //                frag.getClass().getSimpleName() );
                handled = ((XWFragment)frag).getDelegate()
                    .handleNewIntent( intent );

                if ( handled ) {
                    break;
                } else if ( ii > 0 ) {
                    DbgUtils.logf( "popping %s",
                                   frag.getClass().getSimpleName() );
                    fm.popBackStackImmediate(); // callback removes view
                }
            }
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
            DbgUtils.logdf( "dispatchOnActivityResult(): can't dispatch %s", 
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

    protected Point getFragmentSize()
    {
        Rect rect = new Rect();
        m_root.getWindowVisibleDisplayFrame( rect );
        int width = rect.width();
        int height = rect.height();
        if ( null != m_isPortrait && m_isPortrait && height < width ) {
            int tmp = width;
            width = height;
            height = tmp;
        }
        return new Point( width / Math.min( m_maxPanes, m_root.getChildCount() ),
                          height );
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
        addFragmentImpl( fragment, extras, fragment.getParent() );
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

        addFragmentImpl( fragment, extras, fragment.getParent() );
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
        DbgUtils.logf( "MainActivity.onBackStackChanged(); count now %d", fragCount );
        if ( 0 == fragCount ) {
            finish();
        } else {
            if ( fragCount == m_root.getChildCount() - 1 ) {
                View child = m_root.getChildAt( fragCount );
                if ( LOG_IDS ) {
                    DbgUtils.logf( "onBackStackChanged(): removing view with id %x",
                                   child.getId() );
                }
                m_root.removeView( child );
                setVisiblePanes();
            }

            // If there's a pending on-result call, make it.
            if ( null != m_pendingResult ) {
                Fragment target = m_pendingResult.getTarget();
                if ( null != target ) {
                    DbgUtils.logf( "onBackStackChanged(): calling onActivityResult()" );
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
        View child = m_root.getChildAt( m_root.getChildCount() - 1 );
        XWFragment frag = (XWFragment)getSupportFragmentManager()
            .findFragmentById( child.getId() );
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
                DbgUtils.logf( "tellOrienationChanged: NO FRAG at %d, id=%d", ii, id );
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
        return result;
    }

    private void setVisiblePanes()
    {
        // hide all but the right-most m_maxPanes children
        int nPanes = m_root.getChildCount();
        for ( int ii = 0; ii < nPanes; ++ii ) {
            View child = m_root.getChildAt( ii );
            boolean visible = ii >= nPanes - m_maxPanes;
            DbgUtils.logf( "pane %d: visible=%b", ii, visible );
            child.setVisibility( visible ? View.VISIBLE : View.GONE );
            setMenuVisibility( child, visible );
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
                                  Delegator parent )
    {
        fragment.setArguments( bundle );
        addFragmentImpl( fragment, parent );
    }

    private void addFragmentImpl( Fragment fragment, Delegator parent )
    {
        String newName = fragment.getClass().getSimpleName();
        boolean replace = false;
        FragmentManager fm = getSupportFragmentManager();
        int fragCount = fm.getBackStackEntryCount();
        int containerCount = m_root.getChildCount();
        DbgUtils.logf( "fragCount: %d; containerCount: %d", fragCount, containerCount );
        // Assert.assertTrue( fragCount == containerCount );

        // Replace IF we're adding something of the same class at right OR if
        // we're adding something with the existing left pane as its parent
        // (delegator)
        if ( 1 < fragCount ) {
            Assert.assertTrue( MAX_PANES_LANDSCAPE == 2 ); // otherwise FIXME
            FragmentManager.BackStackEntry entry
                = fm.getBackStackEntryAt( fragCount - 2 );
            String curName = entry.getName();
            String delName = parent.getClass().getSimpleName();
            // DbgUtils.logf( "comparing %s, %s", curName, delName );
            replace = curName.equals( delName );
        }

        if ( replace ) {
            fm.popBackStack();
        }

        // Replace doesn't seem to work with generated IDs, so we'll create a
        // new FrameLayout each time.  If we're replacing, we'll replace the
        // current rightmost FrameLayout.  Otherwise we'll add a new one.
        FrameLayout cont = new FrameLayout( this );
        cont.setLayoutParams( new LayoutParams(0, LayoutParams.MATCH_PARENT, 1.0f) );
        int id = --m_nextID;
        cont.setId( id );
        if ( LOG_IDS ) {
            DbgUtils.logf( "assigning id %x to view with name %s", id, newName );
        }
        m_root.addView( cont, replace ? containerCount - 1 : containerCount );

        if ( !replace && containerCount >= m_maxPanes ) {
            int indx = containerCount - m_maxPanes;
            View child = m_root.getChildAt( indx );
            child.setVisibility( View.GONE );

            setMenuVisibility( child, false );

            DbgUtils.logf( "hiding %dth container", indx );
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
}

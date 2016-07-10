/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014-2016 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

import android.graphics.Rect;
import android.app.Dialog;
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentManager.BackStackEntry;
import android.support.v4.app.FragmentTransaction;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.LinearLayout;
import android.widget.LinearLayout;

import junit.framework.Assert;

public class FragActivity extends FragmentActivity 
    implements FragmentManager.OnBackStackChangedListener {

    private static final int MAX_PANES_LANDSCAPE = 3;

    public interface OrientChangeListener {
        void orientationChanged();
    }

    private static FragActivity s_this;

    private LinearLayout m_root;
    private int m_nextID = 0x00FFFFFF;
    private int m_maxPanes;
    private Boolean m_isPortrait;

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        s_this = this;
        super.onCreate( savedInstanceState );
        setContentView( R.layout.dualcontainer );

        m_root = (LinearLayout)findViewById( R.id.main_container );
        getSupportFragmentManager().addOnBackStackChangedListener( this );

        m_maxPanes = maxPanes();

        // Nothing to do if we're restarting
        if ( savedInstanceState == null ) {
            // In case this activity was started with special instructions from an Intent,
            // pass the Intent's extras to the fragment as arguments
            addFragmentImpl( new GamesListFrag(), getIntent().getExtras(), null );
        }
    }

    @Override
    public void onBackPressed() 
    {
        DbgUtils.logf( "FragActivity.onBackPressed()" );
        super.onBackPressed();
    }

    @Override
    public void onConfigurationChanged( Configuration newConfig )
    {
        Rect rect = new Rect();
        m_root.getWindowVisibleDisplayFrame( rect );

        boolean isPortrait
            = Configuration.ORIENTATION_PORTRAIT == newConfig.orientation;
        DbgUtils.logf( "FragActivity.onConfigurationChanged(isPortrait=%b)",
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
        super.onConfigurationChanged( newConfig );
    }

    protected void getFragmentDims( int[] dims )
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
        dims[0] = width / Math.min( m_maxPanes, m_root.getChildCount() );
        dims[1] = height;
    }

    @Override
    protected Dialog onCreateDialog( int id )
    {
        return DlgDelegate.onCreateDialog( id );
    }

    @Override
    protected void onPrepareDialog( int id, Dialog dialog )
    {
        DlgDelegate.onPrepareDialog( id, dialog );
    }

    //////////////////////////////////////////////////////////////////////
    // FragmentManager.OnBackStackChangedListener
    //////////////////////////////////////////////////////////////////////
    public void onBackStackChanged()
    {
        DbgUtils.logf( "FragActivity.onBackStackChanged()" );
        // make sure the right-most are visible
        int fragCount = getSupportFragmentManager().getBackStackEntryCount();
        if ( 0 == fragCount ) {
            finish();
        } else if ( fragCount == m_root.getChildCount() - 1 ) {
            m_root.removeViewAt( fragCount );
            setVisiblePanes();
        }
    }

    // public void launchDictFrag( Bundle args )
    // {
    //     // DictBrowseFrag dbf = new DictBrowseFrag();
    //     // dbf.setArguments( args );
    //     // addFragment( dbf );
    // }

    protected void finishFragment()
    {
        popFragment( null );
    }

    protected void popFragment( Fragment frag )
    {
        getSupportFragmentManager().popBackStack();
    }

    private void addFragmentImpl( Fragment fragment, Bundle bundle, 
                                  Delegator parent ) 
    {
        fragment.setArguments( bundle );
        addFragmentImpl( fragment, parent );
    }

    private void addFragmentImpl( Fragment fragment, Delegator delegator )
    {
        String newName = fragment.getClass().getName();
        boolean replace = false;
        FragmentManager fm = getSupportFragmentManager();
        int fragCount = fm.getBackStackEntryCount();
        int containerCount = m_root.getChildCount();
        DbgUtils.logf( "fragCount: %d; containerCount: %d", fragCount, containerCount );
        // Assert.assertTrue( fragCount == containerCount );

        // Replace IF we're adding something of the same class at right OR if
        // we're adding something with the existing left pane as its parent
        // (delegator)
        if ( 0 < fragCount ) {
            FragmentManager.BackStackEntry entry = fm.getBackStackEntryAt( fragCount - 1 );
            String curName = entry.getName();
            DbgUtils.logf( "name of last entry: %s", curName );
            replace = curName.equals( newName );

            if ( !replace && 1 < fragCount ) {
                entry = fm.getBackStackEntryAt( fragCount - 2 );
                curName = entry.getName();
                String delName = delegator.getClass().getName();
                DbgUtils.logf( "comparing %s, %s", curName, delName );
                replace = curName.equals( delName );
            }

            if ( replace ) {
                fm.popBackStack();
            }
        }

        // Replace doesn't seem to work with generated IDs, so we'll create a
        // new FrameLayout each time.  If we're replacing, we'll replace the
        // current rightmost FrameLayout.  Otherwise we'll add a new one.
        FrameLayout cont = new FrameLayout( this );
        cont.setLayoutParams( new LayoutParams(0, LayoutParams.MATCH_PARENT, 1.0f) );
        int id = --m_nextID;
        cont.setId( id );
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
        // fm.executePendingTransactions();
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
            } else if ( frag instanceof OrientChangeListener ) {
                ((OrientChangeListener)frag).orientationChanged();
            }
        }
    }

    private int maxPanes()
    {
        int result;
        int orientation = getResources().getConfiguration().orientation;
        if ( XWPrefs.getIsTablet( this ) 
             && Configuration.ORIENTATION_LANDSCAPE == orientation ) {
            result = 2;
        } else {
            result = 1;
        }
        return result;
    }

    private static FragActivity getThis()
    {
        Assert.assertNotNull( s_this );
        return s_this;
    }

    public static void addFragment( Fragment fragment, Bundle bundle )
    {
        addFragment( fragment, bundle, null );
    }

    public static void addFragment( Fragment fragment, Bundle bundle, 
                                    Delegator parent ) 
    {
        getThis().addFragmentImpl( fragment, bundle, parent );
    }

    public static void addFragmentForResult( Fragment fragment, Bundle bundle, 
                                             RequestCode requestCode, Delegator parent ) 
    {
        getThis().addFragmentImpl( fragment, bundle, parent );
    }
}

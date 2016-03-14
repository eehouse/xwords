/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Dialog;
import android.content.res.Configuration;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.LinearLayout;
import android.widget.LinearLayout;

import junit.framework.Assert;

public class FragActivity extends FragmentActivity 
    implements FragmentManager.OnBackStackChangedListener {

    public interface OrientChangeListener {
        void orientationChanged( int orientation );
    }

    private static FragActivity s_this;

    private LinearLayout m_root;
    private int m_nextID = 0x00FFFFFF;
    private int m_maxPanes;

    @Override
    public void onCreate( Bundle savedInstanceState ) 
    {
        s_this = this;
        super.onCreate( savedInstanceState );
        setContentView( R.layout.fragact );

        m_root = (LinearLayout)findViewById( R.id.main_container );
        getSupportFragmentManager().addOnBackStackChangedListener( this );

        m_maxPanes = maxPanes();

        // Nothing to do if we're restarting
        if ( savedInstanceState == null ) {
            // In case this activity was started with special instructions from an Intent,
            // pass the Intent's extras to the fragment as arguments
            addFragmentImpl( new GamesListFrag(), getIntent().getExtras() );
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
        int orientation = newConfig.orientation;
        boolean isPortrait = orientation == Configuration.ORIENTATION_PORTRAIT;
        int maxPanes = isPortrait? 1 : 2;
        if ( m_maxPanes != maxPanes ) {
            m_maxPanes = maxPanes;
            setVisiblePanes();
            tellOrientationChanged( orientation );
        }
        super.onConfigurationChanged( newConfig );
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

    private void addFragmentImpl( Fragment fragment, Bundle bundle ) 
    {
        fragment.setArguments( bundle );
        addFragmentImpl( fragment );
    }

    private void addFragmentImpl( Fragment fragment ) 
    {
        String newName = fragment.getClass().getName();
        boolean replace = false;
        FragmentManager fm = getSupportFragmentManager();
        int fragCount = fm.getBackStackEntryCount();
        int contCount = m_root.getChildCount();
        DbgUtils.logf( "fragCount: %d; contCount: %d", fragCount, contCount );
        Assert.assertTrue( fragCount == contCount );

        if ( 0 < contCount ) {
            FragmentManager.BackStackEntry entry = fm.getBackStackEntryAt( fragCount - 1 );
            String curName = entry.getName();
            DbgUtils.logf( "name of last entry: %s", curName );
            replace = curName.equals( newName );
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
        m_root.addView( cont, replace ? contCount - 1 : contCount );

        if ( !replace && contCount >= m_maxPanes ) {
            int indx = contCount - m_maxPanes;
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
        frag.setMenuVisibility( visible );
    }

    // Walk all Fragment children and if they care notify of change.
    private void tellOrientationChanged( int orientation )
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
                ((OrientChangeListener)frag).orientationChanged( orientation );
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

    public static void launchGame( long rowid, boolean invited )
    {
        Bundle args = GameUtils.makeLaunchExtras( rowid, invited );
        addFragment( new BoardFrag(), args );
    }

    public static void addFragment( Fragment fragment, Bundle bundle ) 
    {
        getThis().addFragmentImpl( fragment, bundle );
    }
}

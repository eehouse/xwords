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

import java.lang.ref.WeakReference;
import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.view.ViewGroup;
import android.view.View;

import junit.framework.Assert;

public class BoardContainer extends ViewGroup {
    private static final int TBAR_PCT_HOR = 90;
    private static final int TBAR_PCT_VERT = 85;

    private static boolean s_isPortrait = true; // initial assumption
    private static int s_width = 0;
    private static int s_height = 0;

    interface SizeChangeListener {
        void sizeChanged( int width, int height, boolean isPortrait );
    }
    private static WeakReference<SizeChangeListener> s_scl;
    
    public static void registerSizeChangeListener(SizeChangeListener scl)
    {
        s_scl = new WeakReference<SizeChangeListener>(scl);
        callSCL();
    }

    public static boolean getIsPortrait() { return s_isPortrait; }
    
    public BoardContainer( Context context, AttributeSet as ) {
        super( context, as );
        s_height = s_width = 0;
        s_scl = null;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec)
    {
        final int childCount = getChildCount();
        Assert.assertTrue( 4 == childCount );

        int width = View.MeasureSpec.getSize( widthMeasureSpec );
        int height = View.MeasureSpec.getSize( heightMeasureSpec );
        if ( 0 != width || 0 != height ) {
            setForPortrait( width, height );
        }

        for ( int ii = 0; ii < childCount; ii++ ) {
            final View child = getChildAt( ii );
            if ( GONE != child.getVisibility() ) {
                // Measure the child.
                measureChild( child, widthMeasureSpec, heightMeasureSpec );
            }
        }

        setMeasuredDimension( width, height );
    }

    // In portrait mode, board gets BD_PCT of the vertical space. Otherwise it
    // gets it all IFF the trade buttons aren't visible.
    @Override
    protected void onLayout( boolean changed, int left, int top,
                             int right, int bottom)
    {
        boolean haveTradeBar
            = GONE != findViewById(R.id.exchange_buttons).getVisibility();
        int boardHt = bottom - top;
        if ( haveTradeBar || s_isPortrait ) {
            boardHt = boardHt * TBAR_PCT_HOR / 100;
        }
        int boardWidth = right - left;
        if ( !s_isPortrait ) {
            boardWidth = boardWidth * TBAR_PCT_VERT / 100;
        }

        // layout the board
        BoardView board = (BoardView)getChildAt( 0 );
        board.layout( left, top, left + boardWidth, top + boardHt );

        // The trade bar
        if ( haveTradeBar ) {
            LinearLayout exchButtons = (LinearLayout)getChildAt( 1 );
            Assert.assertTrue( exchButtons.getId() == R.id.exchange_buttons );
            exchButtons.layout( left, top + boardHt, right, bottom );
        }

        // Now one of the toolbars
        View scrollView = getChildAt( s_isPortrait ? 3 : 2 );
        Assert.assertTrue( GONE != scrollView.getVisibility() );
        if ( s_isPortrait ) {
            top += boardHt;
        } else {
            left += boardWidth;
        }
        scrollView.layout( left, top, right, bottom );
    }

    private void setForPortrait( final int width, final int height )
    {
        if ( height != s_height || width != s_width ) {
            s_height = height;
            s_width = width;
            s_isPortrait = height > width;
            findViewById( R.id.tbar_parent_hor )
                .setVisibility( s_isPortrait ? VISIBLE : GONE );
            findViewById( R.id.tbar_parent_vert )
                .setVisibility( s_isPortrait ? GONE : VISIBLE );
     
            callSCL();
        }
    }

    private static void callSCL()
    {
        if ( 0 != s_width || 0 != s_height ) {
            if ( null != s_scl ) {
                SizeChangeListener scl = s_scl.get();
                if ( null != scl ) {
                    scl.sizeChanged( s_width, s_height, s_isPortrait );
                }
            }
        }
    }
}

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

import java.lang.ref.WeakReference;
import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.view.ViewGroup;
import android.view.View;
import android.view.View.MeasureSpec;
import android.graphics.Rect;


public class BoardContainer extends ViewGroup {
    private static final String TAG = BoardContainer.class.getSimpleName();
    // If the ratio of height/width exceeds this, use portrait layout
    private static final int PORTRAIT_THRESHHOLD = 105;

    private static final int BOARD_PCT_VERT = 90;
    private static final int BOARD_PCT_HOR = 85;

    private static final int BOARD_INDX = 0;
    private static final int EXCH_INDX = 1;
    private static final int VBAR_INDX = 2;
    private static final int HBAR_INDX = 3;

    private static boolean s_isPortrait = true; // initial assumption
    private static int s_width = 0;
    private static int s_height = 0;

    private Rect m_boardBounds;
    private Rect m_toolsBounds;

    interface SizeChangeListener {
        void sizeChanged( int width, int height, boolean isPortrait );
    }
    private static WeakReference<SizeChangeListener> s_scl;

    public static void registerSizeChangeListener(SizeChangeListener scl)
    {
        s_scl = new WeakReference<>(scl);
        callSCL();
    }

    public static boolean getIsPortrait() { return s_isPortrait; }

    public BoardContainer( Context context, AttributeSet as ) {
        super( context, as );
        s_height = s_width = 0;
        s_scl = null;
    }

    @Override
    protected void onMeasure( int widthMeasureSpec, int heightMeasureSpec )
    {
        int width = MeasureSpec.getSize( widthMeasureSpec );
        int height = MeasureSpec.getSize( heightMeasureSpec );
        if ( 0 != width || 0 != height ) {
            setForPortrait( width, height );

            // Add a margin of half a percent of the lesser of width,height
            int padding = (Math.min( width, height ) * 5) / 1000;
            figureBounds( padding, padding, width-(padding*2), height-(padding*2) );

            // Measure any toolbar first so we can take extra space for the
            // board
            int childCount = getChildCount();
            if ( 1 < childCount ) {
                Assert.assertTrue( 4 == childCount );

                // Measure the toolbar
                measureChild( s_isPortrait ? HBAR_INDX : VBAR_INDX, m_toolsBounds );
                adjustBounds();
                View child = getChildAt( s_isPortrait ? HBAR_INDX : VBAR_INDX );
                Log.i( TAG, "measured %s; passed ht: %d; got back ht: %d",
                       child.toString(), m_toolsBounds.height(),
                       child.getMeasuredHeight() );

                if ( haveTradeBar() ) {
                    // Measure the exchange buttons bar
                    measureChild( EXCH_INDX, m_toolsBounds );
                }
            }

            // Measure the board
            measureChild( BOARD_INDX, m_boardBounds );
        }
        setMeasuredDimension( width, height );
    }

    // In portrait mode, board gets BD_PCT of the vertical space. Otherwise it
    // gets it all IFF the trade buttons aren't visible.
    @Override
    protected void onLayout( boolean changed, int left, int top,
                             int right, int bottom )
    {
        // If this isn't true, need to refigure the rects
        // Assert.assertTrue( 0 == left && 0 == top );

        // layout the board
        layoutChild( BOARD_INDX, m_boardBounds );

        if ( 1 < getChildCount() ) {
            // The trade bar
            if ( haveTradeBar() ) {
                layoutChild( EXCH_INDX, m_toolsBounds );
            }

            // Now one of the toolbars
            layoutChild( s_isPortrait ? HBAR_INDX : VBAR_INDX, m_toolsBounds );
        }
    }

    private void measureChild( int index, Rect rect )
    {
        int childWidthSpec = MeasureSpec.makeMeasureSpec(rect.width(),
                                                         MeasureSpec.AT_MOST );
        int childHeightSpec = MeasureSpec.makeMeasureSpec(rect.height(),
                                                          MeasureSpec.AT_MOST );
        View view = getChildAt( index );
        measureChild( view, childWidthSpec, childHeightSpec );
    }

    private void layoutChild( int index, Rect rect )
    {
        View child = getChildAt( index );
        if ( GONE != child.getVisibility() ) {
            child.layout( rect.left, rect.top, rect.right, rect.bottom );
        }
    }

    private void setForPortrait( final int width, final int height )
    {
        if ( height != s_height || width != s_width ) {
            s_height = height;
            s_width = width;
            s_isPortrait = PORTRAIT_THRESHHOLD < (height*100) / width;
            findViewById( R.id.tbar_parent_hor )
                .setVisibility( s_isPortrait ? VISIBLE : GONE );
            findViewById( R.id.tbar_parent_vert )
                .setVisibility( s_isPortrait ? GONE : VISIBLE );

            callSCL();
        }
    }

    private void figureBounds( int left, int top, int width, int height )
    {
        int boardHeight = ( haveTradeBar() || s_isPortrait)
            ? height * (BOARD_PCT_VERT) / 100 : height;
        int boardWidth = s_isPortrait ? width : (width * BOARD_PCT_HOR) / 100;

        // board
        m_boardBounds = new Rect( left, top, left + boardWidth,
                                     top + boardHeight );
        // DbgUtils.logf( "BoardContainer: boardBounds: %s", boardBounds.toString() );
        // toolbar
        if ( s_isPortrait ) {
            top += boardHeight;
            height -= boardHeight;
        } else {
            left += boardWidth;
            width -= boardWidth;
        }
        m_toolsBounds = new Rect( left, top, left + width, top + height );
    }

    private void adjustBounds()
    {
        if ( s_isPortrait ) {
            int curHeight = m_toolsBounds.height();
            int newHeight = getChildAt( HBAR_INDX ).getMeasuredHeight();
            int diff = curHeight - newHeight;
            m_boardBounds.bottom += diff;
            m_toolsBounds.top += diff;
        } else {
            int curWidth = m_toolsBounds.width();
            int newWidth = getChildAt( VBAR_INDX ).getMeasuredWidth();
            int diff = curWidth - newWidth;
            m_boardBounds.right += diff;
            m_toolsBounds.left += diff;
        }
    }

    private boolean haveTradeBar()
    {
        boolean result = false;
        if ( s_isPortrait && 1 < getChildCount() ) {
            View bar = getChildAt( 1 );
            result = null != bar && GONE != bar.getVisibility();
        }
        return result;
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

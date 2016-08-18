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
import android.view.View.MeasureSpec;
import android.graphics.Rect;

import junit.framework.Assert;

public class BoardContainer extends ViewGroup {
    // If the ratio of height/width exceeds this, use portrait layout
    private static final int PORTRAIT_THRESHHOLD = 105;

    private static final int BOARD_PCT_HOR = 90;
    private static final int BOARD_PCT_VERT = 85;

    private static final int BOARD_INDX = 0;
    private static final int EXCH_INDX = 1;
    private static final int VBAR_INDX = 2;
    private static final int HBAR_INDX = 3;

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
        int width = MeasureSpec.getSize( widthMeasureSpec );
        int height = MeasureSpec.getSize( heightMeasureSpec );
        if ( 0 != width || 0 != height ) {
            setForPortrait( width, height );

            Rect[] rects = figureBounds( 0, 0, width, height );

            // Measure the board
            measureChild( BOARD_INDX, rects[0] );

            int childCount = getChildCount();
            if ( 1 < childCount ) {
                Assert.assertTrue( 4 == childCount );

                if ( haveTradeBar() ) {
                    // Measure the exchange buttons bar
                    measureChild( EXCH_INDX, rects[1] );
                }

                // Measure the toolbar
                measureChild( s_isPortrait ? HBAR_INDX : VBAR_INDX, rects[1] );
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
        Rect[] rects = figureBounds( left, top, right, bottom );

        // layout the board
        BoardView board = (BoardView)getChildAt( BOARD_INDX );
        layoutChild( board, rects[0] );

        if ( 1 < getChildCount() ) {

            // The trade bar
            if ( haveTradeBar() ) {
                LinearLayout exchButtons = (LinearLayout)getChildAt( EXCH_INDX );
                Assert.assertTrue( exchButtons.getId() == R.id.exchange_buttons );
                layoutChild( exchButtons, rects[1] );
            }

            // Now one of the toolbars
            View scrollView = getChildAt( s_isPortrait ? HBAR_INDX : VBAR_INDX );
            if ( GONE != scrollView.getVisibility() ) {
                layoutChild( scrollView, rects[1] );
            }
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

    private void layoutChild( View child, Rect rect )
    {
        child.layout( rect.left, rect.top, rect.right, rect.bottom );
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

    private Rect[] figureBounds( int left, int top, int width, int height )
    {
        int boardHeight = ( haveTradeBar() || s_isPortrait)
            ? height * BOARD_PCT_HOR / 100 : height;
        int boardWidth = s_isPortrait ? width : width * BOARD_PCT_VERT / 100;

        // board
        Rect boardBounds = new Rect( left, top, left + boardWidth,
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
        Rect toolsBounds = new Rect( left, top, left + width, top + height );
        // DbgUtils.logf( "BoardContainer: toolsBounds: %s", toolsBounds.toString() );
        return new Rect[] { boardBounds, toolsBounds };
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

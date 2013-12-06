/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2012 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.View;
import junit.framework.Assert;

public class ExpiringDelegate {

    private static final long INTERVAL_SECS = 3 * 24 * 60 * 60;
    // private static final long INTERVAL_SECS = 60 * 10;   // for testing

    private Context m_context;
    private View m_view;
    private boolean m_active = false;
    private int m_pct = -1;
    private int m_backPct = -1;
    private Drawable m_back = null;
    private boolean m_doFrame = false;
    private Handler m_handler;
    private boolean m_haveTurnLocal = false;
    private long m_startSecs;
    private Runnable m_runnable = null;
    private boolean m_selected;
    private Drawable m_origDrawable;
    // these can be static as drawing's all in same thread.
    private static Rect s_rect;
    private static Paint s_paint;
    private static float[] s_points;
    private static Drawable s_selDrawable;

    static {
        s_rect = new Rect();
        s_paint = new Paint(); 
        s_paint.setStyle(Paint.Style.STROKE);  
        s_paint.setStrokeWidth( 1 );
        s_points = new float[4*6];
        s_selDrawable = new ColorDrawable( XWApp.SEL_COLOR );
    }

    public ExpiringDelegate( Context context, View view )
    {
        m_context = context;
        m_view = view;
        m_origDrawable = view.getBackground();
    }

    public void setHandler( Handler handler )
    {
        m_handler = handler;
    }

    public void configure( boolean haveTurn, boolean haveTurnLocal, 
                           long startSecs )
    {
        m_active = haveTurn;
        m_doFrame = !haveTurnLocal;
        if ( haveTurn ) {
            m_startSecs = startSecs;
            m_haveTurnLocal = haveTurnLocal;
            figurePct();
            if ( haveTurnLocal ) {
                setBackground();
            } else {
                m_view.setBackgroundDrawable( null );
                m_view.setWillNotDraw( false );
            }
        }
    }

    public void setSelected( boolean selected )
    {
        m_selected = selected;
        if ( selected ) {
            m_origDrawable = m_view.getBackground();
            m_view.setBackgroundDrawable( s_selDrawable );
        } else {
            m_view.setBackgroundDrawable( m_origDrawable );
        }
    }

    public void onDraw( Canvas canvas ) 
    {
        if ( m_selected ) {
            // do nothing; the drawable's set already
        } else if ( m_active && m_doFrame ) {
            Assert.assertTrue( 0 <= m_pct && m_pct <= 100 );
            m_view.getDrawingRect( s_rect );
            int width = s_rect.width();
            int redWidth = width * m_pct / 100;
            Assert.assertTrue( redWidth <= width );

            // left edge
            addPoints( 0, s_rect.left, s_rect.top,
                       s_rect.left, s_rect.bottom - 1 );

            // left horizontals
            addPoints( 1, s_rect.left, s_rect.top, 
                       s_rect.left + redWidth, s_rect.top );
            addPoints( 2, s_rect.left, s_rect.bottom - 1,
                       s_rect.left + redWidth,
                       s_rect.bottom - 1 );

            // right horizontals
            addPoints( 3, s_rect.left + redWidth, s_rect.top, 
                       s_rect.right - 1, s_rect.top );
            addPoints( 4, s_rect.left + redWidth, s_rect.bottom - 1,
                       s_rect.right - 1, s_rect.bottom - 1 );

            // right edge
            addPoints( 5, s_rect.right - 1, s_rect.top,
                       s_rect.right - 1, s_rect.bottom );

            int offset = 0;
            int count = s_points.length;
            if ( 0 < redWidth ) {
                s_paint.setColor( Color.RED );
                canvas.drawLines( s_points, offset, count / 2, s_paint );
                count /= 2;
                offset += count;
            }
            if ( redWidth < width ) {
                s_paint.setColor( Color.GREEN );
            }
            canvas.drawLines( s_points, offset, count, s_paint );
        }
    }

    private void addPoints( int offset, int left, int top,
                            int right, int bottom )
    {
        offset *= 4;
        s_points[offset + 0] = left;
        s_points[offset + 1] = top;
        s_points[offset + 2] = right;
        s_points[offset + 3] = bottom;
    }

    private void setBackground()
    {
        Assert.assertTrue( m_active );
        Drawable back;
        if ( -1 != m_pct && m_backPct != m_pct ) {
            m_back = mkBackground( m_pct );
            m_backPct = m_pct;
        }
        if ( null != m_back ) {
            m_view.setBackgroundDrawable( m_back );
        }
    }

    private Drawable mkBackground( int pct )
    {
        Assert.assertTrue( 0 <= pct && pct <= 100 );
        Bitmap bm = Bitmap.createBitmap( 100, 1, Bitmap.Config.ARGB_8888 );
        Canvas canvas = new Canvas(bm);

        Paint paint = new Paint(); 
        paint.setStyle(Paint.Style.FILL);  
        paint.setColor( Color.RED ); 
        canvas.drawRect( 0, 0, pct, 1, paint );
        paint.setColor( Utils.TURN_COLOR ); 
        canvas.drawRect( pct, 0, 100, 1, paint );
        return new BitmapDrawable( m_context.getResources(), bm );
    }

    private void figurePct()
    {
        if ( 0 == m_startSecs ) {
            m_pct = 0;
        } else {
            long now = Utils.getCurSeconds();
            long passed = now - m_startSecs;
            m_pct = (int)((100 * passed) / INTERVAL_SECS);
            if ( m_pct > 100 ) {
                m_pct = 100;
            } else if ( null != m_handler ) {
                long onePct = INTERVAL_SECS / 100;
                long lastStart = m_startSecs + (onePct * m_pct);
                Assert.assertTrue( lastStart <= now );
                long nextStartIn = lastStart + onePct - now;
                // DbgUtils.logf( "pct change %d seconds from now", nextStartIn );

                m_handler.postDelayed( mkRunnable(), 1000 * nextStartIn ); // NPE
            }
        }
    }

    private Runnable mkRunnable()
    {
        if ( null == m_runnable ) {
            m_runnable = new Runnable() {
                    public void run() {
                        if ( XWApp.DEBUG_EXP_TIMERS ) {
                            DbgUtils.logf( "ExpiringDelegate: timer fired"
                                           + " for %H", this );
                        }
                        if ( m_active ) {
                            figurePct();
                            if ( m_haveTurnLocal ) {
                                m_back = null;
                                setBackground();
                            }
                            if ( XWApp.DEBUG_EXP_TIMERS ) {
                                DbgUtils.logf( "ExpiringDelegate: invalidating"
                                               + " view %H", m_view );
                            }
                            m_view.invalidate();
                        }
                    }
                };
        }
        return m_runnable;
    }
}

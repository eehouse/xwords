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
import android.graphics.drawable.Drawable;
import android.view.View;
import junit.framework.Assert;

public class ExpiringDelegate {
    // private static final long INTERVAL_SECS = 3 * 24 * 60 * 60;
    private static final long INTERVAL_SECS = 60 * 60;

    private Drawable m_back = null;
    private Context m_context;
    private View m_view;
    private int m_pct = -1;
    private boolean m_doFrame = false;
    // these could probably be static as drawing's all in same thread.
    private Rect m_rect;
    private Paint m_paint;
    private float[] m_points;
    private long m_startSecs;

    public ExpiringDelegate( Context context, View view, boolean haveTurn, 
                             boolean haveTurnLocal, long startSecs )
    {
        m_context = context;
        m_view = view;
        m_startSecs = startSecs;

        figurePct();

        if ( !haveTurn ) {
            // nothing to do
        } else if ( haveTurnLocal ) {
            setBackground();
        } else {
            m_view.setWillNotDraw( false );
            m_doFrame = true;   // required if setWillNotDraw() used?
        }
    }

    public void onDraw( Canvas canvas ) 
    {
        if ( m_doFrame ) {
            initDrawingIf();
            Assert.assertTrue( 0 <= m_pct && m_pct <= 100 );
            m_view.getDrawingRect( m_rect );
            int width = m_rect.width();
            int redWidth = width * m_pct / 100;
            Assert.assertTrue( redWidth <= width );

            // left edge
            addPoints( 0, m_rect.left, m_rect.top,
                       m_rect.left, m_rect.bottom - 1 );

            // left horizontals
            addPoints( 1, m_rect.left, m_rect.top, 
                       m_rect.left + redWidth, m_rect.top );
            addPoints( 2, m_rect.left, m_rect.bottom - 1,
                       m_rect.left + redWidth,
                       m_rect.bottom - 1 );

            // right horizontals
            addPoints( 3, m_rect.left + redWidth, m_rect.top, 
                       m_rect.right - 1, m_rect.top );
            addPoints( 4, m_rect.left + redWidth, m_rect.bottom - 1,
                       m_rect.right - 1, m_rect.bottom - 1 );

            // right edge
            addPoints( 5, m_rect.right - 1, m_rect.top,
                       m_rect.right - 1, m_rect.bottom );

            int offset = 0;
            int count = m_points.length;
            if ( 0 < redWidth ) {
                m_paint.setColor( Color.RED );
                canvas.drawLines( m_points, offset, count / 2, m_paint );
                count /= 2;
                offset += count;
            }
            if ( redWidth < width ) {
                m_paint.setColor( Color.GREEN );
            }
            canvas.drawLines( m_points, offset, count, m_paint );
        }
    }

    private void initDrawingIf()
    {
        if ( null == m_rect ) {
            m_rect = new Rect();
            m_paint = new Paint(); 
            m_paint.setStyle(Paint.Style.STROKE);  
            m_paint.setStrokeWidth( 1 );
            m_points = new float[4*6];
        }
    }

    private void addPoints( int offset, int left, int top,
                            int right, int bottom )
    {
        offset *= 4;
        m_points[offset + 0] = left;
        m_points[offset + 1] = top;
        m_points[offset + 2] = right;
        m_points[offset + 3] = bottom;
    }

    private void setBackground()
    {
        if ( null == m_back && -1 != m_pct ) {
            mkTurnIndicator();
        }
        if ( null != m_back ) {
            m_view.setBackgroundDrawable( m_back );
        }
    }

    private void mkTurnIndicator()
    {
        Assert.assertTrue( 0 <= m_pct && m_pct <= 100 );
        if ( null == m_back ) {
            Bitmap bm = Bitmap.createBitmap( 100, 1, Bitmap.Config.ARGB_8888 );
            Canvas canvas = new Canvas(bm);

            Paint paint = new Paint(); 
            paint.setStyle(Paint.Style.FILL);  
            paint.setColor( Color.RED ); 
            canvas.drawRect( 0, 0, m_pct, 1, paint );
            paint.setColor( Utils.TURN_COLOR ); 
            canvas.drawRect( m_pct, 0, 100, 1, paint );
            m_back = new BitmapDrawable( m_context.getResources(), bm );
        }
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
            } else {
                long onePct = INTERVAL_SECS / 100;
                long lastStart = m_startSecs + (onePct * m_pct);
                Assert.assertTrue( lastStart <= now );
                long nextStartIn = lastStart + onePct - now;
                DbgUtils.logf( "pct change %d seconds from now", nextStartIn );
            }
        }
    }
}

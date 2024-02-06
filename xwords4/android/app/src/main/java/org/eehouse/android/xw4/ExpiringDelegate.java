/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.View;


import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

public class ExpiringDelegate {
    private static final String TAG = ExpiringDelegate.class.getSimpleName();

    private static final long INTERVAL_SECS = 3 * 24 * 60 * 60;
    // private static final long INTERVAL_SECS = 60 * 10;   // for testing

    private static boolean s_kitkat =
        19 <= Integer.valueOf( android.os.Build.VERSION.SDK );

    private Context m_context;
    private View m_view;
    private boolean m_active = false;
    private int m_pct = -1;
    private int m_backPct = -1;
    private Drawable m_back = null;
    private boolean m_doFrame = false;
    private boolean m_haveTurnLocal = false;
    private long m_startSecs;
    private Runnable m_runnable = null;
    private boolean m_selected;
    // these can be static as drawing's all in same thread.
    private static Rect s_rect;
    private static Paint s_paint;
    private static float[] s_points;
    private DrawSelDelegate m_dsdel;

    // Combine all the timers into one. Since WeakReferences to the same
    // object aren't equal we need a separate set of their hash codes to
    // prevent storing duplicates.
    private static class ExpUpdater implements Runnable {
        private Handler m_handler;
        private ArrayList<WeakReference<ExpiringDelegate>> m_refs
            = new ArrayList<WeakReference<ExpiringDelegate>>();
        private Set<Integer> m_hashes = new HashSet<>();

        @Override
        public void run() {
            int sizeBefore;
            ArrayList<ExpiringDelegate> dlgts = new ArrayList<>();
            synchronized( this ) {
                sizeBefore = m_refs.size();
                m_hashes.clear();
                Iterator<WeakReference<ExpiringDelegate>> iter = m_refs.iterator();
                while ( iter.hasNext() ) {
                    WeakReference<ExpiringDelegate> ref = iter.next();
                    ExpiringDelegate dlgt = ref.get();
                    if ( null == dlgt/* || dlgts.contains( dlgt )*/ ) {
                        iter.remove();
                    } else {
                        dlgts.add(dlgt);
                        m_hashes.add( dlgt.hashCode() );
                    }
                }
            }

            Log.d( TAG, "ref had %d refs, now has %d expiringdelegate views",
                   sizeBefore, dlgts.size() );

            for ( ExpiringDelegate dlgt : dlgts ) {
                dlgt.timerFired();
            }

            reschedule();
        }

        private void reschedule()
        {
            m_handler.postDelayed( this, INTERVAL_SECS * 1000 / 100 );
        }

        private void add( ExpiringDelegate self )
        {
            int hash = self.hashCode();
            synchronized( this ) {
                if ( ! m_hashes.contains( hash ) ) {
                    m_hashes.add( hash );
                    m_refs.add( new WeakReference<>(self) );
                }
            }
        }

        private void setHandler( Handler handler )
        {
            if ( handler != m_handler ) {
                Log.d( TAG, "handler changing from %H to %H", m_handler, handler );
                m_handler = handler;
                reschedule();
            }
        }
    }

    private static ExpUpdater s_updater;
    static {
        s_rect = new Rect();
        s_paint = new Paint();
        s_paint.setStyle(Paint.Style.STROKE);
        s_paint.setStrokeWidth( 1 );
        s_points = new float[4*6];
        s_updater = new ExpUpdater();
    }

    public ExpiringDelegate( Context context, View view )
    {
        m_context = context;
        m_view = view;
        m_dsdel = new DrawSelDelegate( view );
    }

    public void setHandler( Handler handler )
    {
        s_updater.setHandler( handler );
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
        m_dsdel.showSelected( m_selected );
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

            if ( s_kitkat ) {
                ++s_rect.top;
                ++s_rect.left;
            }

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
                s_paint.setColor( XWApp.RED );
                canvas.drawLines( s_points, offset, count / 2, s_paint );
                count /= 2;
                offset += count;
            }
            if ( redWidth < width ) {
                s_paint.setColor( XWApp.GREEN );
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
        paint.setColor( XWApp.RED );
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
            } else if ( m_pct < 0 ) {
                m_pct = 0;
            } else {
                s_updater.add( this );
            }
        }
    }

    private void timerFired()
    {
        if ( m_active ) {
            figurePct();
            if ( m_haveTurnLocal ) {
                m_back = null;
                setBackground();
            }
            m_view.invalidate();
        }
    }

}

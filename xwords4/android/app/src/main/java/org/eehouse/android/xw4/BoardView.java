/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2012 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Build;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;


import org.eehouse.android.xw4.jni.BoardDims;
import org.eehouse.android.xw4.jni.BoardHandler;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.SyncedDraw;
import org.eehouse.android.xw4.jni.XwJNI;

public class BoardView extends View implements BoardHandler, SyncedDraw {
    private static final String TAG = BoardView.class.getSimpleName();

    private static final float MIN_FONT_DIPS = 10.0f;
    private static final int MULTI_INACTIVE = -1;

    private static boolean s_isFirstDraw;
    private static int s_curGameID;
    private static Bitmap s_bitmap;    // the board

    private static final int PINCH_THRESHOLD = 40;

    private Context m_context;
    private int m_defaultFontHt;
    private int m_mediumFontHt;
    private Runnable m_invalidator;
    private XwJNI.GamePtr m_jniGamePtr;
    private CurGameInfo m_gi;
    private boolean m_isSolo;
    private int m_layoutWidth;
    private int m_dimsTossCount; // hack hack hack!!
    private int m_layoutHeight;
    private BoardCanvas m_canvas;    // owns the bitmap
    private JNIThread m_jniThread;
    private Activity m_parent;
    private boolean m_measuredFromDims = false;
    private BoardDims m_dims;
    private CommsConnTypeSet m_connTypes = null;

    private int m_lastSpacing = MULTI_INACTIVE;


    // called when inflating xml
    public BoardView( Context context, AttributeSet attrs )
    {
        super( context, attrs );

        m_context = context;

        final float scale = getResources().getDisplayMetrics().density;
        m_defaultFontHt = (int)(MIN_FONT_DIPS * scale + 0.5f);
        m_mediumFontHt = m_defaultFontHt * 3 / 2;
        m_invalidator = new Runnable() {
                @Override
                public void run() {
                    invalidate();
                }
            };
    }

    @Override
    public boolean onTouchEvent( MotionEvent event )
    {
        boolean wantMore = null != m_jniThread;
        if ( wantMore ) {
            int action = event.getAction();
            int xx = (int)event.getX();
            int yy = (int)event.getY();

            switch ( action ) {
            case MotionEvent.ACTION_DOWN:
                m_lastSpacing = MULTI_INACTIVE;
                if ( ConnStatusHandler.handleDown( xx, yy ) ) {
                    // do nothing
                } else if ( XwJNI.board_containsPt( m_jniGamePtr, xx, yy ) ) {
                    handle( JNIThread.JNICmd.CMD_PEN_DOWN, xx, yy );
                } else {
                    Log.d( TAG, "onTouchEvent(): in white space" );
                }
                break;
            case MotionEvent.ACTION_MOVE:
                if ( ConnStatusHandler.handleMove( xx, yy ) ) {
                    // do nothing
                } else if ( MULTI_INACTIVE == m_lastSpacing ) {
                    handle( JNIThread.JNICmd.CMD_PEN_MOVE, xx, yy );
                } else {
                    int zoomBy = figureZoom( event );
                    if ( 0 != zoomBy ) {
                        handle( JNIThread.JNICmd.CMD_ZOOM,
                                            zoomBy < 0 ? -2 : 2 );
                    }
                }
                break;
            case MotionEvent.ACTION_UP:
                if ( ConnStatusHandler.handleUp( xx, yy ) ) {
                    // do nothing
                } else {
                    handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy );
                }
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_POINTER_2_DOWN:
                handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy );
                m_lastSpacing = getSpacing( event );
                break;
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_POINTER_2_UP:
                m_lastSpacing = MULTI_INACTIVE;
                break;
            default:
                Log.w( TAG, "onTouchEvent: unknown action: %d", action );
                break;
            }
        }
        return wantMore;            // true required to get subsequent events
    }

    @Override
    protected void onMeasure( int widthMeasureSpec, int heightMeasureSpec )
    {
        // Log.d( TAG, "onMeasure(width: %s, height: %s)",
        //        MeasureSpec.toString( widthMeasureSpec ),
        //        MeasureSpec.toString( heightMeasureSpec ) );

        if ( null != m_dims ) {
            if ( BoardContainer.getIsPortrait() != (m_dims.height > m_dims.width) ) {
                // square possible; will break above! No. tested by forceing square
                Log.d( TAG, "onMeasure: discarding m_dims" );
                if ( ++m_dimsTossCount < 4 ) {
                    m_dims = null;
                    m_layoutWidth = m_layoutHeight = 0;
                } else {
                    Log.d( TAG, "onMeasure(): unexpected width (%d) to height (%d) ratio"
                           + "; proceeding", m_dims.width, m_dims.height );
                }
            }
        }

        int width, height;
        m_measuredFromDims = null != m_dims;
        if ( m_measuredFromDims ) {
            height = m_dims.height;
            width = m_dims.width;
        } else {
            width = View.MeasureSpec.getSize( widthMeasureSpec );
            height = View.MeasureSpec.getSize( heightMeasureSpec );
        }

        int minHeight = getSuggestedMinimumHeight();
        if ( height < minHeight ) {
            height = minHeight;
        }
        int minWidth = getSuggestedMinimumWidth();
        if ( width < minWidth ) {
            width = minWidth;
        }
        setMeasuredDimension( width, height );
        // Log.d( TAG, "onMeasure: calling setMeasuredDimension( width=%d, height=%d )",
        //        width, height );
    }

    // @Override
    // public void onSizeChanged( int width, int height, int oldWidth, int oldHeight )
    // {
    //     DbgUtils.logf( "BoardView.onSizeChanged(): width: %d => %d; height: %d => %d",
    //                    oldWidth, width, oldHeight, height );
    //     super.onSizeChanged( width, height, oldWidth, oldHeight );
    // }

    // This will be called from the UI thread
    @Override
    protected void onDraw( Canvas canvas )
    {
        synchronized( this ) {
            if ( !layoutBoardOnce() ) {
                // Log.d( TAG, "onDraw(): layoutBoardOnce() failed" );
            } else if ( ! m_measuredFromDims ) {
                // Log.d( TAG, "onDraw(): m_measuredFromDims not set" );
            } else {
                Bitmap bitmap = s_bitmap;
                if ( Build.VERSION.SDK_INT >= Build.VERSION_CODES.N ) {
                    bitmap = Bitmap.createBitmap( bitmap );
                }
                canvas.drawBitmap( bitmap, 0, 0, new Paint() );

                ConnStatusHandler.draw( m_context, canvas, getResources(),
                                        m_connTypes, m_isSolo );
            }
        }
    }

    private boolean layoutBoardOnce()
    {
        final int width = getWidth();
        final int height = getHeight();
        boolean layoutDone = width == m_layoutWidth && height == m_layoutHeight;
        if ( layoutDone ) {
            // Log.d( TAG, "layoutBoardOnce(): layoutDone true" );
        } else if ( null == m_gi ) {
            // nothing to do either
            Log.d( TAG, "layoutBoardOnce(): no m_gi" );
        } else if ( null == m_jniThread ) {
            // nothing to do either
            Log.d( TAG, "layoutBoardOnce(): no m_jniThread" );
        } else if ( null == m_dims ) {
            // Log.d( TAG, "layoutBoardOnce(): null m_dims" );
            // m_canvas = null;
            // need to synchronize??
            Paint paint = new Paint();
            paint.setTextSize( m_mediumFontHt );
            Rect scratch = new Rect();
            String timerTxt = "-00:00";
            paint.getTextBounds( timerTxt, 0, timerTxt.length(), scratch );
            int timerWidth = scratch.width();
            int fontWidth =
                Math.min(m_defaultFontHt, timerWidth / timerTxt.length());
            Log.d( TAG, "layoutBoardOnce(): posting JNICmd.CMD_LAYOUT(w=%d, h=%d)",
                   width, height );
            handle( JNIThread.JNICmd.CMD_LAYOUT, width, height,
                    fontWidth, m_defaultFontHt );
            // We'll be back....
        } else {
            Log.d( TAG, "layoutBoardOnce(): DOING IT" );
            // If board size has changed we need a new bitmap
            int bmHeight = 1 + m_dims.height;
            int bmWidth = 1 + m_dims.width;
            if ( null != s_bitmap ) {
                if ( s_bitmap.getHeight() != bmHeight
                     || s_bitmap.getWidth() != bmWidth ) {
                    s_bitmap = null;
                    m_canvas = null;
                }
            }

            if ( null == s_bitmap ) {
                s_bitmap = Bitmap.createBitmap( bmWidth, bmHeight,
                                                Bitmap.Config.ARGB_8888 );
            } else if ( s_isFirstDraw ) {
                // clear so prev game doesn't seem to appear briefly.  Color
                // doesn't seem to matter....
                s_bitmap.eraseColor( 0 );
            }
            if ( null == m_canvas ) {
                m_canvas = new BoardCanvas( m_parent, s_bitmap, m_jniThread,
                                            m_dims );
            } else {
                m_canvas.setJNIThread( m_jniThread );
            }
            handle( JNIThread.JNICmd.CMD_SETDRAW, m_canvas );
            handle( JNIThread.JNICmd.CMD_DRAW );

            // set so we know we're done
            m_layoutWidth = width;
            m_layoutHeight = height;
            layoutDone = true;
        }
        // Log.d( TAG, "layoutBoardOnce()=>%b", layoutDone );
        return layoutDone;
    } // layoutBoardOnce

    // BoardHandler interface implementation
    @Override
    public void startHandling( Activity parent, JNIThread thread,
                               CommsConnTypeSet connTypes )
    {
        Log.d( TAG, "startHandling(thread=%H, parent=%s)", thread, parent );
        Assert.assertTrue( null != parent || !BuildConfig.DEBUG );
        m_parent = parent;
        m_jniThread = thread;
        m_jniGamePtr = thread.getGamePtr(); // .retain()?
        m_gi = thread.getGI();
        m_isSolo = CurGameInfo.DeviceRole.SERVER_STANDALONE == m_gi.serverRole;
        m_connTypes = connTypes;
        m_layoutWidth = 0;
        m_layoutHeight = 0;

        s_isFirstDraw = s_curGameID != m_gi.gameID;
        s_curGameID = m_gi.gameID;

        // Set the jni layout if we already have one
        if ( null != m_dims ) {
            handle( JNIThread.JNICmd.CMD_LAYOUT, m_dims );
        }

        // Make sure we draw.  Sometimes when we're reloading after
        // an obsuring Activity goes away we otherwise won't.
        invalidate();
    }

    @Override
    public void stopHandling()
    {
        m_jniThread = null;
        m_jniGamePtr = null;
        if ( null != m_canvas ) {
            m_canvas.setJNIThread( null );
        }
    }

    // SyncedDraw interface implementation
    @Override
    public void doJNIDraw()
    {
        boolean drew = false;
        synchronized( this ) {
            if ( null != m_jniGamePtr ) {
                drew = XwJNI.board_draw( m_jniGamePtr );
            }
        }

        // Force update now that we have bits to copy. I don't know why (yet),
        // but on older versions of Android we need to run this even if drew
        // is false
        if ( null != m_parent ) {
            m_parent.runOnUiThread( m_invalidator );
        }
    }

    @Override
    public void dimsChanged( BoardDims dims )
    {
        m_dims = dims;
        m_parent.runOnUiThread( new Runnable() {
                @Override
                public void run()
                {
                    requestLayout();
                }
            });
    }

    protected void orientationChanged()
    {
        m_dims = null;
        m_layoutWidth = m_layoutHeight = 0;
        m_dimsTossCount = 0;
        requestLayout();
    }

    public void setInTrade( boolean inTrade )
    {
        if ( null != m_canvas ) {
            m_canvas.setInTrade( inTrade );
        }
    }

    public int getCurPlayer()
    {
        return null == m_canvas? -1 : m_canvas.getCurPlayer();
    }

    public int curPending()
    {
        return null == m_canvas? 0 : m_canvas.curPending();
    }

    private int getSpacing( MotionEvent event )
    {
        int result;
        if ( 1 == event.getPointerCount() ) {
            result = MULTI_INACTIVE;
        } else {
            float xx = event.getX( 0 ) - event.getX( 1 );
            float yy = event.getY( 0 ) - event.getY( 1 );
            result = (int)Math.sqrt( (xx * xx) + (yy * yy) );
        }
        return result;
    }

    private int figureZoom( MotionEvent event )
    {
        int zoomDir = 0;
        if ( MULTI_INACTIVE != m_lastSpacing ) {
            int newSpacing = getSpacing( event );
            int diff = Math.abs( newSpacing - m_lastSpacing );
            if ( diff > PINCH_THRESHOLD ) {
                zoomDir = newSpacing < m_lastSpacing? -1 : 1;
                m_lastSpacing = newSpacing;
            }
        }
        return zoomDir;
    }

    private void handle( JNIThread.JNICmd cmd, Object... args )
    {
        if ( null == m_jniThread ) {
            Log.w( TAG, "not calling handle(%s)", cmd.toString() );
            DbgUtils.printStack( TAG );
        } else {
            m_jniThread.handle( cmd, args );
        }
    }
}

/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
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

import android.view.View;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.content.Context;
import android.util.AttributeSet;
import org.eehouse.android.xw4.jni.*;
import android.view.MotionEvent;
import android.graphics.drawable.Drawable;
import android.content.res.Resources;
import android.graphics.Paint.FontMetricsInt;
import android.os.Build;
import android.os.Handler;
import java.nio.IntBuffer;
import android.util.FloatMath;

import junit.framework.Assert;

public class BoardView extends View implements BoardHandler, SyncedDraw {

    private static final float MIN_FONT_DIPS = 14.0f;
    private static final int MULTI_INACTIVE = -1;

    private static Bitmap s_bitmap;    // the board
    private static final int PINCH_THRESHOLD = 40;

    private Context m_context;
    private Paint m_drawPaint;
    private int m_defaultFontHt;
    private int m_mediumFontHt;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    private int m_layoutWidth;
    private int m_layoutHeight;
    private BoardCanvas m_canvas;    // owns the bitmap
    private int m_trayOwner = -1;
    private boolean m_blackArrow;
    private boolean m_inTrade = false;
    // m_backgroundUsed: alpha not set ensures inequality
    private int m_backgroundUsed = 0x00000000;
    private JNIThread m_jniThread;
    private XWActivity m_parent;
    private String[][] m_scores;
    private String[] m_dictChars;
    private Rect m_boundsScratch;
    private String m_remText;
    private int m_dictPtr = 0;
    private int m_lastSecsLeft;
    private int m_lastTimerPlayer;
    private int m_pendingScore;
    private boolean m_useCommon;
    private CommsAddrRec.CommsConnType m_connType = 
        CommsAddrRec.CommsConnType.COMMS_CONN_NONE;

    private int m_lastSpacing = MULTI_INACTIVE;


    // called when inflating xml
    public BoardView( Context context, AttributeSet attrs ) 
    {
        super( context, attrs );

        m_context = context;

        final float scale = getResources().getDisplayMetrics().density;
        m_defaultFontHt = (int)(MIN_FONT_DIPS * scale + 0.5f);
        m_mediumFontHt = m_defaultFontHt * 3 / 2;

        m_drawPaint = new Paint();

        m_boundsScratch = new Rect();

        // m_bonusSummaries = new String[5];
        // int[] ids = { R.string.bonus_l2x_summary,
        //               R.string.bonus_w2x_summary ,
        //               R.string.bonus_l3x_summary,
        //               R.string.bonus_w3x_summary };
        // for ( int ii = 0; ii < ids.length; ++ii ) {
        //     m_bonusSummaries[ ii+1 ] = getResources().getString( ids[ii] );
        // }
    }

    @Override
    public boolean onTouchEvent( MotionEvent event ) 
    {
        int action = event.getAction();
        int xx = (int)event.getX();
        int yy = (int)event.getY();
        
        switch ( action ) {
        case MotionEvent.ACTION_DOWN:
            m_lastSpacing = MULTI_INACTIVE;
            if ( !ConnStatusHandler.handleDown( xx, yy ) ) {
                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_DOWN, xx, yy );
            }
            break;
        case MotionEvent.ACTION_MOVE:
            if ( ConnStatusHandler.handleMove( xx, yy ) ) {
            } else if ( MULTI_INACTIVE == m_lastSpacing ) {
                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_MOVE, xx, yy );
            } else {
                int zoomBy = figureZoom( event );
                if ( 0 != zoomBy ) {
                    m_jniThread.handle( JNIThread.JNICmd.CMD_ZOOM, 
                                        zoomBy < 0 ? -2 : 2 );
                }
            }
            break;
        case MotionEvent.ACTION_UP:
            if ( ConnStatusHandler.handleUp( xx, yy ) ) {
                // do nothing
            } else {
                m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy );
            }
            break;
        case MotionEvent.ACTION_POINTER_DOWN:
        case MotionEvent.ACTION_POINTER_2_DOWN:
            m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy );
            m_lastSpacing = getSpacing( event );
            break;
        case MotionEvent.ACTION_POINTER_UP:
        case MotionEvent.ACTION_POINTER_2_UP:
            m_lastSpacing = MULTI_INACTIVE;
            break;
        default:
            DbgUtils.logf( "onTouchEvent: unknown action: %d", action );
            break;
        }

        return true;             // required to get subsequent events
    }

    // private void printMode( String comment, int mode )
    // {
    //     comment += ": ";
    //     switch( mode ) {
    //     case View.MeasureSpec.AT_MOST:
    //         comment += "AT_MOST";
    //         break;
    //     case View.MeasureSpec.EXACTLY:
    //         comment += "EXACTLY";
    //         break;
    //     case View.MeasureSpec.UNSPECIFIED:
    //         comment += "UNSPECIFIED";
    //         break;
    //     default:
    //         comment += "<bogus>";
    //     }
    //     DbgUtils.logf( comment );
    // }

    @Override
    protected void onMeasure( int widthMeasureSpec, int heightMeasureSpec )
    {
        // One of the android sample apps ignores mode entirely:
        // int w = MeasureSpec.getSize(widthMeasureSpec);
        // int h = MeasureSpec.getSize(heightMeasureSpec);
        // int d = w == 0 ? h : h == 0 ? w : w < h ? w : h;
        // setMeasuredDimension(d, d);

        int width = View.MeasureSpec.getSize( widthMeasureSpec );
        int height = View.MeasureSpec.getSize( heightMeasureSpec );

        int heightMode = View.MeasureSpec.getMode( heightMeasureSpec );
        // printMode( "heightMode", heightMode );

        m_useCommon = 
            XWPrefs.getPrefsBoolean( m_context, 
                                     R.string.key_enable_commlayt, false );
        BoardDims dims = figureBoardDims( width, height );
        // If I let the spec tell me whether I can reduce the width
        // then I don't change it on the second pass, but if I ignore
        // that it works (even though the docs say my change is what
        // will be ignored.)  I'm probably specifying something
        // incorrectly in the layout xml file.
        if ( false ) {
            height = resolveSize( dims.height, heightMeasureSpec );
            width = resolveSize( dims.width, widthMeasureSpec );
        } else {
            height = dims.height;
            width = dims.width;
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
    }

    // @Override
    // protected void onLayout( boolean changed, int left, int top, 
    //                          int right, int bottom )
    // {
    //     DbgUtils.logf( "BoardView.onLayout(%b, %d, %d, %d, %d)",
    //                    changed, left, top, right, bottom );
    //     super.onLayout( changed, left, top, right, bottom );
    // }

    // @Override
    // protected void onSizeChanged( int width, int height, 
    //                               int oldWidth, int oldHeight )
    // {
    //     DbgUtils.logf( "BoardView.onSizeChanged(%d,%d,%d,%d)", width, height, 
    //                    oldWidth, oldHeight );
    //     super.onSizeChanged( width, height, oldWidth, oldHeight );
    // }

    // This will be called from the UI thread
    @Override
    protected void onDraw( Canvas canvas ) 
    {
        synchronized( this ) {
            if ( layoutBoardOnce() ) {
                canvas.drawBitmap( s_bitmap, 0, 0, m_drawPaint );
                ConnStatusHandler.draw( m_context, canvas, getResources(), 
                                        0, 0, m_connType );
            }
        }
    }

    private BoardDims figureBoardDims( int width, int height )
    {
        BoardDims result = new BoardDims();
        boolean squareTiles = XWPrefs.getSquareTiles( m_context );

        Paint paint = new Paint();
        paint.setTextSize( m_mediumFontHt );
        paint.getTextBounds( "-00:00", 0, 6, m_boundsScratch );
        int timerWidth = m_boundsScratch.width();

        if ( m_useCommon ) {
            Rect bounds = new Rect( 0, 0, width, height );
            int fontWidth = timerWidth / 6;
            XwJNI.board_figureLayout( m_jniGamePtr, m_gi, m_defaultFontHt, 
                                      fontWidth, squareTiles, bounds, result );
    
        } else {
            int nCells = m_gi.boardSize;
            int maxCellSize = 4 * m_defaultFontHt;
            int trayHt;
            int scoreHt;
            int wantHt;
            int nToScroll;

            for ( boolean firstPass = true; ; ) {
                result.width = width;

                int cellSize = width / nCells;
                if ( cellSize > maxCellSize ) {
                    cellSize = maxCellSize;

                    int boardWidth = nCells * cellSize;
                    result.width = boardWidth;
                }
                result.maxCellSize = maxCellSize;

                // Now determine if vertical scrolling will be necessary.
                // There's a minimum tray and scoreboard height.  If we can
                // fit them and all cells no scrolling's needed.  Otherwise
                // determine the minimum number that must be hidden to fit.
                // Finally grow scoreboard and tray to use whatever's left.
                trayHt = 2 * cellSize;
                scoreHt = (cellSize * 3) / 2;
                wantHt = trayHt + scoreHt + (cellSize * nCells);
                if ( wantHt <= height ) {
                    nToScroll = 0;
                } else {
                    // Scrolling's required if we use cell width sufficient to
                    // fill the screen.  But perhaps we don't need to.
                    int cellWidth = 2 * (height / ( 4 + 3 + (2*nCells)));
                    if ( firstPass && cellWidth >= m_defaultFontHt ) {
                        firstPass = false;
                        width = nCells * cellWidth;
                        continue;
                    } else {
                        nToScroll = nCells - ((height - trayHt - scoreHt) / cellSize);
                    }
                }

                int heightUsed = trayHt + scoreHt + (nCells - nToScroll) * cellSize;
                int heightLeft = height - heightUsed;
                if ( 0 < heightLeft ) {
                    if ( heightLeft > (cellSize * 3 / 2) ) {
                        heightLeft = cellSize * 3 / 2;
                    }
                    heightLeft /= 3;
                    scoreHt += heightLeft;

                    trayHt += heightLeft * 2;
                    if ( squareTiles && trayHt > (width / 7) ) {
                        trayHt = width / 7;
                    }
                    heightUsed = trayHt + scoreHt + ((nCells - nToScroll) * cellSize);
                }

                result.trayHt = trayHt;
                result.scoreHt = scoreHt;

                result.boardHt = cellSize * nCells;
                result.trayTop = scoreHt + (cellSize * (nCells-nToScroll));
                result.height = heightUsed;
                result.cellSize = cellSize;

                if ( m_gi.timerEnabled ) {
                    result.timerWidth = timerWidth;
                }
                break;
            }
        }

        return result;
    } // figureBoardDims

    private boolean layoutBoardOnce() 
    {
        final int width = getWidth();
        final int height = getHeight();
        boolean layoutDone = width == m_layoutWidth && height == m_layoutHeight;
        if ( layoutDone ) {
            // nothing to do
        } else if ( null == m_gi ) {
            // nothing to do either
        } else {
            m_layoutWidth = width;
            m_layoutHeight = height;
            // m_fontDims = null; // force recalc of font
            // m_letterRect = null;
            // m_valRect = null;

            BoardDims dims = figureBoardDims( width, height );

            // If board size has changed we need a new bitmap
            int bmHeight = 1 + dims.height;
            int bmWidth = 1 + dims.width;
            if ( null != s_bitmap ) {
                if ( s_bitmap.getHeight() != bmHeight
                     || s_bitmap.getWidth() != bmWidth ) {
                    s_bitmap = null;
                }
            }

            if ( null == s_bitmap ) {
                s_bitmap = Bitmap.createBitmap( bmWidth, bmHeight,
                                                Bitmap.Config.ARGB_8888 );
            }
            m_canvas = new BoardCanvas( m_context, s_bitmap, m_jniThread );
            XwJNI.board_setDraw( m_jniGamePtr, m_canvas );

            // need to synchronize??
            m_jniThread.handle( JNIThread.JNICmd.CMD_LAYOUT, dims, 
                                m_useCommon );
            m_jniThread.handle( JNIThread.JNICmd.CMD_DRAW );
            layoutDone = true;
        }
        return layoutDone;
    } // layoutBoardOnce

    // BoardHandler interface implementation
    public void startHandling( XWActivity parent, JNIThread thread, 
                               int gamePtr, CurGameInfo gi, 
                               CommsAddrRec.CommsConnType connType ) 
    {
        m_parent = parent;
        m_jniThread = thread;
        m_jniGamePtr = gamePtr;
        m_gi = gi;
        m_connType = connType;
        m_layoutWidth = 0;
        m_layoutHeight = 0;

        // Make sure we draw.  Sometimes when we're reloading after
        // an obsuring Activity goes away we otherwise won't.
        invalidate();
    }

    // SyncedDraw interface implementation
    public void doJNIDraw()
    {
        boolean drew;
        synchronized( this ) {
            drew = XwJNI.board_draw( m_jniGamePtr );
        }
        if ( !drew ) {
            DbgUtils.logf( "doJNIDraw: draw not complete" );
        }
    }

    public void setInTrade( boolean inTrade ) 
    {
        m_inTrade = inTrade;
        m_jniThread.handle( JNIThread.JNICmd.CMD_INVALALL );
    }

    public int getCurPlayer()
    {
        return m_trayOwner;
    }

    public int curPending() 
    {
        return m_pendingScore;
    }

    public Bitmap getScaledBoard()
    {
        Bitmap result = null;
        if ( GitVersion.THUMBNAIL_SUPPORTED ) {
            int divisor = XWPrefs.getThumbScale( m_context );

            if ( 0 < divisor ) {
                int[] dims = new int[2];
                Rect rect = new Rect();
                XwJNI.board_getActiveRect( m_jniGamePtr, rect, dims );

                Bitmap tmpb = 
                    Bitmap.createBitmap( s_bitmap, rect.left, rect.top,
                                         1 + rect.width(), 1 + rect.height() );

                result = Bitmap.createScaledBitmap( tmpb,
                                                    rect.width() / divisor,
                                                    rect.height() / divisor,
                                                    false );
            }
        }
        return result;
    }

    private int getSpacing( MotionEvent event ) 
    {
        int result;
        if ( 1 == event.getPointerCount() ) {
            result = MULTI_INACTIVE;
        } else {
            float xx = event.getX( 0 ) - event.getX( 1 );
            float yy = event.getY( 0 ) - event.getY( 1 );
            result = (int)FloatMath.sqrt( (xx * xx) + (yy * yy) );
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

}

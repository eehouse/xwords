/* -*- compile-command: "cd ../../../../../; ant reinstall"; -*- */

package org.eehouse.android.xw4;

import android.view.View;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Bitmap;
import android.content.Context;
import android.util.AttributeSet;
import org.eehouse.android.xw4.jni.*;
import android.view.MotionEvent;
import android.graphics.drawable.Drawable;
import android.content.res.Resources;

public class BoardView extends View implements DrawCtx, 
                                               BoardHandler {

    private Paint m_fillPaint;
    private Paint m_strokePaint;
    private int m_jniGamePtr;
    private CurGameInfo m_gi;
    private boolean m_boardSet = false;
    private Canvas m_canvas;
    private Bitmap m_bitmap;
    private int m_trayOwner;
    private Rect m_valRect;
    private Rect m_letterRect;
    private Drawable m_rightArrow;
    private Drawable m_downArrow;
    private Drawable m_origin;
    private int m_top, m_left;
    private JNIThread m_jniThread;

    private static final int BLACK = 0xFF000000;
    private static final int WHITE = 0xFFFFFFFF;
    private static final int TILE_BACK = 0xFFFFFF99;
    private int [] m_bonusColors = { WHITE,         // BONUS_NONE
                                     0xFFAFAF00,	  /* bonus 1 */
                                     0xFF00AFAF,
                                     0xFFAF00AF,
                                     0xFFAFAFAF };
    private static final int[] m_playerColors = {
        0xFF000000,
        0xFFFF0000,
        0xFF0000FF,
        0xFF008F00,
    };



    public BoardView( Context context ) 
    {
        super( context );
        init();
    }

    // called when inflating xml
    public BoardView( Context context, AttributeSet attrs ) 
    {
        super( context, attrs );
        init();
    }

    // public boolean onClick( View view ) {
    //     Utils.logf( "onClick called" );
    //     return view == this;
    // }

    public boolean onTouchEvent( MotionEvent event ) 
    {
        int action = event.getAction();
        int xx = (int)event.getX() - m_left;
        int yy = (int)event.getY() - m_top;
        
        switch ( action ) {
        case MotionEvent.ACTION_DOWN:
            m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_DOWN, xx, yy );
            break;
        case MotionEvent.ACTION_MOVE:
            m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_MOVE, xx, yy );
            break;
        case MotionEvent.ACTION_UP:
            m_jniThread.handle( JNIThread.JNICmd.CMD_PEN_UP, xx, yy );
            break;
        default:
            Utils.logf( "unknown action: " + action );
            Utils.logf( event.toString() );
        }

        return true;             // required to get subsequent events
    }

    protected void onDraw( Canvas canvas ) 
    {
        if ( layoutBoardOnce() ) {
            canvas.drawBitmap( m_bitmap, m_left, m_top, new Paint() );
        }
    }

    private void init()
    {
        m_fillPaint = new Paint();
        m_fillPaint.setTextAlign( Paint.Align.CENTER ); // center horizontally
        m_strokePaint = new Paint();
        m_strokePaint.setStyle( Paint.Style.STROKE );

        Resources res = getResources();
        m_rightArrow = res.getDrawable( R.drawable.rightarrow );
        m_downArrow = res.getDrawable( R.drawable.downarrow );
        m_origin = res.getDrawable( R.drawable.origin );

        // Move this to finalize?
        // XwJNI.game_dispose( jniGamePtr );
        // Utils.logf( "game_dispose returned" );
        // jniGamePtr = 0;
    }

    private boolean layoutBoardOnce() 
    {
        if ( !m_boardSet && null != m_gi ) {
            m_boardSet = true;

            // For now we're assuming vertical orientation.  Fix way
            // later.

            int width = getWidth();
            int height = getHeight();
            int nCells = m_gi.boardSize;
            int cellSize = width / nCells;
            m_left = (width % nCells) / 2;

            // If we're vertical, we can likely fit all the board and
            // have a tall tray too.  If horizontal, let's assume
            // that's so things will be big, and rather than make 'em
            // small assume some scrolling.  So make the tray 1.5 to
            // 2.5x a cell width in height and then scroll however
            // many.

            int trayHt = cellSize * 2;
            int scoreHt = cellSize; // scoreboard ht same as cells for
                                    // proportion
            int wantHt = trayHt + scoreHt + (cellSize * nCells);
            int nToScroll = 0;
            if ( wantHt <= height ) {
                m_top = (height - wantHt) / 2;
            } else {
                // 
                nToScroll = nCells - ((height - (cellSize*3)) / cellSize);
                Utils.logf( "nToScroll: " + nToScroll );
                trayHt = height - (cellSize * (1 + (nCells-nToScroll)));
                m_top = 0;
            }

            XwJNI.board_setScoreboardLoc( m_jniGamePtr, 0, 0, 
                                          nCells * cellSize, // width
                                          scoreHt, true );

            XwJNI.board_setPos( m_jniGamePtr, 0, scoreHt, false );
            XwJNI.board_setScale( m_jniGamePtr, cellSize, cellSize );

            XwJNI.board_setTrayLoc( m_jniGamePtr, 0,
                                    scoreHt + ((nCells-nToScroll) * cellSize),
                                    nCells * cellSize, // width
                                    trayHt,      // height
                                    4 );

            XwJNI.board_setShowColors( m_jniGamePtr, true ); // get from prefs!

            XwJNI.board_invalAll( m_jniGamePtr );

            m_bitmap = Bitmap.createBitmap( 1 + (cellSize*nCells),
                                            1 + trayHt + scoreHt
                                            + (cellSize *(nCells-nToScroll)),
                                            Bitmap.Config.ARGB_8888 );
            m_canvas = new Canvas( m_bitmap );

            // need to synchronize??
            m_jniThread.handle( JNIThread.JNICmd.CMD_DRAW );
        }
        return m_boardSet;
    }

    public void startHandling( JNIThread thread, int gamePtr, CurGameInfo gi ) 
    {
        m_jniThread = thread;
        m_jniGamePtr = gamePtr;
        m_gi = gi;
    }

    // DrawCtxt interface implementation
    public void scoreBegin( Rect rect, int numPlayers, int[] scores, 
                            int remCount, int dfs )
    {
        clearToBack( rect );
    }

    public void measureRemText( Rect r, int nTilesLeft, int[] width, 
                                int[] height ) 
    {
        width[0] = 30;
        height[0] = r.bottom - r.top;
    }

    public void measureScoreText( Rect r, DrawScoreInfo dsi, 
                                  int[] width, int[] height )
    {
        width[0] = 60;
        height[0] = r.bottom - r.top;
    }

    public void drawRemText( Rect rInner, Rect rOuter, int nTilesLeft, 
                             boolean focussed )
    {
        String text = String.format( "%d", nTilesLeft ); // should cache a formatter
        m_fillPaint.setColor( TILE_BACK );
        m_canvas.drawRect( rOuter, m_fillPaint );

        m_fillPaint.setTextSize( rInner.bottom - rInner.top );
        m_fillPaint.setColor( BLACK );
        drawCentered( text, rInner );
    }

    public void score_drawPlayer( Rect rInner, Rect rOuter, DrawScoreInfo dsi )
    {
        String text = String.format( "%s:%d", dsi.name, dsi.totalScore );
        m_fillPaint.setTextSize( rInner.bottom - rInner.top );
        m_fillPaint.setColor( m_playerColors[dsi.playerNum] );
        drawCentered( text, rInner );
    }

    public boolean drawCell( Rect rect, String text, Object[] bitmaps,
                             int tile, int owner, int bonus, int hintAtts, 
                             int flags ) 
    {
        int backColor;
        int foreColor = WHITE;  // must be initialized :-(
        boolean empty = null == text && null == bitmaps;
        boolean pending = 0 != (flags & CELL_HIGHLIGHT);

        clearToBack( rect );

        if ( empty ) {
            backColor = m_bonusColors[bonus];
        } else if ( pending ) {
            backColor = BLACK;
        } else {
            backColor = TILE_BACK; 
            foreColor = m_playerColors[owner];
        }

        m_fillPaint.setColor( backColor );
        m_canvas.drawRect( rect, m_fillPaint );

        if ( empty ) {
            if ( (CELL_ISSTAR & flags) != 0 ) {
                m_origin.setBounds( rect );
                m_origin.draw( m_canvas );
            }
        } else {
            m_fillPaint.setTextSize( rect.bottom - rect.top );
            if ( owner < 0 ) {  // showColors option not turned on
                owner = 0;
            }
            m_fillPaint.setColor( foreColor );
            drawCentered( text, rect );
        }

        m_canvas.drawRect( rect, m_strokePaint );
        return true;
    }

    public void drawBoardArrow ( Rect rect, int bonus, boolean vert, 
                                 int hintAtts, int flags )
    {
        rect.inset( 2, 2 );
        Drawable arrow = vert? m_downArrow : m_rightArrow;
        arrow.setBounds( rect );
        arrow.draw( m_canvas );
    }

    public boolean vertScrollBoard( Rect /*out*/ rect, int dist, int dfs )
    {
        Utils.logf( "vertScrollBoard" );
        return false;
    }

    public boolean trayBegin ( Rect rect, int owner, int dfs ) 
    {
        m_trayOwner = owner;
        return true;
    }

    public void drawTile( Rect rect, String text, Object[] bitmaps, int val, 
                          int flags ) {
        boolean valHidden = (flags & CELL_VALHIDDEN) != 0;
        boolean notEmpty = (flags & CELL_ISEMPTY) == 0;
        boolean isCursor = (flags & CELL_ISCURSOR) != 0;

        clearToBack( rect );

        if ( isCursor || notEmpty ) {
            m_fillPaint.setColor( TILE_BACK );
            m_canvas.drawRect( rect, m_fillPaint );

            if ( null != text ) {
                m_fillPaint.setColor( m_playerColors[m_trayOwner] );
                positionDrawTile( rect, text, val );
            }

            m_canvas.drawRect( rect, m_strokePaint ); // frame
        }
    }

    public void drawTileMidDrag ( Rect rect, String text, Object[] bitmaps,
                                  int val, int owner, int flags ) 
    {
        drawTile( rect, text, bitmaps, val, flags );
    }

    public void drawTileBack( Rect rect, int flags ) 
    {
        drawTile( rect, "?", null, 0, flags );
    }

    public void drawTrayDivider( Rect rect, int flags ) 
    {
        clearToBack( rect );
        m_fillPaint.setColor( BLACK ); // black for now
        m_canvas.drawRect( rect, m_fillPaint );
    }

    public void score_pendingScore( Rect rect, int score, int playerNum, 
                                    int flags ) 
    {
        String text = score >= 0? String.format( "%d", score ) : "??";
        clearToBack( rect );
        m_fillPaint.setColor( BLACK );
        m_fillPaint.setTextSize( (rect.bottom - rect.top) / 2 );
        drawCentered( text, rect );
    }


    private void drawCentered( String text, Rect rect ) 
    {
        int bottom = rect.bottom;
        int center = rect.left + ( (rect.right - rect.left) / 2 );
        m_canvas.drawText( text, center, bottom, m_fillPaint );
    }

    private void positionDrawTile( Rect rect, String text, int val )
    {
        if ( null == m_letterRect ) {
            // assumes show values is on
            m_letterRect = new Rect( 0, 0, rect.width() * 3 / 4, 
                                     rect.height() * 3 / 4 );
        }
        m_letterRect.offsetTo( rect.left, rect.top );
        m_fillPaint.setTextSize( m_letterRect.height() );
        drawCentered( text, m_letterRect );

        if ( null == m_valRect ) {
            m_valRect = new Rect( 0, 0, rect.width() / 4, rect.height() / 4 );
        }
        m_valRect.offsetTo( rect.right - (rect.width() / 4),
                            rect.bottom - (rect.height() / 4) );
        text = String.format( "%d", val );
        m_fillPaint.setTextSize( m_valRect.height() );
        drawCentered( text, m_valRect );
    }

    private void clearToBack( Rect rect ) 
    {
        m_fillPaint.setColor( WHITE );
        m_canvas.drawRect( rect, m_fillPaint );
    }
    
}
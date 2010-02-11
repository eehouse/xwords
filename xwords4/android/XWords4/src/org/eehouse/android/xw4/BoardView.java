/* -*- compile-command: "cd ../../../../../; ant install"; -*- */

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
import android.graphics.drawable.BitmapDrawable;
import android.content.res.Resources;

public class BoardView extends View implements DrawCtx, 
                                               BoardHandler {

    private Paint m_fillPaint;
    private Paint m_strokePaint;
    private Paint m_tileStrokePaint;
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
    private String[] m_scores;
    private Rect m_boundsScratch;
    private String m_remText;
    private int m_dictPtr = 0;

    private static final int BLACK = 0xFF000000;
    private static final int WHITE = 0xFFFFFFFF;
    private static final int TILE_BACK = 0xFFFFFF99;
    private int [] m_bonusColors;
    private  int[] m_playerColors;

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
        m_tileStrokePaint = new Paint();
        m_tileStrokePaint.setStyle( Paint.Style.STROKE );
        Utils.logf( "stroke starts at " + m_tileStrokePaint.getStrokeWidth() );
        float curWidth = m_tileStrokePaint.getStrokeWidth();
        curWidth *= 2;
        if ( curWidth < 2 ) {
            curWidth = 2;
        }
        m_tileStrokePaint.setStrokeWidth( curWidth );

        Resources res = getResources();
        m_rightArrow = res.getDrawable( R.drawable.rightarrow );
        m_downArrow = res.getDrawable( R.drawable.downarrow );
        m_origin = res.getDrawable( R.drawable.origin );

        m_boundsScratch = new Rect();

        CommonPrefs prefs = CommonPrefs.get();
        m_playerColors = prefs.playerColors;
        m_bonusColors = prefs.bonusColors;
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

            int trayHt = cellSize * 3;
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

            m_bitmap = Bitmap.createBitmap( 1 + (cellSize*nCells),
                                            1 + trayHt + scoreHt
                                            + (cellSize *(nCells-nToScroll)),
                                            Bitmap.Config.ARGB_8888 );
            m_canvas = new Canvas( m_bitmap );

            // need to synchronize??
            m_jniThread.handle( JNIThread.JNICmd.CMD_LAYOUT, getWidth(),
                                getHeight(), m_gi.boardSize );
            m_jniThread.handle( JNIThread.JNICmd.CMD_DRAW );
        }
        return m_boardSet;
    }

    public void changeLayout()
    {
        m_boardSet = false;
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
        m_canvas.save( Canvas.CLIP_SAVE_FLAG );
        m_canvas.clipRect(rect);
        m_scores = new String[numPlayers];
    }

    public void measureRemText( Rect r, int nTilesLeft, int[] width, 
                                int[] height ) 
    {
        m_remText = String.format( "%d", nTilesLeft ); // should cache a formatter
        m_fillPaint.setTextSize( r.bottom - r.top );
        m_fillPaint.getTextBounds( m_remText, 0, m_remText.length(), 
                                   m_boundsScratch );

        int minWidth = m_boundsScratch.right;
        if ( minWidth < 20 ) {
            minWidth = 20; // it's a button; make it bigger
        }
        width[0] = minWidth;
        height[0] = m_boundsScratch.bottom;
    }

    public void drawRemText( Rect rInner, Rect rOuter, int nTilesLeft, 
                             boolean focussed )
    {
        m_fillPaint.setColor( TILE_BACK );
        m_canvas.drawRect( rOuter, m_fillPaint );

        m_fillPaint.setTextSize( rOuter.bottom - rOuter.top );
        m_fillPaint.setColor( BLACK );
        drawCentered( m_remText, rOuter );
    }

    public void measureScoreText( Rect r, DrawScoreInfo dsi, 
                                  int[] width, int[] height )
    {
        StringBuffer sb = new StringBuffer();
        if ( dsi.isTurn ) {
            sb.append( dsi.name );
            sb.append( ":" );
        }
        sb.append( dsi.totalScore );
        if ( dsi.nTilesLeft >= 0 ) {
            sb.append( ":" );
            sb.append( dsi.nTilesLeft );
        }
        String str = sb.toString();
        m_scores[dsi.playerNum] = str;

        m_fillPaint.setTextSize( r.bottom - r.top );
        m_fillPaint.getTextBounds( str, 0, str.length(), m_boundsScratch );

        width[0] = m_boundsScratch.right;
        height[0] = m_boundsScratch.bottom;
    }

    public void score_drawPlayer( Rect rInner, Rect rOuter, DrawScoreInfo dsi )
    {
        String text = m_scores[dsi.playerNum];
        m_fillPaint.setTextSize( rOuter.bottom - rOuter.top );
        m_fillPaint.setColor( m_playerColors[dsi.playerNum] );
        drawCentered( text, rOuter );
    }

    public boolean drawCell( Rect rect, String text, BitmapDrawable[] bitmaps,
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
            if ( owner < 0 ) {
                owner = 0;
            }
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
            m_fillPaint.setColor( foreColor );
            if ( null == bitmaps ) {
                m_fillPaint.setTextSize( rect.bottom - rect.top );
                drawCentered( text, rect );
            } else {
                bitmaps[0].setBounds( rect );
                bitmaps[0].draw( m_canvas );
            }
        }

        m_canvas.drawRect( rect, m_strokePaint );
        return true;
    } // drawCell

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

    public void drawTile( Rect rect, String text, BitmapDrawable[] bitmaps, int val, 
                          int flags ) 
    {
        drawTileImpl( rect, text, bitmaps, val, flags, true );
    }

    public void drawTileMidDrag( Rect rect, String text, BitmapDrawable[] bitmaps,
                                  int val, int owner, int flags ) 
    {
        drawTileImpl( rect, text, bitmaps, val, flags, false );
    }

    public void drawTileBack( Rect rect, int flags ) 
    {
        drawTileImpl( rect, "?", null, -1, flags, true );
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

    public void objFinished( /*BoardObjectType*/int typ, Rect rect, int dfs )
    {
        if ( DrawCtx.OBJ_SCORE == typ ) {
            m_canvas.restore();
        }
    }

    public void dictChanged( int dictPtr )
    {
        Utils.logf( "BoardView::dictChanged" );
        if ( m_dictPtr != dictPtr ) {
            if ( m_dictPtr == 0 || !XwJNI.dict_tilesAreSame( m_dictPtr, dictPtr ) ) {
                String[] chars = XwJNI.dict_getChars( dictPtr );
                for ( String str : chars ) {
                    if ( str.length() > 0 && str.charAt(0) >= 32 ) {
                        Utils.logf( "got " + str );
                    }
                }
            }
            m_dictPtr = dictPtr;
        }
    }

    private void drawTileImpl( Rect rect, String text, 
                                  BitmapDrawable[] bitmaps, int val, 
                                  int flags, boolean clearBack )
    {
        boolean valHidden = (flags & CELL_VALHIDDEN) != 0;
        boolean notEmpty = (flags & CELL_ISEMPTY) == 0;
        boolean isCursor = (flags & CELL_ISCURSOR) != 0;

        m_canvas.save( Canvas.CLIP_SAVE_FLAG );
        m_canvas.clipRect( rect );
        
        if ( clearBack ) {
            clearToBack( rect );
        }

        if ( isCursor || notEmpty ) {

            if ( clearBack ) {
                m_fillPaint.setColor( TILE_BACK );
                m_canvas.drawRect( rect, m_fillPaint );
            }

            m_fillPaint.setColor( m_playerColors[m_trayOwner] );
            positionDrawTile( rect, text, bitmaps, val );

            m_canvas.drawRect( rect, m_tileStrokePaint); // frame
            if ( 0 != (flags & CELL_HIGHLIGHT) ) {
                rect.inset( 2, 2 );
                m_canvas.drawRect( rect, m_tileStrokePaint ); // frame
            }
        }
        m_canvas.restore();
    }

    private void drawCentered( String text, Rect rect ) 
    {
        int bottom = rect.bottom;
        int center = rect.left + ( (rect.right - rect.left) / 2 );
        m_canvas.drawText( text, center, bottom, m_fillPaint );
    }

    private void positionDrawTile( Rect rect, String text, 
                                   BitmapDrawable bitmaps[], int val )
    {

        if ( null != bitmaps || null != text ) {
            if ( null == m_letterRect ) {
                m_letterRect = new Rect( 0, 0, rect.width() * 3 / 4, 
                                         rect.height() * 3 / 4 );
            }
            m_letterRect.offsetTo( rect.left, rect.top );
            if ( null != bitmaps ) {
                bitmaps[0].setBounds( m_letterRect );
                bitmaps[0].draw( m_canvas );
            } else /*if ( null != text )*/ {
                m_fillPaint.setTextSize( m_letterRect.height() );
                drawCentered( text, m_letterRect );
            }
        }

        if ( val >= 0 ) {
            if ( null == m_valRect ) {
                m_valRect = new Rect( 0, 0, rect.width() / 4, rect.height() / 4 );
            }
            m_valRect.offsetTo( rect.right - (rect.width() / 4),
                                rect.bottom - (rect.height() / 4) );
            text = String.format( "%d", val );
            m_fillPaint.setTextSize( m_valRect.height() );
            drawCentered( text, m_valRect );
        }
    }

    private void clearToBack( Rect rect ) 
    {
        m_fillPaint.setColor( WHITE );
        m_canvas.drawRect( rect, m_fillPaint );
    }
    
}
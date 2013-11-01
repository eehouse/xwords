/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009 - 2013 by Eric House (xwords@eehouse.org).  All
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

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.graphics.RectF;
import android.graphics.Paint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.BitmapDrawable;

import org.eehouse.android.xw4.jni.DrawCtx;
import org.eehouse.android.xw4.jni.DrawScoreInfo;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.XwJNI;

import junit.framework.Assert;

public class BoardCanvas extends Canvas implements DrawCtx {
    private static final int BLACK = 0xFF000000;
    private static final int WHITE = 0xFFFFFFFF;
    private static final int FRAME_GREY = 0xFF101010;
    private static final int SCORE_HT_DROP = 2;
    private static final boolean DEBUG_DRAWFRAMES = false;
    private static final int NOT_TURN_ALPHA = 0x3FFFFFFF;
    private static final int IN_TRADE_ALPHA = 0x3FFFFFFF;
    private static final boolean FRAME_TRAY_RECTS = false; // for debugging

    private Bitmap m_bitmap;
    private JNIThread m_jniThread;
    private Paint m_fillPaint;
    private Paint m_strokePaint;
    private Paint m_drawPaint;
    private Paint m_tileStrokePaint;
    private String[][] m_scores;
    private String m_remText;
    private int m_mediumFontHt;
    private Rect m_boundsScratch = new Rect();
    private Rect m_letterRect;
    private Rect m_valRect;

    private int[] m_bonusColors;
    private int[] m_playerColors;
    private int[] m_otherColors;
    private String[] m_bonusSummaries;
    private int m_defaultFontHt;
    private CommonPrefs m_prefs;
    private int m_lastSecsLeft;
    private int m_lastTimerPlayer;
    private boolean m_inTrade;
    private boolean m_darkOnLight;
    private Drawable m_origin;
    private boolean m_blackArrow;
    private Drawable m_rightArrow;
    private Drawable m_downArrow;
    private Context m_context;
    private int m_trayOwner = -1;
    private int m_pendingScore;
    private int m_dictPtr = 0;
    private String[] m_dictChars;
    private boolean m_hasSmallScreen;
    private int m_backgroundUsed = 0x00000000;

    // FontDims: exists to translate space available to the largest
    // font we can draw within that space taking advantage of our use
    // being limited to a known small subset of glyphs.  We need two
    // numbers from this: the textHeight to pass to Paint.setTextSize,
    // and the descent to use when drawing.  Both can be calculated
    // proportionally.  We know the ht we passed to Paint to get the
    // height we've now measured; that gives a percent to multiply any
    // future wantHt by.  Ditto for the descent
    private class FontDims {
        FontDims( float askedHt, int topRow, int bottomRow, float width ) {
            // DbgUtils.logf( "FontDims(): askedHt=" + askedHt );
            // DbgUtils.logf( "FontDims(): topRow=" + topRow );
            // DbgUtils.logf( "FontDims(): bottomRow=" + bottomRow );
            // DbgUtils.logf( "FontDims(): width=" + width );
            float gotHt = bottomRow - topRow + 1;
            m_htProportion = gotHt / askedHt;
            Assert.assertTrue( (bottomRow+1) >= askedHt );
            float descent = (bottomRow+1) - askedHt;
            // DbgUtils.logf( "descent: " + descent );
            m_descentProportion = descent / askedHt;
            Assert.assertTrue( m_descentProportion >= 0 );
            m_widthProportion = width / askedHt;
            // DbgUtils.logf( "m_htProportion: " + m_htProportion );
            // DbgUtils.logf( "m_descentProportion: " + m_descentProportion );
        }
        private float m_htProportion;
        private float m_descentProportion;
        private float m_widthProportion;
        int heightFor( int ht ) { return (int)(ht / m_htProportion); }
        int descentFor( int ht ) { return (int)(ht * m_descentProportion); }
        int widthFor( int width ) { return (int)(width / m_widthProportion); }
    }
    private FontDims m_fontDims;

    public BoardCanvas( Context context, Bitmap bitmap, JNIThread jniThread )
    {
        super( bitmap );
        m_context = context;
        m_bitmap = bitmap;
        m_jniThread = jniThread;

        m_hasSmallScreen = Utils.hasSmallScreen( context );

        m_drawPaint = new Paint();
        m_fillPaint = new Paint( Paint.ANTI_ALIAS_FLAG );
        m_strokePaint = new Paint();
        m_strokePaint.setStyle( Paint.Style.STROKE );
        m_tileStrokePaint = new Paint();
        m_tileStrokePaint.setStyle( Paint.Style.STROKE );
        float curWidth = m_tileStrokePaint.getStrokeWidth();
        curWidth *= 2;
        if ( curWidth < 2 ) {
            curWidth = 2;
        }
        m_tileStrokePaint.setStrokeWidth( curWidth );

        Resources res = context.getResources();
        m_origin = res.getDrawable( R.drawable.origin );

        m_prefs = CommonPrefs.get( context );
        m_playerColors = m_prefs.playerColors;
        m_bonusColors = m_prefs.bonusColors;
        m_otherColors = m_prefs.otherColors;

        fillRect( new Rect( 0, 0, bitmap.getWidth(), bitmap.getHeight() ),
                  WHITE );

        m_bonusSummaries = new String[5];
        int[] ids = { R.string.bonus_l2x_summary,
                      R.string.bonus_w2x_summary ,
                      R.string.bonus_l3x_summary,
                      R.string.bonus_w3x_summary };
        for ( int ii = 0; ii < ids.length; ++ii ) {
            m_bonusSummaries[ ii+1 ] = res.getString( ids[ii] );
        }
    }

    // DrawCtxt interface implementation
    public boolean scoreBegin( Rect rect, int numPlayers, int[] scores, 
                               int remCount )
    {
        fillRectOther( rect, CommonPrefs.COLOR_BACKGRND );
        m_scores = new String[numPlayers][];
        return true;
    }

    public boolean measureRemText( Rect r, int nTilesLeft, int[] width, 
                                   int[] height ) 
    {
        boolean showREM = 0 <= nTilesLeft;
        if ( showREM ) {
            // should cache a formatter
            m_remText = String.format( "%d", nTilesLeft );
            m_fillPaint.setTextSize( m_mediumFontHt );
            m_fillPaint.getTextBounds( m_remText, 0, m_remText.length(), 
                                       m_boundsScratch );

            int minWidth = m_boundsScratch.width();
            if ( minWidth < 20 ) {
                minWidth = 20; // it's a button; make it bigger
            }
            width[0] = minWidth;
            height[0] = m_boundsScratch.height();
        }
        return showREM;
    }

    public void drawRemText( Rect rInner, Rect rOuter, int nTilesLeft, 
                             boolean focussed )
    {
        int indx = focussed ? CommonPrefs.COLOR_FOCUS
            : CommonPrefs.COLOR_TILE_BACK;
        fillRectOther( rOuter, indx );

        m_fillPaint.setColor( adjustColor(BLACK) );
        drawCentered( m_remText, rInner, null );
    }

    public void measureScoreText( Rect rect, DrawScoreInfo dsi, 
                                  int[] width, int[] height )
    {
        String[] scoreInfo = new String[dsi.isTurn?1:2];
        int indx = 0;
        StringBuffer sb = new StringBuffer();

        // If it's my turn I get one line.  Otherwise squeeze into
        // two.

        if ( dsi.isTurn ) {
            sb.append( dsi.name );
            sb.append( ":" );
        } else {
            scoreInfo[indx++] = dsi.name;
        }
        sb.append( dsi.totalScore );
        if ( dsi.nTilesLeft >= 0 ) {
            sb.append( ":" );
            sb.append( dsi.nTilesLeft );
        }
        scoreInfo[indx] = sb.toString();
        m_scores[dsi.playerNum] = scoreInfo;

        int rectHt = rect.height();
        if ( !dsi.isTurn ) {
            rectHt /= 2;
        }
        int textHeight = rectHt - SCORE_HT_DROP;
        if ( textHeight < m_defaultFontHt ) {
            textHeight = m_defaultFontHt;
        }
        m_fillPaint.setTextSize( textHeight );

        int needWidth = 0;
        for ( int ii = 0; ii < scoreInfo.length; ++ii ) {
            m_fillPaint.getTextBounds( scoreInfo[ii], 0, scoreInfo[ii].length(), 
                                       m_boundsScratch );
            if ( needWidth < m_boundsScratch.width() ) {
                needWidth = m_boundsScratch.width();
            }
        }
        if ( needWidth > rect.width() ) {
            needWidth = rect.width();
        }
        width[0] = needWidth;

        height[0] = rect.height();
    }

    public void score_drawPlayer( Rect rInner, Rect rOuter, 
                                  int gotPct, DrawScoreInfo dsi )
    {
        if ( 0 != (dsi.flags & CELL_ISCURSOR) ) {
            fillRectOther( rOuter, CommonPrefs.COLOR_FOCUS );
        } else if ( DEBUG_DRAWFRAMES && dsi.selected ) {
            fillRectOther( rOuter, CommonPrefs.COLOR_FOCUS );
        }
        String[] texts = m_scores[dsi.playerNum];
        int color = m_playerColors[dsi.playerNum];
        if ( !m_prefs.allowPeek ) {
            color = adjustColor( color );
        }
        m_fillPaint.setColor( color );

        int height = rOuter.height() / texts.length;
        rOuter.bottom = rOuter.top + height;
        for ( String text : texts ) {
            drawCentered( text, rOuter, null );
            rOuter.offset( 0, height );
        }
        if ( DEBUG_DRAWFRAMES ) {
            m_strokePaint.setColor( BLACK );
            drawRect( rInner, m_strokePaint );
        }
    }

    // public boolean drawRemText( int nTilesLeft, boolean focussed, Rect rect )
    // {
    //     boolean willDraw = 0 <= nTilesLeft;
    //     if ( willDraw ) {
    //         String remText = null;
    //         // should cache a formatter
    //         remText = String.format( "%d", nTilesLeft );
    //         m_fillPaint.setTextSize( m_mediumFontHt );
    //         m_fillPaint.getTextBounds( remText, 0, remText.length(), 
    //                                    m_boundsScratch );

    //         int width = m_boundsScratch.width();
    //         if ( width < 20 ) {
    //             width = 20; // it's a button; make it bigger
    //         }
    //         rect.right = rect.left + width;

    //         Rect drawRect = new Rect( rect );
    //         int height = m_boundsScratch.height();
    //         if ( height < drawRect.height() ) {
    //             drawRect.inset( 0, (drawRect.height() - height) / 2 );
    //         }

    //         int indx = focussed ? CommonPrefs.COLOR_FOCUS
    //             : CommonPrefs.COLOR_TILE_BACK;
    //         fillRectOther( rect, indx );

    //         m_fillPaint.setColor( adjustColor(BLACK) );
    //         drawCentered( remText, drawRect, null );
    //     }
    //     return willDraw;
    // }

    // public void score_drawPlayers( Rect scoreRect, DrawScoreInfo[] playerData, 
    //                                Rect[] playerRects )
    // {
    //     Rect tmp = new Rect();
    //     int nPlayers = playerRects.length;
    //     int width = scoreRect.width() / (nPlayers + 1);
    //     int left = scoreRect.left;
    //     int right;
    //     StringBuffer sb = new StringBuffer();
    //     String[] scoreStrings = new String[2];
    //     for ( int ii = 0; ii < nPlayers; ++ii ) {
    //         DrawScoreInfo dsi = playerData[ii];
    //         boolean isTurn = dsi.isTurn;
    //         int indx = 0;
    //         sb.delete( 0, sb.length() );

    //         if ( isTurn ) {
    //             sb.append( dsi.name );
    //             sb.append( ":" );
    //         } else {
    //             scoreStrings[indx++] = dsi.name;
    //         }
    //         sb.append( dsi.totalScore );
    //         if ( dsi.nTilesLeft >= 0 ) {
    //             sb.append( ":" );
    //             sb.append( dsi.nTilesLeft );
    //         }
    //         scoreStrings[indx] = sb.toString();

    //         int color = m_playerColors[dsi.playerNum];
    //         if ( !m_prefs.allowPeek ) {
    //             color = adjustColor( color );
    //         }
    //         m_fillPaint.setColor( color );

    //         right = left + (width * (isTurn? 2 : 1) );
    //         playerRects[ii].set( left, scoreRect.top, right, scoreRect.bottom );
    //         left = right;

    //         tmp.set( playerRects[ii] );
    //         tmp.inset( 2, 2 );
    //         int height = tmp.height() / (isTurn? 1 : 2);
    //         tmp.bottom = tmp.top + height;
    //         for ( String str : scoreStrings ) {
    //             drawCentered( str, tmp, null );
    //             if ( isTurn ) {
    //                 break;
    //             }
    //             tmp.offset( 0, height );
    //         }
    //         if ( DEBUG_DRAWFRAMES ) {
    //             m_canvas.drawRect( playerRects[ii], m_strokePaint );
    //         }
    //     }
    // }

    public void drawTimer( Rect rect, int player, int secondsLeft )
    {
        if ( (m_lastSecsLeft != secondsLeft || m_lastTimerPlayer != player) ) {
            m_lastSecsLeft = secondsLeft;
            m_lastTimerPlayer = player;

            String negSign = secondsLeft < 0? "-":"";
            secondsLeft = Math.abs( secondsLeft );
            String time = String.format( "%s%d:%02d", negSign, secondsLeft/60, 
                                         secondsLeft%60 );

            fillRectOther( rect, CommonPrefs.COLOR_BACKGRND );
            m_fillPaint.setColor( m_playerColors[player] );

            Rect shorter = new Rect( rect );
            shorter.inset( 0, shorter.height() / 5 );
            drawCentered( time, shorter, null );

            m_jniThread.handle( JNIThread.JNICmd.CMD_DRAW );
        }
    }

    public boolean boardBegin( Rect rect, int cellWidth, int cellHeight )
    {
        return true;
    }

    public boolean drawCell( final Rect rect, String text, int tile, int value,
                             int owner, int bonus, int hintAtts, 
                             final int flags ) 
    {
        boolean canDraw = figureFontDims();
        if ( canDraw ) {
            int backColor;
            boolean empty = 0 != (flags & (CELL_DRAGSRC|CELL_ISEMPTY));
            boolean pending = 0 != (flags & CELL_HIGHLIGHT);
            String bonusStr = null;

            if ( m_inTrade ) {
                fillRectOther( rect, CommonPrefs.COLOR_BACKGRND );
            }

            if ( owner < 0 ) {
                owner = 0;
            }
            int foreColor = m_playerColors[owner];

            if ( 0 != (flags & CELL_ISCURSOR) ) {
                backColor = m_otherColors[CommonPrefs.COLOR_FOCUS];
            } else if ( empty ) {
                if ( 0 == bonus ) {
                    backColor = m_otherColors[CommonPrefs.COLOR_NOTILE];
                } else {
                    backColor = m_bonusColors[bonus];
                    bonusStr = m_bonusSummaries[bonus];
                }
            } else if ( pending ) {
                if ( darkOnLight() ) {
                    foreColor = WHITE;
                    backColor = BLACK;
                } else {
                    foreColor = BLACK;
                    backColor = WHITE;
                }
            } else {
                backColor = m_otherColors[CommonPrefs.COLOR_TILE_BACK];
            }

            fillRect( rect, adjustColor( backColor ) );

            if ( empty ) {
                if ( (CELL_ISSTAR & flags) != 0 ) {
                    m_origin.setBounds( rect );
                    m_origin.setAlpha( m_inTrade? IN_TRADE_ALPHA >> 24 : 255 );
                    m_origin.draw( BoardCanvas.this );
                } else if ( null != bonusStr ) {
                    int color = m_otherColors[CommonPrefs.COLOR_BONUSHINT];
                    m_fillPaint.setColor( adjustColor(color) );
                    Rect brect = new Rect( rect );
                    brect.inset( 0, brect.height()/10 );
                    drawCentered( bonusStr, brect, m_fontDims );
                }
            } else {
                m_fillPaint.setColor( adjustColor(foreColor) );
                drawCentered( text, rect, m_fontDims );
            }

            if ( (CELL_ISBLANK & flags) != 0 ) {
                markBlank( rect, backColor );
            }

            // frame the cell
            m_strokePaint.setColor( adjustColor(FRAME_GREY) );
            drawRect( rect, m_strokePaint );

            drawCrosshairs( rect, flags );
        }
        return canDraw;
    } // drawCell

    private boolean m_arrowHintShown = false;
    public void drawBoardArrow( Rect rect, int bonus, boolean vert, 
                                int hintAtts, int flags )
    {
        // figure out if the background is more dark than light
        boolean useDark = darkOnLight();
        if ( m_blackArrow != useDark ) {
            m_blackArrow = useDark;
            m_downArrow = m_rightArrow = null;
        }
        Drawable arrow;
        if ( vert ) {
            if ( null == m_downArrow ) {
                m_downArrow = loadAndRecolor( R.drawable.downarrow, useDark );
            }
            arrow = m_downArrow;
        } else {
            if ( null == m_rightArrow ) {
                m_rightArrow = loadAndRecolor( R.drawable.rightarrow, useDark );
            }
            arrow = m_rightArrow;
        }

        rect.inset( 2, 2 );
        arrow.setBounds( rect );
        arrow.draw( BoardCanvas.this );

        if ( !m_arrowHintShown ) {
            m_arrowHintShown = true;
            // Handler handler = getHandler();
            // if ( null != handler ) {
            //     handler.post( new Runnable() {
            //             public void run() {
            //                 m_parent.
            //                     showNotAgainDlgThen( R.string.not_again_arrow, 
            //                                          R.string.
            //                                          key_notagain_arrow );
            //             } } );
            // }
        }
    }

    public boolean trayBegin( Rect rect, int owner, int score ) 
    {
        m_trayOwner = owner;
        m_pendingScore = score;
        return true;
    }

    public void drawTile( Rect rect, String text, int val, int flags ) 
    {
        drawTileImpl( rect, text, val, flags, true );
    }

    public void drawTileMidDrag( Rect rect, String text, int val, int owner, 
                                 int flags ) 
    {
        drawTileImpl( rect, text, val, flags, false );
    }

    public void drawTileBack( Rect rect, int flags ) 
    {
        drawTileImpl( rect, "?", -1, flags, true );
    }

    public void drawTrayDivider( Rect rect, int flags ) 
    {
        boolean isCursor = 0 != (flags & CELL_ISCURSOR);
        boolean selected = 0 != (flags & CELL_HIGHLIGHT);

        int index = isCursor? CommonPrefs.COLOR_FOCUS : CommonPrefs.COLOR_BACKGRND;
        rect.inset( 0, 1 );
        fillRectOther( rect, index );

        rect.inset( rect.width()/4, 0 );
        if ( selected ) {
            drawRect( rect, m_strokePaint );
        } else {
            fillRect( rect, m_playerColors[m_trayOwner] );
        }
    }

    public void score_pendingScore( Rect rect, int score, int playerNum, 
                                    int curTurn, int flags ) 
    {
        String text = score >= 0? String.format( "%d", score ) : "??";
        int otherIndx = (0 == (flags & CELL_ISCURSOR)) 
            ? CommonPrefs.COLOR_BACKGRND : CommonPrefs.COLOR_FOCUS;
        ++rect.top;
        fillRectOther( rect, otherIndx );

        int playerColor = m_playerColors[playerNum];
        if ( playerNum != curTurn ) {
            playerColor &= NOT_TURN_ALPHA;
        }
        m_fillPaint.setColor( playerColor );

        rect.bottom -= rect.height() / 2;
        drawCentered( text, rect, null );

        rect.offset( 0, rect.height() );
        drawCentered( m_context.getResources().getString( R.string.pts ), 
                      rect, null );
    }

    public void objFinished( /*BoardObjectType*/int typ, Rect rect )
    {
        if ( DrawCtx.OBJ_BOARD == typ ) {
            // On squat screens, where I can't use the full width for
            // the board (without scrolling), the right-most cells
            // don't draw their right borders due to clipping, so draw
            // for them.
            m_strokePaint.setColor( adjustColor(FRAME_GREY) );
            int xx = rect.left + rect.width() - 1;
            drawLine( xx, rect.top, xx, rect.top + rect.height(),
                      m_strokePaint );
        }
    }

    public void dictChanged( int dictPtr )
    {
        if ( m_dictPtr != dictPtr ) {
            if ( 0 == dictPtr ) {
                m_fontDims = null;
                m_dictChars = null;
            } else if ( m_dictPtr == 0 || 
                        !XwJNI.dict_tilesAreSame( m_dictPtr, dictPtr ) ) {
                m_fontDims = null;
                m_dictChars = XwJNI.dict_getChars( dictPtr );
            }
            m_dictPtr = dictPtr;
        }
    }

    private void drawTileImpl( Rect rect, String text, int val, 
                               int flags, boolean clearBack )
    {
        // boolean valHidden = (flags & CELL_VALHIDDEN) != 0;
        boolean notEmpty = (flags & CELL_ISEMPTY) == 0;
        boolean isCursor = (flags & CELL_ISCURSOR) != 0;

        save( Canvas.CLIP_SAVE_FLAG );
        rect.top += 1;
        clipRect( rect );

        if ( clearBack ) {
            fillRectOther( rect, CommonPrefs.COLOR_BACKGRND );
        }

        if ( isCursor || notEmpty ) {
            int color = m_otherColors[isCursor? CommonPrefs.COLOR_FOCUS 
                                      : CommonPrefs.COLOR_TILE_BACK];
            if ( !clearBack ) {
                color &= 0x7FFFFFFF; // translucent if being dragged.
            }
            fillRect( rect, color );

            m_fillPaint.setColor( m_playerColors[m_trayOwner] );

            if ( notEmpty ) {
                positionDrawTile( rect, text, val );

                drawRect( rect, m_tileStrokePaint); // frame
                if ( 0 != (flags & CELL_HIGHLIGHT) ) {
                    rect.inset( 2, 2 );
                    drawRect( rect, m_tileStrokePaint ); // frame
                }
            }
        }
        restoreToCount(1); // in case new canvas....
    } // drawTileImpl

    private void drawCrosshairs( final Rect rect, final int flags )
    {
        int color = m_otherColors[CommonPrefs.COLOR_FOCUS];
        if ( 0 != (flags & CELL_CROSSHOR) ) {
            Rect hairRect = new Rect( rect );
            hairRect.inset( 0, hairRect.height() / 3 );
            fillRect( hairRect, color );
        }
        if ( 0 != (flags & CELL_CROSSVERT) ) {
            Rect hairRect = new Rect( rect );
            hairRect.inset( hairRect.width() / 3, 0 );
            fillRect( hairRect, color );
        }
    }

    private void drawCentered( String text, Rect rect, FontDims fontDims ) 
    {
        drawIn( text, rect, fontDims, Paint.Align.CENTER );
    }

    private void drawIn( String text, Rect rect, FontDims fontDims, 
                         Paint.Align align ) 
    {
        int descent = -1;
        int textSize;
        if ( null == fontDims ) {
            textSize = rect.height() - SCORE_HT_DROP;
        } else {
            int height = rect.height() - 4; // borders and padding, 2 each 
            descent = fontDims.descentFor( height );
            textSize = fontDims.heightFor( height );
            // DbgUtils.logf( "using descent: " + descent + " and textSize: " 
            //             + textSize + " in height " + height );
        }
        m_fillPaint.setTextSize( textSize );
        if ( descent == -1 ) {
            descent = m_fillPaint.getFontMetricsInt().descent;
        }
        descent += 2;

        m_fillPaint.getTextBounds( text, 0, text.length(), m_boundsScratch );
        int extra = rect.width() - m_boundsScratch.width();
        if ( 0 >= extra ) {
            m_fillPaint.setTextAlign( Paint.Align.LEFT );
            drawScaled( text, rect, m_boundsScratch, descent );
        } else {
            int bottom = rect.bottom - descent;
            int origin = rect.left;
            if ( Paint.Align.CENTER == align ) {
                origin += rect.width() / 2;
            } else {
                origin += (extra / 5) - m_boundsScratch.left;
            }
            m_fillPaint.setTextAlign( align );
            drawText( text, origin, bottom, m_fillPaint );
        }
    } // drawCentered

    private void drawScaled( String text, final Rect rect, 
                             Rect textBounds, int descent )
    {
        textBounds.bottom = rect.height();

        Bitmap bitmap = Bitmap.createBitmap( textBounds.width(),
                                             rect.height(), 
                                             Bitmap.Config.ARGB_8888 );

        Canvas canvas = new Canvas( bitmap );
        int bottom = textBounds.bottom - descent;
        canvas.drawText( text, -textBounds.left, bottom, m_fillPaint );

        drawBitmap( bitmap, null, rect, m_drawPaint );
    }

    private void positionDrawTile( final Rect rect, String text, int val )
    {
        if ( figureFontDims() ) {
            final int offset = 2;
            if ( null != text ) {
                if ( null == m_letterRect ) {
                    m_letterRect = new Rect( 0, 0, rect.width() - offset,
                                             rect.height() * 3 / 4 );
                }
                m_letterRect.offsetTo( rect.left + offset, rect.top + offset );
                drawIn( text, m_letterRect, m_fontDims, Paint.Align.LEFT );
                if ( FRAME_TRAY_RECTS ) {
                    drawRect( m_letterRect, m_strokePaint );
                }
            }

            if ( val >= 0 ) {
                int divisor = m_hasSmallScreen ? 3 : 4;
                if ( null == m_valRect ) {
                    m_valRect = new Rect( 0, 0, rect.width() / divisor, 
                                          rect.height() / divisor );
                    m_valRect.inset( offset, offset );
                }
                m_valRect.offsetTo( rect.right - (rect.width() / divisor),
                                    rect.bottom - (rect.height() / divisor) );
                text = String.format( "%d", val );
                m_fillPaint.setTextSize( m_valRect.height() );
                m_fillPaint.setTextAlign( Paint.Align.RIGHT );
                drawText( text, m_valRect.right, m_valRect.bottom, 
                                   m_fillPaint );
                if ( FRAME_TRAY_RECTS ) {
                    drawRect( m_valRect, m_strokePaint );
                }
            }
        }
    }

    private void fillRectOther( Rect rect, int index )
    {
        fillRect( rect, m_otherColors[index] );
    }

    private void fillRect( Rect rect, int color )
    {
        m_fillPaint.setColor( color );
        drawRect( rect, m_fillPaint );
    }

    private boolean figureFontDims()
    {
        if ( null == m_fontDims && null != m_dictChars  ) {

            final int ht = 24;
            final int width = 20;

            Paint paint = new Paint(); // CommonPrefs.getFontFlags()??
            paint.setStyle( Paint.Style.STROKE );
            paint.setTextAlign( Paint.Align.LEFT );
            paint.setTextSize( ht );

            Bitmap bitmap = Bitmap.createBitmap( width, (ht*3)/2, 
                                                 Bitmap.Config.ARGB_8888 );
            Canvas canvas = new Canvas( bitmap );

            // FontMetrics fmi = paint.getFontMetrics();
            // DbgUtils.logf( "ascent: " + fmi.ascent );
            // DbgUtils.logf( "bottom: " + fmi.bottom );
            // DbgUtils.logf( "descent: " + fmi.descent );
            // DbgUtils.logf( "leading: " + fmi.leading );
            // DbgUtils.logf( "top : " + fmi.top );

            // DbgUtils.logf( "using as baseline: " + ht );

            Rect bounds = new Rect();
            int maxWidth = 0;
            for ( String str : m_dictChars ) {
                if ( str.length() == 1 && str.charAt(0) >= 32 ) {
                    canvas.drawText( str, 0, ht, paint );
                    paint.getTextBounds( str, 0, 1, bounds );
                    if ( maxWidth < bounds.right ) {
                        maxWidth = bounds.right;
                    }
                }
            }

            // for ( int row = 0; row < bitmap.getHeight(); ++row ) {
            //     StringBuffer sb = new StringBuffer( bitmap.getWidth() );
            //     for ( int col = 0; col < bitmap.getWidth(); ++col ) {
            //         int pixel = bitmap.getPixel( col, row );
            //         sb.append( pixel==0? "." : "X" );
            //     }
            //     DbgUtils.logf( sb.append(row).toString() );
            // }

            int topRow = 0;
            findTop:
            for ( int row = 0; row < bitmap.getHeight(); ++row ) {
                for ( int col = 0; col < bitmap.getWidth(); ++col ) {
                    if ( 0 != bitmap.getPixel( col, row ) ){
                        topRow = row;
                        break findTop;
                    }
                }
            }

            int bottomRow = 0;
            findBottom:
            for ( int row = bitmap.getHeight() - 1; row > topRow; --row ) {
                for ( int col = 0; col < bitmap.getWidth(); ++col ) {
                    if ( 0 != bitmap.getPixel( col, row ) ){
                        bottomRow = row;
                        break findBottom;
                    }
                }
            }
        
            m_fontDims = new FontDims( ht, topRow, bottomRow, maxWidth );
        }
        return null != m_fontDims;
    } // figureFontDims

    private int adjustColor( int color )
    {
        if ( m_inTrade ) {
            color = color & IN_TRADE_ALPHA;
        }
        return color;
    }

    private boolean darkOnLight()
    {
        int background = m_otherColors[ CommonPrefs.COLOR_NOTILE ];
        if ( background != m_backgroundUsed ) {
            m_backgroundUsed = background;
            m_darkOnLight = isLightColor( background );
        }
        return m_darkOnLight;
    }

    private void markBlank( final Rect rect, int backColor )
    {
        RectF oval = new RectF( rect.left, rect.top, rect.right, rect.bottom );
        int curColor = 0;
        boolean whiteOnBlack = !isLightColor( backColor );
        if ( whiteOnBlack ) {
            curColor = m_strokePaint.getColor();
            m_strokePaint.setColor( WHITE );
        }
        drawArc( oval, 0, 360, false, m_strokePaint );
        if ( whiteOnBlack ) {
            m_strokePaint.setColor( curColor );
        }
    }

    private Drawable loadAndRecolor( int resID, boolean useDark )
    {
         Resources res = m_context.getResources();
         Drawable arrow = res.getDrawable( resID );

         if ( !useDark ) {
             Bitmap src = ((BitmapDrawable)arrow).getBitmap();
             Bitmap bitmap = src.copy( Bitmap.Config.ARGB_8888, true );
             for ( int xx = 0; xx < bitmap.getWidth(); ++xx ) {
                 for( int yy = 0; yy < bitmap.getHeight(); ++yy ) {
                     if ( BLACK == bitmap.getPixel( xx, yy ) ) {
                         bitmap.setPixel( xx, yy, WHITE );
                     }
                 }
             }

             arrow = new BitmapDrawable(bitmap); 
         }
         return arrow;
    }

    private boolean isLightColor( int color )
    {
        int sum = 0;
        for ( int ii = 0; ii < 3; ++ii ) {
            sum += color & 0xFF;
            color >>= 8;
        }
        boolean result = sum > (127*3);
        return result;
    }

}

/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2021 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;

import java.lang.reflect.Method;
import java.util.HashSet;
import java.util.Set;

import org.eehouse.android.xw4.jni.BoardDims;
import org.eehouse.android.xw4.jni.BoardHandler;
import org.eehouse.android.xw4.jni.CommonPrefs.TileValueType;
import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.DrawCtx;
import org.eehouse.android.xw4.jni.DrawCtx.DrawScoreInfo;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.XwJNI.DictWrapper;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

public class BoardCanvas extends Canvas implements DrawCtx {
    private static final String TAG = BoardCanvas.class.getSimpleName();
    private static final int BLACK = 0xFF000000;
    private static final int WHITE = 0xFFFFFFFF;
    private static final int SCORE_HT_DROP = 2;
    private static final boolean DEBUG_DRAWFRAMES = false;
    private static final int NOT_TURN_ALPHA = 0x3FFFFFFF;
    private static final int IN_TRADE_ALPHA = 0x3FFFFFFF;
    private static final boolean FRAME_TRAY_RECTS = false; // for debugging
    private static final float MIN_FONT_DIPS = 14.0f;

    private Activity m_activity;
    private Context m_context;
    private JNIThread m_jniThread;
    private Paint m_fillPaint;
    private Paint m_strokePaint;
    private Paint m_drawPaint;
    private Paint m_tileStrokePaint;
    private String[][] m_scores;
    private String m_remText;
    private int m_mediumFontHt;
    private int m_defaultFontHt;
    private int m_minRemWidth;
    private Rect m_boundsScratch = new Rect();
    private Rect m_letterRect;
    private Rect m_valRect;

    private int[] m_bonusColors;
    private int[] m_playerColors;
    private int[] m_otherColors;
    private String[] m_bonusSummaries;
    private CommonPrefs m_prefs;
    private int m_lastSecsLeft;
    private int m_lastTimerPlayer;
    private boolean m_lastTimerTurnDone;
    private boolean m_inTrade;
    private boolean m_darkOnLight;
    private Drawable m_origin;
    private boolean m_blackArrow;
    private Drawable m_rightArrow;
    private Drawable m_downArrow;
    private int m_trayOwner = -1;
    private int m_pendingScore;
    private DictWrapper m_dict;
    protected String[] m_dictChars;
    private boolean m_hasSmallScreen;
    private int m_backgroundUsed = 0x00000000;
    private int mPendingCount;
    private boolean mSawRecents;
    private BoardHandler.NewRecentsProc mNRP;

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
    protected FontDims m_fontDims;

    public BoardCanvas( Context context, Bitmap bitmap )
    {
        this( context, null, bitmap, null, null, null );
    }

    public BoardCanvas( Activity activity, Bitmap bitmap, JNIThread jniThread,
                        BoardDims dims, BoardHandler.NewRecentsProc nrp )
    {
        this( activity, activity, bitmap, jniThread, dims, nrp );
    }

    private BoardCanvas( Context context, Activity activity, Bitmap bitmap,
                         JNIThread jniThread, BoardDims dims,
                         BoardHandler.NewRecentsProc nrp )
    {
        super( bitmap );
        m_context = context;
        m_activity = activity;
        m_jniThread = jniThread;
        mNRP = nrp;

        m_hasSmallScreen = Utils.hasSmallScreen( m_context );

        Resources res = m_context.getResources();
        float scale = res.getDisplayMetrics().density;
        m_defaultFontHt = (int)(MIN_FONT_DIPS * scale + 0.5f);
        m_mediumFontHt = m_defaultFontHt * 3 / 2;
        if ( null != dims ) {
            m_minRemWidth = dims.cellSize;
        }

        m_drawPaint = new Paint();
        m_fillPaint = new Paint( Paint.ANTI_ALIAS_FLAG );
        m_strokePaint = new Paint();
        m_strokePaint.setStyle( Paint.Style.STROKE );

        m_origin = res.getDrawable( R.drawable.ic_origin );

        m_prefs = CommonPrefs.get( m_context );
        m_playerColors = m_prefs.playerColors;
        m_bonusColors = m_prefs.bonusColors;
        m_otherColors = m_prefs.otherColors;

        int[] ids = { R.string.bonus_l2x_summary,
                      R.string.bonus_w2x_summary,
                      R.string.bonus_l3x_summary,
                      R.string.bonus_w3x_summary,
                      R.string.bonus_l4x_summary,
                      R.string.bonus_w4x_summary,
        };
        m_bonusSummaries = new String[1 + ids.length];
        for ( int ii = 0; ii < ids.length; ++ii ) {
            m_bonusSummaries[ ii+1 ] = res.getString( ids[ii] );
        }
    }

    public void setJNIThread( JNIThread jniThread )
    {
        DbgUtils.assertOnUIThread();
        if ( null == jniThread ) {
            // do nothing
        } else if ( ! jniThread.equals( m_jniThread ) ) {
            Log.w( TAG, "changing threads" );
        }
        m_jniThread = jniThread;
        updateDictChars();
    }

    public int getCurPlayer()
    {
        return m_trayOwner;
    }

    public int curPending()
    {
        return m_pendingScore;
    }

    public void setInTrade( boolean inTrade )
    {
        if ( m_inTrade != inTrade ) {
            m_inTrade = inTrade;
            m_jniThread.handle( JNIThread.JNICmd.CMD_INVALALL );
        }
    }

    // DrawCtxt interface implementation
    @Override
    public boolean scoreBegin( Rect rect, int numPlayers, int[] scores,
                               int remCount )
    {
        fillRectOther( rect, CommonPrefs.COLOR_BACKGRND );
        m_scores = new String[numPlayers][];
        return true;
    }

    @Override
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
            if ( minWidth < m_minRemWidth ) {
                minWidth = m_minRemWidth; // it's a button; make it bigger
            }
            width[0] = minWidth;
            height[0] = m_boundsScratch.height();
        }
        return showREM;
    }

    @Override
    public void drawRemText( Rect rInner, Rect rOuter, int nTilesLeft,
                             boolean focussed )
    {
        int indx = focussed ? CommonPrefs.COLOR_FOCUS
            : CommonPrefs.COLOR_TILE_BACK;
        fillRectOther( rOuter, indx );

        m_fillPaint.setColor( adjustColor(BLACK) );
        drawCentered( m_remText, rInner, null );
    }

    @Override
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

    @Override
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

    @Override
    public void drawTimer( Rect rect, final int player,
                           int secondsLeft, final boolean turnDone )
    {
        Activity activity = m_activity;
        if ( null == activity ) {
            // Do nothing
        } else if ( m_lastSecsLeft != secondsLeft
                    || m_lastTimerPlayer != player
                    || m_lastTimerTurnDone != turnDone ) {
            final Rect rectCopy = new Rect(rect);
            final int secondsLeftCopy = secondsLeft;
            activity.runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        if ( null != m_jniThread ) {
                            m_lastSecsLeft = secondsLeftCopy;
                            m_lastTimerPlayer = player;
                            m_lastTimerTurnDone = turnDone;

                            String negSign = secondsLeftCopy < 0? "-":"";
                            int secondsLeft = Math.abs( secondsLeftCopy );
                            String time =
                                String.format( "%s%d:%02d", negSign,
                                               secondsLeft/60, secondsLeft%60 );

                            fillRectOther( rectCopy, CommonPrefs.COLOR_BACKGRND );
                            m_fillPaint.setColor( m_playerColors[player] );

                            rectCopy.inset( 0, rectCopy.height() / 5 );
                            drawCentered( time, rectCopy, null );

                            m_jniThread.handle( JNIThread.JNICmd.CMD_DRAW );
                        }
                    }
                } );
        }
    }

    @Override
    public boolean drawCell( Rect rect, String text, int tile, int tileValue,
                             int owner, int bonus, int flags, TileValueType tvType )
    {
        boolean canDraw = figureFontDims();
        if ( canDraw ) {
            int backColor;
            boolean empty = 0 != (flags & (CELL_DRAGSRC|CELL_ISEMPTY));
            boolean pending = 0 != (flags & CELL_PENDING);
            boolean recent = 0 != (flags & CELL_RECENT);
            mSawRecents = recent || mSawRecents;
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
                ++mPendingCount;
                if ( darkOnLight() ) {
                    foreColor = WHITE;
                    backColor = BLACK;
                } else {
                    foreColor = BLACK;
                    backColor = WHITE;
                }
            } else {
                int indx = recent ? CommonPrefs.COLOR_TILE_BACK_RECENT:CommonPrefs.COLOR_TILE_BACK;
                backColor = m_otherColors[indx];
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
                    float inset = (float)(brect.height() / 3.5);
                    brect.inset( 0, (int)inset );
                    drawCentered( bonusStr, brect, m_fontDims );
                }
            } else {
                String value = String.format( "%d", tileValue );
                switch ( tvType ) {
                case TVT_BOTH:
                    break;
                case TVT_FACES:
                    value = null;
                    break;
                case TVT_VALUES:
                    text = value;
                    value = null;
                    break;
                }

                m_fillPaint.setColor( adjustColor(foreColor) );
                if ( null == value ) {
                    drawCentered( text, rect, m_fontDims );
                } else {
                    Rect smaller = new Rect(rect);
                    smaller.bottom -= smaller.height() / 4;
                    smaller.right -= smaller.width() / 4;
                    drawCentered( text, smaller, m_fontDims );

                    smaller = new Rect(rect);
                    smaller.left += (2 * smaller.width()) / 3;
                    smaller.top += (2 * smaller.height()) / 3;
                    drawCentered( value, smaller, m_fontDims );
                }
            }

            if ( (CELL_ISBLANK & flags) != 0 ) {
                markBlank( rect, backColor );
            }

            // frame the cell
            int frameColor = m_otherColors[CommonPrefs.COLOR_CELLLINE];
            m_strokePaint.setColor( adjustColor(frameColor) );

            // PENDING: fetch/calculate this a lot less frequently!!
            int width = XWPrefs.getPrefsInt( m_activity, R.string.key_board_line_width, 1 );
            m_strokePaint.setStrokeWidth( width );

            drawRect( rect, m_strokePaint );

            drawCrosshairs( rect, flags );
        }
        return canDraw;
    } // drawCell

    @Override
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
                m_downArrow = loadAndRecolor( R.drawable.ic_downarrow, useDark );
            }
            arrow = m_downArrow;
        } else {
            if ( null == m_rightArrow ) {
                m_rightArrow = loadAndRecolor( R.drawable.ic_rightarrow, useDark );
            }
            arrow = m_rightArrow;
        }

        rect.inset( 2, 2 );
        arrow.setBounds( rect );
        arrow.draw( BoardCanvas.this );

        postNAHint( R.string.not_again_arrow, R.string.key_notagain_arrow );
    }

    @Override
    public boolean trayBegin( Rect rect, int owner, int score )
    {
        m_trayOwner = owner;
        m_pendingScore = score;
        if ( null != m_tileStrokePaint ) {
            // force new color just in case it's changed
            m_tileStrokePaint.setColor( m_otherColors[CommonPrefs.COLOR_CELLLINE] );
        }
        return true;
    }

    @Override
    public boolean drawTile( Rect rect, String text, int val, int flags )
    {
        return drawTileImpl( rect, text, val, flags, true );
    }

    @Override
    public boolean drawTileMidDrag( Rect rect, String text, int val, int owner,
                                 int flags )
    {
        return drawTileImpl( rect, text, val, flags, false );
    }

    @Override
    public boolean drawTileBack( Rect rect, int flags )
    {
        return drawTileImpl( rect, "?", -1, flags, true );
    }

    @Override
    public void drawTrayDivider( Rect rect, int flags )
    {
        boolean isCursor = 0 != (flags & CELL_ISCURSOR);
        boolean selected = 0 != (flags & (CELL_PENDING|CELL_RECENT));

        int index = isCursor? CommonPrefs.COLOR_FOCUS : CommonPrefs.COLOR_BACKGRND;
        fillRectOther( rect, index );

        rect.inset( rect.width()/4, 1 );
        if ( selected ) {
            drawRect( rect, m_strokePaint );
        } else {
            fillRect( rect, m_playerColors[m_trayOwner] );
        }
    }

    @Override
    public void score_pendingScore( Rect rect, int score, int playerNum,
                                    boolean curTurn, int flags )
    {
        // Log.d( TAG, "score_pendingScore(playerNum=%d, curTurn=%b)",
        //        playerNum, curTurn );

        int otherIndx = (0 == (flags & CELL_ISCURSOR))
            ? CommonPrefs.COLOR_BACKGRND : CommonPrefs.COLOR_FOCUS;
        ++rect.top;
        fillRectOther( rect, otherIndx );

        int playerColor = m_playerColors[playerNum];
        if ( !curTurn ) {
            playerColor &= NOT_TURN_ALPHA;
        }
        m_fillPaint.setColor( playerColor );

        String text = score >= 0? String.format( "%d", score ) : "??";
        rect.bottom -= rect.height() / 2;
        drawCentered( text, rect, null );

        rect.offset( 0, rect.height() );
        drawCentered( LocUtils.getString( m_context, R.string.pts ),
                      rect, null );
    }

    @Override
    public void objFinished( /*BoardObjectType*/int typ, Rect rect )
    {
        if ( DrawCtx.OBJ_BOARD == typ ) {
            // On squat screens, where I can't use the full width for
            // the board (without scrolling), the right-most cells
            // don't draw their right borders due to clipping, so draw
            // for them.
            int frameColor = m_otherColors[CommonPrefs.COLOR_CELLLINE];
            m_strokePaint.setColor( adjustColor(frameColor) );
            int xx = rect.left + rect.width() - 1;
            drawLine( xx, rect.top, xx, rect.top + rect.height(),
                      m_strokePaint );

            // Remove this for now. It comes up at the wrong time for new
            // installs. Need to delay it. PENDING
            if ( false && mPendingCount > 0 ) {
                mPendingCount = 0;
                postNAHint( R.string.not_again_longtap_lookup,
                            R.string.key_na_longtap_lookup );
            }

            if ( mSawRecents && null != mNRP ) {
                mNRP.sawNew();
            }
            mSawRecents = false;
        }
    }

    @Override
    public void dictChanged( final long newPtr )
    {
        long curPtr = null == m_dict ? 0 : m_dict.getDictPtr();
        boolean doPost = false;
        if ( curPtr != newPtr ) {
            if ( 0 == newPtr ) {
                m_fontDims = null;
                m_dictChars = null;
            } else if ( 0 == curPtr
                        || !XwJNI.dict_tilesAreSame( curPtr, newPtr ) ) {
                m_fontDims = null;
                m_dictChars = null;
                doPost = true;
            }
            if ( null != m_dict ) {
                m_dict.release();
            }
            m_dict = new DictWrapper( newPtr );
        }

        // If we're on the UI thread this is run inline, so make sure it's
        // after m_dict is set above.
        if ( doPost ) {
            m_activity.runOnUiThread( new Runnable() {
                    @Override
                    public void run() {
                        updateDictChars();
                    }
                });
        }
    }

    private void updateDictChars()
    {
        if ( null == m_jniThread ) {
            // Log.d( TAG, "updateDictChars(): m_jniThread still null!!" );
        } else if ( null == m_dict ) {
            // Log.d( TAG, "updateDictChars(): m_dict still null!!" );
        } else {
            m_dictChars = XwJNI.dict_getChars( m_dict.getDictPtr() );
            // draw again
            m_jniThread.handle( JNIThread.JNICmd.CMD_INVALALL );
        }
    }

    private static Method sSaveMethod;
    private void saveImpl( Rect rect )
    {
        if ( Build.VERSION.SDK_INT >= 21 ) {
            saveLayer( new RectF(rect), null );
        } else {
            if ( null == sSaveMethod ) {
                try {
                    Class<?> cls = Class.forName("android.graphics.Canvas");
                    sSaveMethod = cls.getDeclaredMethod( "save", new Class[] {int.class} );
                } catch ( NoSuchMethodException | ClassNotFoundException ex ) {
                    Log.e( TAG, "%s", ex );
                    Assert.failDbg();
                }
            }

            final int CLIP_SAVE_FLAG = 0x02;
            try {
                sSaveMethod.invoke( this, CLIP_SAVE_FLAG );
                // Log.d( TAG, "saveImpl() worked" );
            } catch ( java.lang.reflect.InvocationTargetException
                      | IllegalAccessException ex ) {
                Log.e( TAG, "%s", ex );
                Assert.failDbg();
            }
        }
    }

    private boolean drawTileImpl( Rect rect, String text, int val,
                                  int flags, boolean clearBack )
    {
        boolean canDraw = figureFontDims();
        if ( canDraw ) {
            boolean notEmpty = (flags & CELL_ISEMPTY) == 0;
            boolean isCursor = (flags & CELL_ISCURSOR) != 0;

            saveImpl( rect );
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

                    Paint paint = getTileStrokePaint( rect );
                    drawRect( rect, paint ); // frame
                    if ( 0 != (flags & (CELL_PENDING|CELL_RECENT)) ) {
                        int width = (int)paint.getStrokeWidth();
                        rect.inset( width, width );
                        drawRect( rect, paint ); // frame
                    }
                }
            }
            restoreToCount(1); // in case new canvas....
        }
        return canDraw;
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
    } // drawIn

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
             Bitmap bitmap = Bitmap.createBitmap( arrow.getIntrinsicWidth(),
                                                  arrow.getIntrinsicHeight(),
                                                  Bitmap.Config.ARGB_8888 );
             Canvas canvas = new Canvas( bitmap );
             arrow.setBounds( 0, 0, canvas.getWidth(), canvas.getHeight() );
             arrow.draw( canvas );

             for ( int xx = 0; xx < bitmap.getWidth(); ++xx ) {
                 for ( int yy = 0; yy < bitmap.getHeight(); ++yy ) {
                     if ( BLACK == bitmap.getPixel( xx, yy ) ) {
                         bitmap.setPixel( xx, yy, WHITE );
                     }
                 }
             }
             arrow = new BitmapDrawable( bitmap );
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

    private Paint getTileStrokePaint( final Rect rect )
    {
        if ( null == m_tileStrokePaint ) {
            Paint paint = new Paint();
            paint.setStyle( Paint.Style.STROKE );
            paint.setStrokeWidth( Math.max( 2, rect.width() / 20 ) );
            paint.setColor( m_otherColors[CommonPrefs.COLOR_CELLLINE] );
            m_tileStrokePaint = paint;
        }
        return m_tileStrokePaint;
    }

    private static Set<Integer> sShown = new HashSet<>();
    private void postNAHint( final int msgID, final int keyID )
    {
        if ( !sShown.contains( keyID ) ) {
            sShown.add( keyID );

            if ( m_activity instanceof XWActivity ) {
                final XWActivity activity = (XWActivity)m_activity;

                activity.runOnUiThread( new Runnable() {
                        @Override
                        public void run() {
                            activity.makeNotAgainBuilder( keyID, msgID )
                                .show();
                        }
                    } );
            }
        }
    }
}

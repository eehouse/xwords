/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
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
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.AsyncTask;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.loc.LocUtils;

import java.text.DateFormat;
import java.util.Date;
import java.util.HashSet;
import java.util.concurrent.LinkedBlockingQueue;

public class GameListItem extends LinearLayout
    implements View.OnClickListener, SelectableItem.LongClickHandler {

    private static final int SUMMARY_WAIT_MSECS = 1000;

    private static HashSet<Long> s_invalRows = new HashSet<Long>();

    private Activity m_activity;
    private Context m_context;
    private boolean m_loaded;
    private long m_rowid;
    private View m_hideable;
    private ImageView m_thumb;
    private ExpiringTextView m_name;
    private TextView m_viewUnloaded;
    private View m_viewLoaded;
    private LinearLayout m_list;
    private TextView m_state;
    private TextView m_modTime;
    private ImageView m_marker;
    private TextView m_role;

    private boolean m_expanded, m_haveTurn, m_haveTurnLocal;
    private long m_lastMoveTime;
    private ImageButton m_expandButton;
    private Handler m_handler;
    private GameSummary m_summary;
    private SelectableItem m_cb;
    private int m_fieldID;
    private int m_loadingCount;
    private boolean m_selected = false;
    private DrawSelDelegate m_dsdel;

    public GameListItem( Context cx, AttributeSet as )
    {
        super( cx, as );
        m_context = cx;
        if ( cx instanceof Activity ) {
            m_activity = (Activity)cx;
        }
        m_loaded = false;
        m_rowid = DBUtils.ROWID_NOTFOUND;
        m_lastMoveTime = 0;
        m_loadingCount = 0;
        m_dsdel = new DrawSelDelegate( this );

        setOnClickListener( new View.OnClickListener() {
                @Override
                public void onClick( View v ) {
                    // if selected, just un-select
                    if ( null != m_summary ) {
                        m_cb.itemClicked( GameListItem.this, m_summary );
                    }
                }
            } );
    }

    public GameSummary getSummary()
    {
        Assert.assertNotNull( m_summary );
        return m_summary;
    }

    private void init( Handler handler, long rowid, int fieldID,
                       SelectableItem cb )
    {
        m_handler = handler;
        m_rowid = rowid;
        m_fieldID = fieldID;
        m_cb = cb;

        forceReload();
    }

    public void forceReload()
    {
        // DbgUtils.logf( "GameListItem.forceReload: rowid=%d", m_rowid );
        m_summary = null;
        setLoaded( false );
        // Apparently it's impossible to reliably cancel an existing
        // AsyncTask, so let it complete, but drop the results as soon
        // as we're back on the UI thread.
        ++m_loadingCount;

        new LoadItemTask().execute();
    }

    public void invalName()
    {
        setName();
    }

    public void setSelected( boolean selected )
    {
        // If new value and state not in sync, force change in state
        if ( selected != m_selected ) {
            toggleSelected();
        }
    }

    @Override
    protected void onDraw( Canvas canvas )
    {
        super.onDraw( canvas );
        if ( DBUtils.ROWID_NOTFOUND != m_rowid ) {
            synchronized( s_invalRows ) {
                if ( s_invalRows.contains( m_rowid ) ) {
                    forceReload();
                }
            }
        }
    }

    private void update( boolean expanded, long lastMoveTime, boolean haveTurn,
                         boolean haveTurnLocal )
    {
        m_expanded = expanded;
        m_lastMoveTime = lastMoveTime;
        m_haveTurn = haveTurn;
        m_haveTurnLocal = haveTurnLocal;
        showHide();
    }

    public long getRowID()
    {
        return m_rowid;
    }

    // View.OnClickListener interface
    public void onClick( View view ) {
        m_expanded = !m_expanded;
        DBUtils.setExpanded( m_rowid, m_expanded );

        makeThumbnailIf( m_expanded );

        showHide();
    }

    private void findViews()
    {
        m_hideable = (LinearLayout)findViewById( R.id.hideable );
        m_name = (ExpiringTextView)findViewById( R.id.game_name );
        m_expandButton = (ImageButton)findViewById( R.id.expander );
        m_expandButton.setOnClickListener( this );
        m_viewUnloaded = (TextView)findViewById( R.id.view_unloaded );
        m_viewLoaded = findViewById( R.id.view_loaded );
        m_list = (LinearLayout)findViewById( R.id.player_list );
        m_state = (TextView)findViewById( R.id.state );
        m_modTime = (TextView)findViewById( R.id.modtime );
        m_marker = (ImageView)findViewById( R.id.msg_marker );
        m_thumb = (ImageView)findViewById( R.id.thumbnail );
        m_role = (TextView)findViewById( R.id.role );
    }

    private void setLoaded( boolean loaded )
    {
        if ( loaded != m_loaded ) {
            m_loaded = loaded;

            if ( loaded ) {
                // This should be enough to invalidate
                m_viewUnloaded.setVisibility( View.INVISIBLE );
                m_viewLoaded.setVisibility( View.VISIBLE );
            } else {
                m_viewLoaded.invalidate();
            }
        }
    }

    private void showHide()
    {
        m_expandButton.setImageResource( m_expanded ?
                                         R.drawable.expander_ic_maximized :
                                         R.drawable.expander_ic_minimized);
        m_hideable.setVisibility( m_expanded? View.VISIBLE : View.GONE );

        int vis = m_expanded && XWPrefs.getThumbEnabled( m_context )
            ? View.VISIBLE : View.GONE;
        m_thumb.setVisibility( vis );

        m_name.setBackgroundColor( android.R.color.transparent );
        m_name.setPct( m_handler, m_haveTurn && !m_expanded,
                       m_haveTurnLocal, m_lastMoveTime );
    }

    private String setName()
    {
        String state = null;    // hack to avoid calling summarizeState twice
        if ( null != m_summary ) {
            state = m_summary.summarizeState();
            String value = null;
            switch ( m_fieldID ) {
            case R.string.game_summary_field_empty:
                break;
            case R.string.game_summary_field_gameid:
                value = String.format( "%X", m_summary.gameID );
                break;
            case R.string.game_summary_field_rowid:
                value = String.format( "%d", m_rowid );
                break;
            case R.string.game_summary_field_npackets:
                value = String.format( "%d", m_summary.nPacketsPending );
                break;
            case R.string.game_summary_field_language:
                value =
                    DictLangCache.getLangName( m_context,
                                               m_summary.dictLang );
                value = LocUtils.xlateLang( m_context, value, true );
                break;
            case R.string.game_summary_field_opponents:
                value = m_summary.playerNames();
                break;
            case R.string.game_summary_field_state:
                value = state;
                break;
            }

            String name = GameUtils.getName( m_context, m_rowid );
            if ( null != value ) {
                value = LocUtils.getString( m_context, R.string.str_game_name_fmt,
                                            name, value );
            } else {
                value = name;
            }

            m_name.setText( value );
        }
        return state;
    }

    private void setData( GameSummary summary, boolean expanded )
    {
        if ( null != summary ) {
            String state = setName();

            m_list.removeAllViews();
            boolean haveATurn = false;
            boolean haveALocalTurn = false;
            boolean[] isLocal = new boolean[1];
            for ( int ii = 0; ii < summary.nPlayers; ++ii ) {
                ExpiringLinearLayout tmp = (ExpiringLinearLayout)
                    LocUtils.inflate( m_context, R.layout.player_list_elem );
                TextView tview = (TextView)tmp.findViewById( R.id.item_name );
                tview.setText( summary.summarizePlayer( ii ) );
                tview = (TextView)tmp.findViewById( R.id.item_score );
                tview.setText( String.format( "  %d", summary.scores[ii] ) );
                boolean thisHasTurn = summary.isNextToPlay( ii, isLocal );
                if ( thisHasTurn ) {
                    haveATurn = true;
                    if ( isLocal[0] ) {
                        haveALocalTurn = true;
                    }
                }
                tmp.setPct( m_handler, thisHasTurn, isLocal[0],
                            summary.lastMoveTime );
                m_list.addView( tmp, ii );
            }

            m_state.setText( state );

            long lastMoveTime = summary.lastMoveTime;
            lastMoveTime *= 1000;
            DateFormat df = DateFormat.getDateTimeInstance( DateFormat.SHORT,
                                                            DateFormat.SHORT );
            m_modTime.setText( df.format( new Date( lastMoveTime ) ) );

            int iconID = summary.isMultiGame() ?
                R.drawable.multigame__gen : R.drawable.sologame__gen;
            m_marker.setImageResource( iconID );
            m_marker.setOnClickListener( new View.OnClickListener() {
                    @Override
                    public void onClick( View view ) {
                        toggleSelected();
                    }
                } );

            String roleSummary = summary.summarizeRole( m_rowid );
            if ( null != roleSummary ) {
                m_role.setText( roleSummary );
            } else {
                m_role.setVisibility( View.GONE );
            }

            update( expanded, summary.lastMoveTime, haveATurn,
                    haveALocalTurn );
        }
    }

    private void toggleSelected()
    {
        m_selected = !m_selected;
        m_dsdel.showSelected( m_selected );
        m_cb.itemToggled( this, m_selected );
    }

    private void makeThumbnailIf( boolean expanded )
    {
        if ( expanded && null != m_activity
             && XWPrefs.getThumbEnabled( m_context ) ) {
            enqueueGetThumbnail( this, m_rowid );
        }
    }

    private class LoadItemTask extends AsyncTask<Void, Void, GameSummary> {
        @Override
        protected GameSummary doInBackground( Void... unused )
        {
            return GameUtils.getSummary( m_context, m_rowid, SUMMARY_WAIT_MSECS );
        } // doInBackground

        @Override
        protected void onPostExecute( GameSummary summary )
        {
            if ( 0 == --m_loadingCount ) {
                m_summary = summary;

                boolean expanded = DBUtils.getExpanded( m_context, m_rowid );
                makeThumbnailIf( expanded );

                setData( summary, expanded );
                setLoaded( null != m_summary );
                if ( null == summary ) {
                    m_viewUnloaded
                        .setText( LocUtils.getString( m_context,
                                                      R.string.summary_busy ) );
                }
                synchronized( s_invalRows ) {
                    s_invalRows.remove( m_rowid );
                }
            }
        }
    } // class LoadItemTask

    public static GameListItem makeForRow( Context context, View convertView,
                                           long rowid, Handler handler,
                                           int fieldID, SelectableItem cb )
    {
        GameListItem result;
        if ( null != convertView && convertView instanceof GameListItem ) {
            result = (GameListItem)convertView;
        } else {
            result = (GameListItem)LocUtils.inflate( context,
                                                     R.layout.game_list_item );
            result.findViews();
        }
        result.init( handler, rowid, fieldID, cb );
        return result;
    }

    public static void inval( long rowid )
    {
        synchronized( s_invalRows ) {
            s_invalRows.add( rowid );
        }
        // DbgUtils.logf( "GameListItem.inval(rowid=%d); inval rows now %s",
        //                rowid, invalRowsToString() );
    }

    // private static String invalRowsToString()
    // {
    //     String[] strs;
    //     synchronized( s_invalRows ) {
    //         strs = new String[s_invalRows.size()];
    //         Iterator<Long> iter = s_invalRows.iterator();
    //         for ( int ii = 0; iter.hasNext(); ++ii ) {
    //             strs[ii] = String.format("%d", iter.next() );
    //         }
    //     }
    //     return TextUtils.join(",", strs );
    // }
    // GameListAdapter.ClickHandler interface

    public void longClicked()
    {
        toggleSelected();
    }

    private static class ThumbQueueElem {
        public ThumbQueueElem( GameListItem item, long rowid ) {
            m_item = item;
            m_rowid = rowid;
        }
        long m_rowid;
        GameListItem m_item;
    }
    private static LinkedBlockingQueue<ThumbQueueElem> s_queue
        = new LinkedBlockingQueue<ThumbQueueElem>();
    private static Thread s_thumbThread;

    private static void enqueueGetThumbnail( GameListItem item, long rowid )
    {
        s_queue.add( new ThumbQueueElem( item, rowid ) );

        synchronized( GameListItem.class ) {
            if ( null == s_thumbThread ) {
                s_thumbThread = makeThumbThread();
                s_thumbThread.start();
            }
        }
    }

    private static Thread makeThumbThread()
    {
        return new Thread( new Runnable() {
                public void run()
                {
                    for ( ; ; ) {
                        ThumbQueueElem elem;
                        try {
                            elem = s_queue.take();
                        } catch ( InterruptedException ie ) {
                            DbgUtils.logw( getClass(), "interrupted; killing "
                                           + "s_thumbThread" );
                            break;
                        }
                        Activity activity = elem.m_item.m_activity;
                        long rowid = elem.m_rowid;
                        Bitmap thumb = DBUtils.getThumbnail( activity, rowid );
                        if ( null == thumb ) {
                            // loadMakeBitmap puts in DB
                            thumb = GameUtils.loadMakeBitmap( activity, rowid );
                        }

                        final GameListItem item = elem.m_item;
                        final Bitmap ft = thumb;
                        activity.runOnUiThread( new Runnable() {
                                public void run() {
                                    ImageView iview = item.m_thumb;
                                    if ( null != iview ) {
                                        iview.setImageBitmap( ft );
                                    }
                                }
                            });
                    }
                }
            });
    }

}

/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2020 by Eric House (xwords@eehouse.org).  All rights
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

import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.loc.LocUtils;

import java.text.DateFormat;
import java.util.Date;
import java.util.HashSet;
import java.util.concurrent.LinkedBlockingQueue;

public class GameListItem extends LinearLayout
    implements View.OnClickListener, SelectableItem.LongClickHandler,
               ExpandImageButton.ExpandChangeListener {
    private static final String TAG = GameListItem.class.getSimpleName();

    private static final int SUMMARY_WAIT_MSECS = 1000;

    private static HashSet<Long> s_invalRows = new HashSet<>();

    private Activity m_activity;
    private Context m_context;
    private boolean m_loaded;
    private long m_rowid;
    private View m_hideable;
    private ImageView mThumbView;
    private Bitmap mThumb;
    private ExpiringTextView m_name;
    private TextView m_viewUnloaded;
    private View m_viewLoaded;
    private LinearLayout m_list;
    private TextView m_state;
    private TextView m_modTime;
    private ImageView m_gameTypeImage;
    private TextView m_role;

    private boolean m_expanded, m_haveTurn, m_haveTurnLocal;
    private long m_lastMoveTime;
    private ExpandImageButton m_expandButton;
    private Handler m_handler;
    private GameSummary m_summary;
    private SelectableItem m_cb;
    private int m_fieldID;
    private int m_loadingCount;
    private boolean m_selected = false;
    private DrawSelDelegate m_dsdel;

    private static DateFormat sDF = DateFormat
        .getDateTimeInstance( DateFormat.SHORT, DateFormat.SHORT );


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
    }

    // Might return null!!
    public GameSummary getSummary()
    {
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
    @Override
    public void onClick( View view )
    {
        int id = view.getId();
        switch ( id ) {
        case R.id.game_view_container:
            toggleSelected();
            break;

        case R.id.right_side:
        case R.id.thumbnail:
            if ( null != m_summary ) {
                m_cb.itemClicked( GameListItem.this, m_summary );
            }
            break;
        default:
            Assert.failDbg();
            break;
        }
    }

    // ExpandImageButton.ExpandChangeListener
    @Override
    public void expandedChanged( boolean nowExpanded )
    {
        m_expanded = nowExpanded;
        DBUtils.setExpanded( m_rowid, m_expanded );

        makeThumbnailIf( m_expanded );

        showHide();
    }

    private void findViews()
    {
        m_hideable = (LinearLayout)findViewById( R.id.hideable );
        m_name = (ExpiringTextView)findViewById( R.id.game_name );
        m_expandButton = (ExpandImageButton)findViewById( R.id.expander );
        m_expandButton.setOnExpandChangedListener( this );
        m_viewUnloaded = (TextView)findViewById( R.id.view_unloaded );
        m_viewLoaded = findViewById( R.id.view_loaded );
        findViewById( R.id.game_view_container ).setOnClickListener(this);
        m_list = (LinearLayout)findViewById( R.id.player_list );
        m_state = (TextView)findViewById( R.id.state );
        m_modTime = (TextView)findViewById( R.id.modtime );
        m_gameTypeImage = (ImageView)findViewById( R.id.game_type_marker );
        mThumbView = (ImageView)findViewById( R.id.thumbnail );
        mThumbView.setOnClickListener( this );
        m_role = (TextView)findViewById( R.id.role );

        findViewById( R.id.right_side ).setOnClickListener( this );
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
        m_expandButton.setExpanded( m_expanded );
        m_hideable.setVisibility( m_expanded? View.VISIBLE : View.GONE );

        boolean showThumb = null != mThumb
            && XWPrefs.getThumbEnabled( m_context )
            && m_expanded;
        if ( showThumb ) {
            mThumbView.setVisibility( View.VISIBLE );
            mThumbView.setImageBitmap( mThumb );
        } else {
            mThumbView.setVisibility( View.GONE );
        }

        m_name.setBackgroundColor( android.R.color.transparent );
        m_name.setPct( m_handler, m_haveTurn && !m_expanded,
                       m_haveTurnLocal, m_lastMoveTime );
    }

    private String setName()
    {
        String state = null;    // hack to avoid calling summarizeState twice
        if ( null != m_summary ) {
            state = m_summary.summarizeState( m_context );
            String value = null;
            switch ( m_fieldID ) {
            case R.string.game_summary_field_empty:
                break;
            case R.string.game_summary_field_gameid:
                value = String.format( "ID:%d", m_summary.gameID );
                break;
            case R.string.game_summary_field_rowid:
                value = String.format( "%d", m_rowid );
                break;
            case R.string.game_summary_field_npackets:
                value = String.format( "%d", m_summary.nPacketsPending );
                break;
            case R.string.game_summary_field_language:
                value =
                    DictLangCache.getLangNameForISOCode( m_context, m_summary.isoCode );
                value = LocUtils.xlateLang( m_context, value, true );
                break;
            case R.string.game_summary_field_opponents:
                value = m_summary.playerNames( m_context );
                break;
            case R.string.game_summary_field_state:
                value = state;
                break;
            case R.string.title_addrs_pref:
                value = m_summary.conTypes.toString( m_context, false );
                break;
            case R.string.game_summary_field_created:
                value = sDF.format( new Date( m_summary.created ) );
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
                tview.setText( summary.summarizePlayer( m_context, m_rowid, ii ) );
                tview = (TextView)tmp.findViewById( R.id.item_score );
                tview.setText( String.format( "%d", summary.scores[ii] ) );
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
            m_modTime.setText( sDF.format( new Date( lastMoveTime ) ) );

            setTypeIcon();

            // Let's use the chat-icon space for an ALERT icon when we're
            // quarantined. Not ready for non-debug use though, as it shows up
            // periodically as a false positive. Chat icon wins if both should
            // be displayed, mostly because of the false positives.
            int resID = 0;
            if ( summary.isMultiGame() ) {
                int flags = DBUtils.getMsgFlags( m_context, m_rowid );
                if ( 0 != (flags & GameSummary.MSG_FLAGS_CHAT) ) {
                    resID = R.drawable.green_chat__gen;
                }
            }
            if ( 0 == resID
                 && BuildConfig.NON_RELEASE
                 && !Quarantine.safeToOpen( m_rowid ) ) {
                resID = android.R.drawable.stat_sys_warning;
            }
            // Setting to 0 clears, which we want
            ImageView iv = (ImageView)findViewById( R.id.has_chat_marker );
            iv.setImageResource( resID );
            if ( BuildConfig.NON_RELEASE ) {
                int quarCount = Quarantine.getCount( m_rowid );
                ((TextView)findViewById(R.id.corrupt_count_marker))
                    .setText( 0 == quarCount ? "" : "" + quarCount );
            }

            if ( XWPrefs.moveCountEnabled( m_context ) ) {
                TextView tv = (TextView)findViewById( R.id.n_pending );
                int nPending = summary.nPacketsPending;
                String str = nPending == 0 ? "" : String.format( "%d", nPending );
                tv.setText( str );
            }

            String roleSummary = summary.summarizeRole( m_context, m_rowid );
            m_role.setVisibility( null == roleSummary ? View.GONE : View.VISIBLE );
            if ( null != roleSummary ) {
                m_role.setText( roleSummary );
            }

            findViewById( R.id.dup_tag )
                .setVisibility( summary.inDuplicateMode() ? View.VISIBLE : View.GONE );

            update( expanded, summary.lastMoveTime, haveATurn,
                    haveALocalTurn );
        }
    }

    private void setTypeIcon()
    {
        if ( null != m_summary ) { // to be safe
            int iconID = m_summary.isMultiGame()
                ? R.drawable.ic_multigame
                : R.drawable.ic_sologame;
            m_gameTypeImage.setImageResource( iconID );
        }
    }

    private void toggleSelected()
    {
        m_selected = !m_selected;
        m_dsdel.showSelected( m_selected );
        m_cb.itemToggled( this, m_selected );

        findViewById(R.id.game_checked)
            .setVisibility(m_selected ? View.VISIBLE: View.GONE );
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

    @Override
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
        = new LinkedBlockingQueue<>();
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
                @Override
                public void run()
                {
                    for ( ; ; ) {
                        final ThumbQueueElem elem;
                        try {
                            elem = s_queue.take();
                        } catch ( InterruptedException ie ) {
                            Log.w( TAG, "interrupted; killing s_thumbThread" );
                            break;
                        }
                        Activity activity = elem.m_item.m_activity;
                        long rowid = elem.m_rowid;
                        Bitmap thumb = DBUtils.getThumbnail( activity, rowid );
                        if ( null == thumb ) {
                            // loadMakeBitmap puts in DB
                            thumb = GameUtils.loadMakeBitmap( activity, rowid );
                        }

                        if ( null != thumb ) {
                            final Bitmap fThumb = thumb;
                            activity.runOnUiThread( new Runnable() {
                                    @Override
                                    public void run() {
                                        GameListItem item = elem.m_item;
                                        item.mThumb = fThumb;
                                        item.showHide();
                                    }
                                });
                        }
                    }
                }
            });
    }

}

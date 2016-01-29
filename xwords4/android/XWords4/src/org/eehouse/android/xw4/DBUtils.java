/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
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

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteConstraintException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteStatement;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Environment;
import android.text.TextUtils;

import java.sql.Timestamp;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.channels.FileChannel;

import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;
import junit.framework.Assert;

import org.eehouse.android.xw4.DictUtils.DictLoc;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans;
import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.loc.LocUtils;

public class DBUtils {
    public static final int ROWID_NOTFOUND = -1;
    public static final int ROWIDS_ALL = -2;
    public static final int GROUPID_UNSPEC = -1;
    public static final String KEY_NEWGAMECOUNT = "DBUtils.newGameCount";

    private static final String DICTS_SEP = ",";

    private static final String ROW_ID = "rowid";
    private static final String ROW_ID_FMT = "rowid=%d";
    private static final String NAME_FMT = "%s='%s'";
    private static final String NAMELOC_FMT = "%s='%s' AND %s=%d";

    private static long s_cachedRowID = ROWID_NOTFOUND;
    private static byte[] s_cachedBytes = null;

    public static enum GameChangeType { GAME_CHANGED, GAME_CREATED, GAME_DELETED };

    public static interface DBChangeListener {
        public void gameSaved( long rowid, GameChangeType change );
    }
    private static HashSet<DBChangeListener> s_listeners = 
        new HashSet<DBChangeListener>();

    private static SQLiteOpenHelper s_dbHelper = null;

    public static class Obit {
        public Obit( String relayID, int seed ) {
            m_relayID = relayID; m_seed = seed;
        }
        String m_relayID;
        int m_seed;
    }

    public static class HistoryPair {
        private HistoryPair( String p_msg, boolean p_sourceLocal )
        {
            msg = p_msg;
            sourceLocal = p_sourceLocal;
        }
        String msg;
        boolean sourceLocal;
    }

    public static class DictBrowseState {
        public int m_minShown;
        public int m_maxShown;
        public int m_pos;
        public int m_top;
        public String m_prefix;
        public int[] m_counts;
    }

    public static GameSummary getSummary( Context context, long rowid, 
                                          long maxMillis )
    {
        GameSummary result = null;
        GameLock lock = new GameLock( rowid, false ).lock( maxMillis );
        if ( null != lock ) {
            result = getSummary( context, lock );
            lock.unlock();
        }
        return result;
    }

    public static GameSummary getSummary( Context context, long rowid )
    {
        return getSummary( context, rowid, 0L );
    }

    public static GameSummary getSummary( Context context, 
                                          GameLock lock )
    {
        initDB( context );
        GameSummary summary = null;
        String[] columns = { ROW_ID,
                             DBHelper.NUM_MOVES, DBHelper.NUM_PLAYERS,
                             DBHelper.MISSINGPLYRS,
                             DBHelper.GAME_OVER, DBHelper.PLAYERS,
                             DBHelper.TURN, DBHelper.GIFLAGS,
                             DBHelper.CONTYPE, DBHelper.SERVERROLE,
                             DBHelper.ROOMNAME, DBHelper.RELAYID, 
                             /*DBHelper.SMSPHONE,*/ DBHelper.SEED, 
                             DBHelper.DICTLANG, DBHelper.GAMEID,
                             DBHelper.SCORES, 
                             DBHelper.LASTPLAY_TIME, DBHelper.REMOTEDEVS,
                             DBHelper.LASTMOVE, DBHelper.NPACKETSPENDING,
                             DBHelper.EXTRAS,
        };
        String selection = String.format( ROW_ID_FMT, lock.getRowid() );

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                summary = new GameSummary( context );
                summary.nMoves = cursor.getInt(cursor.
                                               getColumnIndex(DBHelper.NUM_MOVES));
                summary.nPlayers = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.NUM_PLAYERS));
                summary.missingPlayers = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.MISSINGPLYRS));
                summary.
                    setPlayerSummary( cursor.
                                      getString( cursor.
                                                 getColumnIndex( DBHelper.
                                                                 PLAYERS ) ) );
                summary.turn = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.TURN));
                summary.
                    setGiFlags( cursor.getInt(cursor.
                                              getColumnIndex(DBHelper.GIFLAGS))
                                );
                summary.gameID = 
                    cursor.getInt(cursor.getColumnIndex(DBHelper.GAMEID) ); 

                String players = cursor.
                    getString(cursor.getColumnIndex( DBHelper.PLAYERS ));
                summary.readPlayers( players );

                summary.dictLang = 
                    cursor.getInt(cursor.
                                  getColumnIndex(DBHelper.DICTLANG));
                summary.modtime = 
                    cursor.getLong(cursor.
                                   getColumnIndex(DBHelper.LASTPLAY_TIME));
                int tmp = cursor.getInt(cursor.
                                        getColumnIndex(DBHelper.GAME_OVER));
                summary.gameOver = tmp != 0;
                summary.lastMoveTime = 
                    cursor.getInt(cursor.getColumnIndex(DBHelper.LASTMOVE));
                String str = cursor
                    .getString(cursor.getColumnIndex(DBHelper.EXTRAS));
                summary.setExtras( str );

                String scoresStr = 
                    cursor.getString( cursor.getColumnIndex(DBHelper.SCORES));
                int[] scores = new int[summary.nPlayers];
                if ( null != scoresStr && scoresStr.length() > 0 ) {
                    StringTokenizer st = new StringTokenizer( scoresStr );
                    for ( int ii = 0; ii < scores.length; ++ii ) {
                        Assert.assertTrue( st.hasMoreTokens() );
                        String token = st.nextToken();
                        scores[ii] = Integer.parseInt( token );
                    }
                } else {
                    for ( int ii = 0; ii < scores.length; ++ii ) {
                        scores[ii] = 0;
                    }
                }
                summary.scores = scores;

                int col = cursor.getColumnIndex( DBHelper.CONTYPE );
                if ( 0 <= col ) {
                    tmp = cursor.getInt( col );
                    summary.conTypes = new CommsConnTypeSet( tmp );
                    col = cursor.getColumnIndex( DBHelper.SEED );
                    if ( 0 < col ) {
                        summary.seed = cursor.getInt( col );
                    }
                    col = cursor.getColumnIndex( DBHelper.NPACKETSPENDING );
                    if ( 0 <= col ) {
                        summary.nPacketsPending = cursor.getInt( col );
                    }

                    for ( Iterator<CommsConnType> iter = summary.conTypes.iterator();
                          iter.hasNext(); ) {
                        switch ( iter.next() ) {
                        case COMMS_CONN_RELAY:
                            col = cursor.getColumnIndex( DBHelper.ROOMNAME );
                            if ( col >= 0 ) {
                                summary.roomName = cursor.getString( col );
                            }
                            col = cursor.getColumnIndex( DBHelper.RELAYID );
                            if ( col >= 0 ) {
                                summary.relayID = cursor.getString( col );
                            }
                            break;
                        case COMMS_CONN_BT:
                        case COMMS_CONN_SMS:
                            col = cursor.getColumnIndex( DBHelper.REMOTEDEVS );
                            if ( col >= 0 ) {
                                summary.setRemoteDevs( context, 
                                                       cursor.getString( col ) );
                            }
                            break;
                        }
                    }
                }

                col = cursor.getColumnIndex( DBHelper.SERVERROLE );
                tmp = cursor.getInt( col );
                summary.serverRole = CurGameInfo.DeviceRole.values()[tmp];
            }
            cursor.close();
            db.close();
        }

        if ( null == summary && lock.canWrite() ) {
            summary = GameUtils.summarize( context, lock );
        }
        return summary;
    } // getSummary

    public static void saveSummary( Context context, GameLock lock,
                                    GameSummary summary )
    {
        boolean needsTimer = false;
        Assert.assertTrue( lock.canWrite() );
        long rowid = lock.getRowid();
        String selection = String.format( ROW_ID_FMT, rowid );

        ContentValues values = null;
        if ( null != summary ) {
            values = new ContentValues();
            values.put( DBHelper.NUM_MOVES, summary.nMoves );
            values.put( DBHelper.NUM_PLAYERS, summary.nPlayers );
            values.put( DBHelper.MISSINGPLYRS, summary.missingPlayers );
            values.put( DBHelper.TURN, summary.turn );
            values.put( DBHelper.GIFLAGS, summary.giflags() );
            values.put( DBHelper.PLAYERS, 
                        summary.summarizePlayers() );
            values.put( DBHelper.DICTLANG, summary.dictLang );
            values.put( DBHelper.GAMEID, summary.gameID );
            values.put( DBHelper.GAME_OVER, summary.gameOver? 1 : 0 );
            values.put( DBHelper.LASTMOVE, summary.lastMoveTime );

            // Don't overwrite extras! Sometimes this method is called from
            // JNIThread which has created the summary from common code that
            // doesn't know about Android additions. Leave those unset to
            // avoid overwriting.
            String extras = summary.getExtras();
            if ( null != extras ) {
                values.put( DBHelper.EXTRAS, summary.getExtras() );
            }
            long nextNag = summary.nextTurnIsLocal() ?
                NagTurnReceiver.figureNextNag( context, 
                                               1000*(long)summary.lastMoveTime )
                : 0;
            values.put( DBHelper.NEXTNAG, nextNag );
                
            values.put( DBHelper.DICTLIST, summary.dictNames(DICTS_SEP) );

            if ( null != summary.scores ) {
                StringBuffer sb = new StringBuffer();
                for ( int score : summary.scores ) {
                    sb.append( String.format( "%d ", score ) );
                }
                values.put( DBHelper.SCORES, sb.toString() );
            }

            if ( null != summary.conTypes ) {
                values.put( DBHelper.CONTYPE, summary.conTypes.toInt() );
                values.put( DBHelper.SEED, summary.seed );
                values.put( DBHelper.NPACKETSPENDING, summary.nPacketsPending );
                for ( Iterator<CommsConnType> iter = summary.conTypes.iterator();
                      iter.hasNext(); ) {
                    switch ( iter.next() ) {
                    case COMMS_CONN_RELAY:
                        values.put( DBHelper.ROOMNAME, summary.roomName );
                        String relayID = summary.relayID;
                        values.put( DBHelper.RELAYID, relayID );
                        needsTimer = null != relayID && 0 < relayID.length();
                        break;
                    case COMMS_CONN_BT:
                    case COMMS_CONN_SMS:
                        values.put( DBHelper.REMOTEDEVS, 
                                    summary.summarizeDevs() );
                        break;
                    }
                }
            }

            values.put( DBHelper.SERVERROLE, summary.serverRole.ordinal() );
        }

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            if ( null == summary ) {
                db.delete( DBHelper.TABLE_NAME_SUM, selection, null );
            } else {
                long result = db.update( DBHelper.TABLE_NAME_SUM,
                                         values, selection, null );
                Assert.assertTrue( result >= 0 );
            }
            db.close();
            notifyListeners( rowid, GameChangeType.GAME_CHANGED );
            invalGroupsCache();
        }

        if ( null != summary ) { // nag time may have changed
            NagTurnReceiver.setNagTimer( context );
        }

        if ( needsTimer ) {
            RelayReceiver.setTimer( context );
        }
    } // saveSummary

    public static void addRematchInfo( Context context, long rowid, String btAddr, 
                                       String phone, String relayID )
    {
        if ( XWApp.REMATCH_SUPPORTED ) {
            GameLock lock = new GameLock( rowid, true ).lock();
            GameSummary summary = getSummary( context, lock );
            if ( null != btAddr ) {
                summary.putStringExtra( GameSummary.EXTRA_REMATCH_BTADDR, btAddr );
            }
            if ( null != phone ) {
                summary.putStringExtra( GameSummary.EXTRA_REMATCH_PHONE, phone );
            }
            if ( null != relayID ) {
                summary.putStringExtra( GameSummary.EXTRA_REMATCH_RELAY, relayID );
            }
            saveSummary( context, lock, summary );
            lock.unlock();
        }
    }

    public static int countGamesUsingLang( Context context, int lang )
    {
        int result = 0;
        String selection = String.format( "%s = %d", DBHelper.DICTLANG,
                                          lang );
        // null for columns will return whole rows: bad
        String[] columns = { DBHelper.DICTLANG };
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );

            result = cursor.getCount();
            cursor.close();
            db.close();
        }
        return result;
    }

    public static int countGamesUsingDict( Context context, String dict )
    {
        int result = 0;
        String pattern = String.format( "%%%s%s%s%%", 
                                        DICTS_SEP, dict, DICTS_SEP );
        String selection = String.format( "%s LIKE '%s'", 
                                          DBHelper.DICTLIST, pattern );
        // null for columns will return whole rows: bad.  But
        // might as well make it an int for speed
        String[] columns = { DBHelper.DICTLANG }; 
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            result = cursor.getCount();
            cursor.close();
            db.close();
        }
        return result;
    }

    public static class SentInvitesInfo {
        public long m_rowid;
        private ArrayList<InviteMeans> m_means;
        private ArrayList<String> m_targets;
        private ArrayList<Timestamp> m_timestamps;
        private int m_cachedCount = 0;

        private SentInvitesInfo( long rowID ) {
            m_rowid = rowID;
            m_means = new ArrayList<InviteMeans>();
            m_targets = new ArrayList<String>();
            m_timestamps = new ArrayList<Timestamp>();
        }

        private void addEntry( InviteMeans means, String target, Timestamp ts )
        {
            m_means.add( means );
            m_targets.add( target );
            m_timestamps.add( ts );
            m_cachedCount = -1;
        }

        public InviteMeans getLastMeans()
        {
            return 0 < m_means.size() ? m_means.get(0) : null;
        }

        public String getLastDev( InviteMeans means )
        {
            String result = null;
            for ( int ii = 0; null == result && ii < m_means.size(); ++ii ) {
                if ( means == m_means.get( ii ) ) {
                    result = m_targets.get( ii );
                }
            }
            return result;
        }

        // There will be lots of duplicates, but we can't detect them all. BUT
        // if means and target are the same it's definitely a dup. So count
        // them all and return the largest number we have. 99% of the time we
        // care only that it's non-0.
        public int getMinPlayerCount() {
            if ( -1 == m_cachedCount ) {
                int count = m_timestamps.size();
                Map<InviteMeans, Set<String>> hashes
                    = new HashMap<InviteMeans, Set<String>>();
                int fakeCount = 0; // make all null-targets count for one
                for ( int ii = 0; ii < count; ++ii ) {
                    InviteMeans means = m_means.get(ii);
                    Set<String> devs;
                    if ( ! hashes.containsKey( means ) ) {
                        devs = new HashSet<String>();
                        hashes.put( means, devs );
                    }
                    devs = hashes.get( means );
                    String target = m_targets.get( ii );
                    if ( null == target ) {
                        target = String.format( "%d", ++fakeCount );
                    }
                    devs.add( target );
                }

                // Now find the max
                m_cachedCount = 0;
                for ( InviteMeans means : InviteMeans.values() ) {
                    if ( hashes.containsKey( means ) ) {
                        int siz = hashes.get( means ).size();
                        m_cachedCount += siz;
                    }
                }
            }
            return m_cachedCount;
        }

        public String getAsText( Context context )
        {
            String result;
            int count = m_timestamps.size();
            if ( 0 == count ) {
                result = LocUtils.getString( context, R.string.no_invites );
            } else {
                String[] strs = new String[count];
                for ( int ii = 0; ii < count; ++ii ) {
                    InviteMeans means = m_means.get(ii);
                    String target = m_targets.get(ii);
                    String timestamp = m_timestamps.get(ii).toString();
                    String msg;

                    switch ( means ) {
                    case SMS:
                        msg = LocUtils.getString( context, R.string.invit_expl_sms_fmt,
                                                  target, timestamp );
                        break;
                    case BLUETOOTH:
                        String devName = BTService.nameForAddr( target );
                        msg = LocUtils.getString( context, R.string.invit_expl_bt_fmt,
                                                  devName, timestamp );
                        break;
                    case RELAY:
                        msg = LocUtils.getString( context, R.string.invit_expl_relay_fmt,
                                                  timestamp );
                        break;
                    default:
                        msg = LocUtils.getString( context, R.string.invit_expl_notarget_fmt,
                                                  means.toString(), timestamp );

                    }
                    strs[ii] = msg;
                }
                result = TextUtils.join( "\n\n", strs );
            }
            return result;
        }
        
    }

    public static SentInvitesInfo getInvitesFor( Context context, long rowid )
    {
        SentInvitesInfo result = new SentInvitesInfo( rowid );

        String[] columns = { DBHelper.MEANS, DBHelper.TIMESTAMP, DBHelper.TARGET }; 
        String selection = String.format( "%s = %d", DBHelper.ROW, rowid );
        String orderBy = DBHelper.TIMESTAMP + " DESC";
        
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_INVITES, columns, 
                                      selection, null, null, null, orderBy );
            if ( 0 < cursor.getCount() ) {
                int indxMns = cursor.getColumnIndex( DBHelper.MEANS );
                int indxTS = cursor.getColumnIndex( DBHelper.TIMESTAMP );
                int indxTrgt = cursor.getColumnIndex( DBHelper.TARGET );
                
                while ( cursor.moveToNext() ) {
                    InviteMeans means = InviteMeans.values()[cursor.getInt( indxMns )];
                    Timestamp ts = Timestamp.valueOf(cursor.getString(indxTS));
                    String target = cursor.getString( indxTrgt );
                    result.addEntry( means, target, ts );
                }
            }
            cursor.close();
            db.close();
        }
        
        return result;
    }

    public static void recordInviteSent( Context context, long rowid,
                                         InviteMeans means, String target )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.ROW, rowid );
        values.put( DBHelper.MEANS, means.ordinal() );
        if ( null != target ) {
            values.put( DBHelper.TARGET, target );
        }

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.insert( DBHelper.TABLE_NAME_INVITES, null, values );
            db.close();
        }

    }

    private static void setInt( long rowid, String column, int value )
    {
        ContentValues values = new ContentValues();
        values.put( column, value );
        updateRow( null, DBHelper.TABLE_NAME_SUM, rowid, values );
    }

    public static void setMsgFlags( long rowid, int flags )
    {
        setInt( rowid, DBHelper.HASMSGS, flags );
        notifyListeners( rowid, GameChangeType.GAME_CHANGED );
    }

    public static void setExpanded( long rowid, boolean expanded )
    {
        setInt( rowid, DBHelper.CONTRACTED, expanded?0:1 );
    }

    private static int getInt( Context context, long rowid, String column,
                               int dflt )
    {
        int result = dflt;
        String selection = String.format( ROW_ID_FMT, rowid );
        String[] columns = { column };
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result =
                    cursor.getInt( cursor.getColumnIndex(column));
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static int getMsgFlags( Context context, long rowid )
    {
        return getInt( context, rowid, DBHelper.HASMSGS, 
                       GameSummary.MSG_FLAGS_NONE );
    }

    public static boolean getExpanded( Context context, long rowid )
    {
        return 0 == getInt( context, rowid, DBHelper.CONTRACTED, 0 );
    }

    public static boolean gameOver( Context context, long rowid ) 
    {
        return 0 != getInt( context, rowid, DBHelper.GAME_OVER, 0 );
    }

    public static void saveThumbnail( Context context, GameLock lock, 
                                      Bitmap thumb )
    {
        if ( BuildConstants.THUMBNAIL_SUPPORTED ) {
            long rowid = lock.getRowid();
            String selection = String.format( ROW_ID_FMT, rowid );
            ContentValues values = new ContentValues();

            if ( null == thumb ) {
                values.putNull( DBHelper.THUMBNAIL );
            } else {
                ByteArrayOutputStream bas = new ByteArrayOutputStream();
                thumb.compress( CompressFormat.PNG, 0, bas );
                values.put( DBHelper.THUMBNAIL, bas.toByteArray() );
            }

            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getWritableDatabase();

                long result = db.update( DBHelper.TABLE_NAME_SUM,
                                         values, selection, null );
                Assert.assertTrue( result >= 0 );

                db.close();

                notifyListeners( rowid, GameChangeType.GAME_CHANGED );
            }
        }
    }

    public static void clearThumbnails( Context context )
    {
        if ( BuildConstants.THUMBNAIL_SUPPORTED ) {
            ContentValues values = new ContentValues();
            values.putNull( DBHelper.THUMBNAIL );
            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getWritableDatabase();
                long result = db.update( DBHelper.TABLE_NAME_SUM,
                                         values, null, null );
                db.close();

                notifyListeners( ROWIDS_ALL, GameChangeType.GAME_CHANGED );
            }
        }
    }

    public static String getRelayID( Context context, long rowid )
    {
        String result = null;
        String[] columns = { DBHelper.RELAYID };
        String selection = String.format( ROW_ID_FMT, rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = 
                    cursor.getString( cursor.getColumnIndex(DBHelper.RELAYID) );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static HashMap<Long,CommsConnTypeSet> 
        getGamesWithSendsPending( Context context )
    {
        HashMap<Long, CommsConnTypeSet> result = new HashMap<Long,CommsConnTypeSet>();
        String[] columns = { ROW_ID, DBHelper.CONTYPE };
        String selection = String.format( "%s > 0", DBHelper.NPACKETSPENDING );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            int indx1 = cursor.getColumnIndex( ROW_ID );
            int indx2 = cursor.getColumnIndex( DBHelper.CONTYPE );
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                long rowid = cursor.getLong( indx1 );
                CommsConnTypeSet typs = new CommsConnTypeSet( cursor.getInt(indx2) );
                // Better have an address if has pending sends
                if ( 0 < typs.size() ) {
                    result.put( rowid, typs );
                }
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static long[] getRowIDsFor( Context context, String relayID )
    {
        long[] result = null;
        String[] columns = { ROW_ID };
        String selection = DBHelper.RELAYID + "='" + relayID + "'";
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            result = new long[cursor.getCount()];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                result[ii] = cursor.getLong( cursor.getColumnIndex(ROW_ID) );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static long[] getRowIDsFor( Context context, int gameID )
    {
        long[] result = null;
        String[] columns = { ROW_ID };
        String selection = String.format( DBHelper.GAMEID + "=%d", gameID );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            result = new long[cursor.getCount()];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                result[ii] = cursor.getLong( cursor.getColumnIndex(ROW_ID) );
            }
            cursor.close();
            db.close();
        }
        if ( null != result && 1 < result.length ) {
            DbgUtils.logf( "getRowIDsFor(%x)=>length %d array", gameID,
                           result.length );
        }
        return result;
    }

    public static boolean haveGame( Context context, int gameID ) 
    {
        long[] rows = getRowIDsFor( context, gameID );
        return rows != null && 0 < rows.length;
    }

    public static boolean haveGame( Context context, long rowid ) 
    {
        boolean result = false;
        String[] columns = { ROW_ID };
        String selection = String.format( ROW_ID + "=%d", rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            Assert.assertTrue( 1 >= cursor.getCount() );
            result = 1 == cursor.getCount();
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void listBTGames( Context context, 
                                    HashMap<String, int[]> result )
    {
        HashSet<Integer> set;
        String[] columns = { DBHelper.GAMEID, DBHelper.REMOTEDEVS };
        String selection = DBHelper.GAMEID + "!=0";
        HashMap<String, HashSet<Integer> > map = 
            new HashMap<String, HashSet<Integer> >();
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            while ( cursor.moveToNext() ) {
                int col = cursor.getColumnIndex( DBHelper.GAMEID );
                int gameID = cursor.getInt( col );
                col = cursor.getColumnIndex( DBHelper.REMOTEDEVS );
                String devs = cursor.getString( col );
                DbgUtils.logf( "gameid %d has remote[s] %s", gameID, devs );

                if ( null != devs && 0 < devs.length() ) {
                    for ( String dev : TextUtils.split( devs, "\n" ) ) {
                        set = map.get( dev );
                        if ( null == set ) {
                            set = new HashSet<Integer>();
                            map.put( dev, set );
                        }
                        set.add( new Integer(gameID) );
                    }
                }
            }
            cursor.close();
            db.close();
        }

        Set<String> devs = map.keySet();
        Iterator<String> iter = devs.iterator();
        while ( iter.hasNext() ) {
            String dev = iter.next();
            set = map.get( dev );
            int[] gameIDs = new int[set.size()];
            Iterator<Integer> idIter = set.iterator();
            for ( int ii = 0; idIter.hasNext(); ++ii ) {
                gameIDs[ii] = idIter.next();
            }
            result.put( dev, gameIDs );
        }
    }

    // Return creation time of newest game matching this nli, or null
    // if none found.
    public static Date getMostRecentCreate( Context context, int gameID )
    {
        Date result = null;

        String selection = String.format("%s=%d", DBHelper.GAMEID, gameID );
        String[] columns = { DBHelper.CREATE_TIME };

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( cursor.moveToNext() ) {
                int indx = cursor.getColumnIndex( columns[0] );
                result = new Date( cursor.getLong( indx ) );
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    public static String[] getRelayIDs( Context context, long[][] rowIDs ) 
    {
        String[] result = null;
        String[] columns = { ROW_ID, DBHelper.RELAYID };
        String selection = DBHelper.RELAYID + " NOT null";
        ArrayList<String> ids = new ArrayList<String>();

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            int count = cursor.getCount();
            if ( 0 < count ) {
                result = new String[count];
                if ( null != rowIDs ) {
                    rowIDs[0] = new long[count];
                }

                int idIndex = cursor.getColumnIndex(DBHelper.RELAYID);
                int rowIndex = cursor.getColumnIndex(ROW_ID);
                for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                    result[ii] = cursor.getString( idIndex );
                    if ( null != rowIDs ) {
                        rowIDs[0][ii] = cursor.getLong( rowIndex );
                    }
                }
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    public static boolean haveRelayIDs( Context context )
    {
        long[][] rowIDss = new long[1][];
        String[] relayIDs = getRelayIDs( context, rowIDss );
        boolean result = null != relayIDs && 0 < relayIDs.length;
        return result;
    }

    public static void addDeceased( Context context, String relayID, 
                                    int seed )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.RELAYID, relayID );
        values.put( DBHelper.SEED, seed );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            try {
                long result = db.replaceOrThrow( DBHelper.TABLE_NAME_OBITS,
                                                 "", values );
            } catch ( Exception ex ) {
                DbgUtils.loge( ex );
            }
            db.close();
        }
    }

    public static Obit[] listObits( Context context )
    {
        Obit[] result = null;
        ArrayList<Obit> al = new ArrayList<Obit>();
        String[] columns = { DBHelper.RELAYID, DBHelper.SEED };

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_OBITS, columns, 
                                      null, null, null, null, null );
            if ( 0 < cursor.getCount() ) {
                int idIndex = cursor.getColumnIndex( DBHelper.RELAYID );
                int seedIndex = cursor.getColumnIndex( DBHelper.SEED );
                while ( cursor.moveToNext() ) {
                    String relayID = cursor.getString( idIndex );
                    int seed = cursor.getInt( seedIndex );
                    al.add( new Obit( relayID, seed ) );
                }
            }
            cursor.close();
            db.close();
        }

        int siz = al.size();
        if ( siz > 0 ) {
            result = al.toArray( new Obit[siz] );
        }
        return result;
    }

    public static void clearObits( Context context, Obit[] obits )
    {
        String fmt = DBHelper.RELAYID + "= \"%s\" AND + " 
            + DBHelper.SEED + " = %d";

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            for ( Obit obit: obits ) {
                String selection = String.format( fmt, obit.m_relayID, 
                                                  obit.m_seed );
                db.delete( DBHelper.TABLE_NAME_OBITS, selection, null );
            }
            db.close();
        }
    }

    public static GameLock saveNewGame( Context context, byte[] bytes,
                                        long groupID, String name )
    {
        Assert.assertTrue( GROUPID_UNSPEC != groupID );
        GameLock lock = null;

        ContentValues values = new ContentValues();
        values.put( DBHelper.SNAPSHOT, bytes );

        long timestamp = new Date().getTime();
        values.put( DBHelper.CREATE_TIME, timestamp );
        values.put( DBHelper.LASTPLAY_TIME, timestamp );
        values.put( DBHelper.GROUPID, groupID );
        if ( null != name ) {
            values.put( DBHelper.GAME_NAME, name );
        }

        invalGroupsCache();  // do first in case any listener has cached data

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            values.put( DBHelper.VISID, maxVISID( db ) );

            long rowid = db.insert( DBHelper.TABLE_NAME_SUM, null, values );
            db.close();

            setCached( rowid, null ); // force reread

            lock = new GameLock( rowid, true ).lock();
            notifyListeners( rowid, GameChangeType.GAME_CREATED );
        }

        invalGroupsCache();     // then again after
        return lock;
    } // saveNewGame

    public static long saveGame( Context context, GameLock lock, 
                                 byte[] bytes, boolean setCreate )
    {
        Assert.assertTrue( lock.canWrite() );
        long rowid = lock.getRowid();

        ContentValues values = new ContentValues();
        values.put( DBHelper.SNAPSHOT, bytes );

        long timestamp = new Date().getTime();
        if ( setCreate ) {
            values.put( DBHelper.CREATE_TIME, timestamp );
        }
        values.put( DBHelper.LASTPLAY_TIME, timestamp );

        updateRow( context, DBHelper.TABLE_NAME_SUM, rowid, values );

        setCached( rowid, null ); // force reread
        if ( ROWID_NOTFOUND != rowid ) {      // Means new game?
            notifyListeners( rowid, GameChangeType.GAME_CHANGED );
        }
        invalGroupsCache();
        return rowid;
    }

    public static byte[] loadGame( Context context, GameLock lock )
    {
        long rowid = lock.getRowid();
        Assert.assertTrue( ROWID_NOTFOUND != rowid );
        byte[] result = getCached( rowid );
        if ( null == result ) {
            String[] columns = { DBHelper.SNAPSHOT };
            String selection = String.format( ROW_ID_FMT, rowid );
            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getReadableDatabase();

                Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                          selection, null, null, null, null );
                if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                    result = cursor.getBlob( cursor
                                             .getColumnIndex(DBHelper.SNAPSHOT));
                } else {
                    DbgUtils.logf( "DBUtils.loadGame: none for rowid=%d",
                                   rowid );
                }
                cursor.close();
                db.close();
            }
            setCached( rowid, result );
        }
        return result;
    }

    public static void deleteGame( Context context, long rowid )
    {
        GameLock lock = new GameLock( rowid, true ).lock( 300 );
        if ( null != lock ) {
            deleteGame( context, lock );
            lock.unlock();
        } else {
            DbgUtils.logf( "deleteGame: unable to lock rowid %d", rowid );
        }
    }

    public static void deleteGame( Context context, GameLock lock )
    {
        Assert.assertTrue( lock.canWrite() );
        String selSummaries = String.format( ROW_ID_FMT, lock.getRowid() );
        String selInvites = String.format( "%s=%d", DBHelper.ROW, lock.getRowid() );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.delete( DBHelper.TABLE_NAME_SUM, selSummaries, null );

            // Delete invitations too
            db.delete( DBHelper.TABLE_NAME_INVITES, selInvites, null );
            
            db.close();
        }
        notifyListeners( lock.getRowid(), GameChangeType.GAME_DELETED );
        invalGroupsCache();
    }

    public static int getVisID( Context context, long rowid )
    {
        int result = ROWID_NOTFOUND;
        String[] columns = { DBHelper.VISID };
        String selection = String.format( ROW_ID_FMT, rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getInt( cursor
                                        .getColumnIndex(DBHelper.VISID));
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    // Get either the file name or game name, preferring the latter.
    public static String getName( Context context, long rowid )
    {
        String result = null;
        String[] columns = { DBHelper.GAME_NAME };
        String selection = String.format( ROW_ID_FMT, rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();

            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getString( cursor
                                           .getColumnIndex(DBHelper.GAME_NAME));
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    public static void setName( Context context, long rowid, String name )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.GAME_NAME, name );
        updateRow( context, DBHelper.TABLE_NAME_SUM, rowid, values );
    }

    public static HistoryPair[] getChatHistory( Context context, long rowid )
    {
        HistoryPair[] result = null;
        if ( BuildConstants.CHAT_SUPPORTED ) {
            final String localPrefix =
                LocUtils.getString( context, R.string.chat_local_id );
            String history = getChatHistoryStr( context, rowid );
            if ( null != history ) {
                String[] msgs = history.split( "\n" );
                result = new HistoryPair[msgs.length];
                for ( int ii = 0; ii < result.length; ++ii ) {
                    String msg = msgs[ii];
                    boolean isLocal = msg.startsWith( localPrefix );
                    result[ii] = new HistoryPair( msg, isLocal );
                }
            }
        }
        return result;
    }

    public static class NeedsNagInfo {
        public long m_rowid;
        public long m_nextNag;
        public long m_lastMoveMillis;
        private boolean m_isSolo;

        public NeedsNagInfo( long rowid, long nextNag, long lastMove, 
                             CurGameInfo.DeviceRole role ) {
            m_rowid = rowid;
            m_nextNag = nextNag;
            m_lastMoveMillis = 1000 * lastMove;
            m_isSolo = CurGameInfo.DeviceRole.SERVER_STANDALONE == role;
        }

        public boolean isSolo() {
            return m_isSolo;
        }
    }

    public static NeedsNagInfo[] getNeedNagging( Context context )
    {
        NeedsNagInfo[] result = null;
        long now = new Date().getTime(); // in milliseconds
        String[] columns = { ROW_ID, DBHelper.NEXTNAG, DBHelper.LASTMOVE, 
                             DBHelper.SERVERROLE };
        // where nextnag > 0 AND nextnag < now
        String selection = 
            String.format( "%s > 0 AND %s < %s", DBHelper.NEXTNAG, 
                           DBHelper.NEXTNAG, now );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            int count = cursor.getCount();
            if ( 0 < count ) {
                result = new NeedsNagInfo[count];
                int rowIndex = cursor.getColumnIndex(ROW_ID);
                int nagIndex = cursor.getColumnIndex( DBHelper.NEXTNAG );
                int lastMoveIndex = cursor.getColumnIndex( DBHelper.LASTMOVE );
                int roleIndex = cursor.getColumnIndex( DBHelper.SERVERROLE );
                for ( int ii = 0; ii < result.length && cursor.moveToNext(); ++ii ) {
                    long rowid = cursor.getLong( rowIndex );
                    long nextNag = cursor.getLong( nagIndex );
                    long lastMove = cursor.getLong( lastMoveIndex );
                    CurGameInfo.DeviceRole role = 
                        CurGameInfo.DeviceRole.values()[cursor.getInt( roleIndex )];
                    result[ii] = new NeedsNagInfo( rowid, nextNag, lastMove, role );
                }
            }

            cursor.close();
            db.close();
        }
        return result;
    }

    public static long getNextNag( Context context )
    {
        long result = 0;
        String[] columns = { "MIN(" + DBHelper.NEXTNAG + ") as min" };
        String selection = "NOT " + DBHelper.NEXTNAG + "= 0";
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, null, null, null, null );
            if ( cursor.moveToNext() ) {
                result = cursor.getLong( cursor.getColumnIndex( "min" ) );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void updateNeedNagging( Context context, NeedsNagInfo[] needNagging )
    {
        String updateQuery = "update %s set %s = ? "
            + " WHERE %s = ? ";
        updateQuery = String.format( updateQuery, DBHelper.TABLE_NAME_SUM,
                                     DBHelper.NEXTNAG, ROW_ID );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            SQLiteStatement updateStmt = db.compileStatement( updateQuery );

            for ( NeedsNagInfo info : needNagging ) {
                updateStmt.bindLong( 1, info.m_nextNag );
                updateStmt.bindLong( 2, info.m_rowid );
                updateStmt.execute();
            }
            db.close();
        }
    }

    // Groups stuff
    public static class GameGroupInfo {
        public String m_name;
        public int m_count;
        public boolean m_expanded;
        public long m_lastMoveTime;
        public boolean m_hasTurn;
        public boolean m_turnLocal;

        public GameGroupInfo( String name, int count, boolean expanded ) {
            m_name = name; m_expanded = expanded;
            m_lastMoveTime = 0;
            m_count = count;
        }
    }

    private static HashMap<Long,GameGroupInfo> s_groupsCache = null;

    private static void invalGroupsCache() 
    {
        s_groupsCache = null;
    }

    public static Bitmap getThumbnail( Context context, long rowid )
    {
        Bitmap thumb = null;
        if ( BuildConstants.THUMBNAIL_SUPPORTED ) {
            byte[] data = null;
            String[] columns = { DBHelper.THUMBNAIL };
            String selection = String.format( ROW_ID_FMT, rowid );

            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getReadableDatabase();
                Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                          selection, null, null, null, null );
                if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                    data = cursor.getBlob( cursor.
                                           getColumnIndex(DBHelper.THUMBNAIL));
                }
                cursor.close();
                db.close();
            }

            if ( null != data ) {
                thumb = BitmapFactory.decodeByteArray( data, 0, data.length );
            }
        }
        return thumb;
    }

    private static HashMap<Long, Integer> getGameCounts( SQLiteDatabase db )
    {
        HashMap<Long, Integer> result = new HashMap<Long, Integer>();
        String query = "SELECT %s, count(%s) as cnt FROM %s GROUP BY %s";
        query = String.format( query, DBHelper.GROUPID, DBHelper.GROUPID,
                               DBHelper.TABLE_NAME_SUM, DBHelper.GROUPID );
        
        Cursor cursor = db.rawQuery( query, null );
        int rowIndex = cursor.getColumnIndex( DBHelper.GROUPID );
        int cntIndex = cursor.getColumnIndex( "cnt" );
        while ( cursor.moveToNext() ) {
            long row = cursor.getLong(rowIndex);
            int count = cursor.getInt(cntIndex);
            result.put( row, count );
        }
        cursor.close();
        return result;
    }

    // Map of groups rowid (= summaries.groupid) to group info record
    protected static HashMap<Long,GameGroupInfo> getGroups( Context context )
    {
        if ( null == s_groupsCache ) {
            HashMap<Long,GameGroupInfo> result = 
                new HashMap<Long,GameGroupInfo>();

            // Select all groups.  For each group get the number of games in
            // that group.  There should be a way to do that with one query
            // but I can't figure it out.

            String query = "SELECT rowid, groupname as groups_groupname, "
                + " groups.expanded as groups_expanded FROM groups";

            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getReadableDatabase();

                HashMap<Long, Integer> map = getGameCounts( db );

                Cursor cursor = db.rawQuery( query, null );
                int idIndex = cursor.getColumnIndex( "rowid" );
                int nameIndex = cursor.getColumnIndex( "groups_groupname" );
                int expandedIndex = cursor.getColumnIndex( "groups_expanded" );

                while ( cursor.moveToNext() ) {
                    long id = cursor.getLong( idIndex );
                    String name = cursor.getString( nameIndex );
                    Assert.assertNotNull( name );
                    boolean expanded = 0 != cursor.getInt( expandedIndex );
                    int count = map.containsKey( id ) ? map.get( id ) : 0;
                    result.put( id, new GameGroupInfo( name, count, expanded ) );
                }
                cursor.close();

                Iterator<Long> iter = result.keySet().iterator();
                while ( iter.hasNext() ) {
                    Long groupID = iter.next();
                    GameGroupInfo ggi = result.get( groupID );
                    readTurnInfo( db, groupID, ggi );
                }

                db.close();
            }
            s_groupsCache = result;
        }
        return s_groupsCache;
    } // getGroups

    // public static void unhideTo( Context context, long groupID )
    // {
    //     Assert.assertTrue( GROUPID_UNSPEC != groupID );
    //     initDB( context );
    //     synchronized( s_dbHelper ) {
    //         SQLiteDatabase db = s_dbHelper.getWritableDatabase();
    //         ContentValues values = new ContentValues();
    //         values.put( DBHelper.GROUPID, groupID );
    //         String selection = String.format( "%s = %d", DBHelper.GROUPID,
    //                                           GROUPID_UNSPEC );
    //         long result = db.update( DBHelper.TABLE_NAME_SUM,
    //                                  values, selection, null );
    //         db.close();

    //         notifyListeners( ROWID_NOTFOUND, true );
    //     }
    // }

    private static void readTurnInfo( SQLiteDatabase db, long groupID, 
                                      GameGroupInfo ggi )
    {
        String[] columns = { DBHelper.LASTMOVE, DBHelper.GIFLAGS, 
                             DBHelper.TURN };
        String orderBy = DBHelper.LASTMOVE;
        String selection = String.format( "%s=%d", DBHelper.GROUPID, groupID );
        Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                  selection,
                                  null, // args
                                  null, // groupBy,
                                  null, // having
                                  orderBy
                                  );
        
        // We want the earliest LASTPLAY_TIME (i.e. the first we see
        // since they're in order) that's a local turn, if any,
        // otherwise a non-local turn.
        long lastPlayTimeLocal = 0;
        long lastPlayTimeRemote = 0;
        int indexLPT = cursor.getColumnIndex( DBHelper.LASTMOVE );
        int indexFlags = cursor.getColumnIndex( DBHelper.GIFLAGS );
        int turnFlags = cursor.getColumnIndex( DBHelper.TURN );
        while ( cursor.moveToNext() && 0 == lastPlayTimeLocal ) {
            int flags = cursor.getInt( indexFlags );
            int turn = cursor.getInt( turnFlags );
            Boolean isLocal = GameSummary.localTurnNext( flags, turn );
            if ( null != isLocal ) {
                long lpt = cursor.getLong( indexLPT );
                if ( isLocal ) {
                    lastPlayTimeLocal = lpt;
                } else if ( 0 == lastPlayTimeRemote ) {
                    lastPlayTimeRemote = lpt;
                }
            }
        }
        cursor.close();

        ggi.m_hasTurn = 0 != lastPlayTimeLocal || 0 != lastPlayTimeRemote;
        if ( ggi.m_hasTurn ) {
            ggi.m_turnLocal = 0 != lastPlayTimeLocal;
            if ( ggi.m_turnLocal ) {
                ggi.m_lastMoveTime = lastPlayTimeLocal;
            } else {
                ggi.m_lastMoveTime = lastPlayTimeRemote;
            }
            // DateFormat df = DateFormat.getDateTimeInstance( DateFormat.SHORT, 
            //                                                 DateFormat.SHORT );
            // DbgUtils.logf( "using last play time %s for", 
            //                df.format( new Date( 1000 * ggi.m_lastMoveTime ) ) );
        }
    }

    public static int countGames( Context context )
    {
        int result = 0;
        String[] columns = { ROW_ID };
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      null, // selection
                                      null, // args
                                      null, // groupBy
                                      null, // having
                                      null
                                      );
            result = cursor.getCount();
            cursor.close();
            db.close();
        }
        return result;
    }

    public static long[] getGroupGames( Context context, long groupID )
    {
        long[] result = null;
        initDB( context );
        String[] columns = { ROW_ID };
        String selection = String.format( "%s=%d", DBHelper.GROUPID, groupID );
        String orderBy = DBHelper.CREATE_TIME + " DESC";
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, // selection
                                      null, // args
                                      null, // groupBy
                                      null, // having
                                      orderBy
                                      );
            int index = cursor.getColumnIndex( ROW_ID );
            result = new long[ cursor.getCount() ];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                long rowid = cursor.getInt( index );
                result[ii] = rowid;
            }
            cursor.close();
            db.close();
        }

        return result;
    }

    // pass ROWID_NOTFOUND to get *any* group.  Because there may be
    // some hidden games stored with group = -1 thanks to
    // recently-fixed bugs, be sure to skip them.
    public static long getGroupForGame( Context context, long rowid )
    {
        long result = GROUPID_UNSPEC;
        initDB( context );
        String[] columns = { DBHelper.GROUPID };

        String selection = String.format( "%s != %d", DBHelper.GROUPID,
                                          DBUtils.GROUPID_UNSPEC );
        if ( ROWID_NOTFOUND != rowid ) {
            selection += " AND " + String.format( ROW_ID_FMT, rowid );
        }

        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                      selection, // selection
                                      null, // args
                                      null, // groupBy
                                      null, // having
                                      null //orderby 
                                      );
            if ( cursor.moveToNext() ) {
                int index = cursor.getColumnIndex( DBHelper.GROUPID );
                result = cursor.getLong( index );
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static long getAnyGroup( Context context )
    {
        long result = GROUPID_UNSPEC;
        HashMap<Long,GameGroupInfo> groups = getGroups( context );
        Iterator<Long> iter = groups.keySet().iterator();
        if ( iter.hasNext() ) {
            result = iter.next();
        }
        Assert.assertTrue( GROUPID_UNSPEC != result );
        return result;
    }

    public static long addGroup( Context context, String name )
    {
        long rowid = GROUPID_UNSPEC;
        if ( null != name && 0 < name.length() ) {
            HashMap<Long,GameGroupInfo> gameInfo = getGroups( context );
            if ( null == gameInfo.get( name ) ) {
                ContentValues values = new ContentValues();
                values.put( DBHelper.GROUPNAME, name );
                values.put( DBHelper.EXPANDED, 1 );

                // initDB( context ); <- getGroups will have called this
                synchronized( s_dbHelper ) {
                    SQLiteDatabase db = s_dbHelper.getWritableDatabase();
                    rowid = db.insert( DBHelper.TABLE_NAME_GROUPS, null, 
                                       values );
                    db.close();
                }
                invalGroupsCache();
            }
        }
        return rowid;
    }
    
    public static void deleteGroup( Context context, long groupid )
    {
        // Nuke games having this group id
        String selectionGames = 
            String.format( "%s=%d", DBHelper.GROUPID, groupid );

        // And nuke the group record itself
        String selectionGroups = String.format( ROW_ID_FMT, groupid );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.delete( DBHelper.TABLE_NAME_SUM, selectionGames, null );
            db.delete( DBHelper.TABLE_NAME_GROUPS, selectionGroups, null );

            db.close();
        }
        invalGroupsCache();
    }

    public static void setGroupName( Context context, long groupid, 
                                     String name )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.GROUPNAME, name );
        updateRow( context, DBHelper.TABLE_NAME_GROUPS, groupid, values );
        invalGroupsCache();
    }

    public static void setGroupExpanded( Context context, long groupid, 
                                         boolean expanded )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.EXPANDED, expanded? 1 : 0 );
        updateRow( context, DBHelper.TABLE_NAME_GROUPS, groupid, values );
        invalGroupsCache();
    }

    // Change group id of a game
    public static void moveGame( Context context, long gameid, long groupID )
    {
        Assert.assertTrue( GROUPID_UNSPEC != groupID );
        ContentValues values = new ContentValues();
        values.put( DBHelper.GROUPID, groupID );
        updateRow( context, DBHelper.TABLE_NAME_SUM, gameid, values );
    }

    private static String getChatHistoryStr( Context context, long rowid )
    {
        String result = null;
        if ( BuildConstants.CHAT_SUPPORTED ) {
            String[] columns = { DBHelper.CHAT_HISTORY };
            String selection = String.format( ROW_ID_FMT, rowid );
            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getReadableDatabase();

                Cursor cursor = db.query( DBHelper.TABLE_NAME_SUM, columns, 
                                          selection, null, null, null, null );
                if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                    result = 
                        cursor.getString( cursor
                                          .getColumnIndex(DBHelper
                                                          .CHAT_HISTORY));
                }
                cursor.close();
                db.close();
            }
        }
        return result;
    }

    public static void appendChatHistory( Context context, long rowid,
                                          String msg, boolean local )
    {
        if ( BuildConstants.CHAT_SUPPORTED ) {
            Assert.assertNotNull( msg );
            int id = local ? R.string.chat_local_id : R.string.chat_other_id;
            msg = LocUtils.getString( context, id ) + msg;

            String cur = getChatHistoryStr( context, rowid );
            if ( null != cur ) {
                msg = cur + "\n" + msg;
            }

            saveChatHistory( context, rowid, msg );
        }
    } // appendChatHistory

    public static void clearChatHistory( Context context, long rowid )
    {
        saveChatHistory( context, rowid, null );
    }

    public static void setDBChangeListener( DBChangeListener listener )
    {
        synchronized( s_listeners ) {
            Assert.assertNotNull( listener );
            s_listeners.add( listener );
        }
    }

    public static void clearDBChangeListener( DBChangeListener listener )
    {
        synchronized( s_listeners ) {
            Assert.assertTrue( s_listeners.contains( listener ) );
            s_listeners.remove( listener );
        }
    }

    public static void loadDB( Context context )
    {
        copyGameDB( context, false );
    }

    public static void saveDB( Context context )
    {
        copyGameDB( context, true );
    }

    public static boolean copyFileStream( FileOutputStream fos,
                                          FileInputStream fis )
    {
        boolean success = false;
        FileChannel channelSrc = null;
        FileChannel channelDest = null;
        try {
            channelSrc = fis.getChannel();
            channelDest = fos.getChannel();
            channelSrc.transferTo( 0, channelSrc.size(), channelDest );
            success = true;
        } catch( java.io.IOException ioe ) {
            DbgUtils.loge( ioe );
        } finally {
            try {
                channelSrc.close();
                channelDest.close();
            } catch( java.io.IOException ioe ) {
                DbgUtils.loge( ioe );
            }
        }
        return success;
    }

    /////////////////////////////////////////////////////////////////
    // DictsDB stuff
    /////////////////////////////////////////////////////////////////
    public static DictBrowseState dictsGetOffset( Context context, String name,
                                                  DictLoc loc )
    {
        Assert.assertTrue( DictLoc.UNKNOWN != loc );
        DictBrowseState result = null;
        String[] columns = { DBHelper.ITERPOS, DBHelper.ITERTOP,
                             DBHelper.ITERMIN, DBHelper.ITERMAX,
                             DBHelper.WORDCOUNTS, DBHelper.ITERPREFIX };
        String selection = 
            String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                           name, DBHelper.LOC, loc.ordinal() );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_DICTBROWSE, columns, 
                                      selection, null, null, null, null );
            if ( 1 >= cursor.getCount() && cursor.moveToFirst() ) {
                result = new DictBrowseState();
                result.m_pos = cursor.getInt( cursor
                                              .getColumnIndex(DBHelper.ITERPOS));
                result.m_top = cursor.getInt( cursor
                                              .getColumnIndex(DBHelper.ITERTOP));
                result.m_minShown = 
                    cursor.getInt( cursor
                                   .getColumnIndex(DBHelper.ITERMIN));
                result.m_maxShown = 
                    cursor.getInt( cursor
                                   .getColumnIndex(DBHelper.ITERMAX));
                result.m_prefix = 
                    cursor.getString( cursor
                                      .getColumnIndex(DBHelper.ITERPREFIX));
                String counts = 
                    cursor.getString( cursor.getColumnIndex(DBHelper.WORDCOUNTS));
                if ( null != counts ) {
                    String[] nums = TextUtils.split( counts, ":" );
                    int[] ints = new int[nums.length];
                    for ( int ii = 0; ii < nums.length; ++ii ) {
                        ints[ii] = Integer.parseInt( nums[ii] );
                    }
                    result.m_counts = ints;
                }
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void dictsSetOffset( Context context, String name, 
                                       DictLoc loc, DictBrowseState state )
    {
        Assert.assertTrue( DictLoc.UNKNOWN != loc );
        String selection = 
            String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                           name, DBHelper.LOC, loc.ordinal() );
        ContentValues values = new ContentValues();
        values.put( DBHelper.ITERPOS, state.m_pos );
        values.put( DBHelper.ITERTOP, state.m_top );
        values.put( DBHelper.ITERMIN, state.m_minShown );
        values.put( DBHelper.ITERMAX, state.m_maxShown );
        values.put( DBHelper.ITERPREFIX, state.m_prefix );
        if ( null != state.m_counts ) {
            String[] nums = new String[state.m_counts.length];
            for ( int ii = 0; ii < nums.length; ++ii ) {
                nums[ii] = String.format( "%d", state.m_counts[ii] );
            }
            values.put( DBHelper.WORDCOUNTS, TextUtils.join( ":", nums ) );
        }

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            int result = db.update( DBHelper.TABLE_NAME_DICTBROWSE,
                                    values, selection, null );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, name );
                values.put( DBHelper.LOC, loc.ordinal() );
                db.insert( DBHelper.TABLE_NAME_DICTBROWSE, null, values );
            }
            db.close();
        }
    }

    // Called from jni
    public static String dictsGetMD5Sum( Context context, String name )
    {
        DictInfo info = dictsGetInfo( context, name );
        String result = null == info? null : info.md5Sum;
        return result;
    }

    // Called from jni
    public static void dictsSetMD5Sum( Context context, String name, String sum )
    {
        String selection = String.format( NAME_FMT, DBHelper.DICTNAME, name );
        ContentValues values = new ContentValues();
        values.put( DBHelper.MD5SUM, sum );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            int result = db.update( DBHelper.TABLE_NAME_DICTINFO,
                                    values, selection, null );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, name );
                db.insert( DBHelper.TABLE_NAME_DICTINFO, null, values );
            }
            db.close();
        }
    }

    public static DictInfo dictsGetInfo( Context context, String name )
    {
        DictInfo result = null;
        String[] columns = { DBHelper.LANGCODE,
                             DBHelper.WORDCOUNT,
                             DBHelper.MD5SUM,
                             DBHelper.LOC };
        String selection = String.format( NAME_FMT, DBHelper.DICTNAME, name );
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_DICTINFO, columns, 
                                      selection, null, null, null, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = new DictInfo();
                result.name = name;
                result.langCode = 
                    cursor.getInt( cursor.getColumnIndex(DBHelper.LANGCODE));
                result.wordCount = 
                    cursor.getInt( cursor.getColumnIndex(DBHelper.WORDCOUNT));
                result.md5Sum =
                    cursor.getString( cursor.getColumnIndex(DBHelper.MD5SUM));
             }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void dictsSetInfo( Context context, DictUtils.DictAndLoc dal,
                                     DictInfo info )
    {
        String selection = 
            String.format( NAME_FMT, DBHelper.DICTNAME, dal.name );
        ContentValues values = new ContentValues();

        values.put( DBHelper.LANGCODE, info.langCode );
        values.put( DBHelper.WORDCOUNT, info.wordCount );
        values.put( DBHelper.MD5SUM, info.md5Sum );
        values.put( DBHelper.LOC, dal.loc.ordinal() );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            int result = db.update( DBHelper.TABLE_NAME_DICTINFO,
                                    values, selection, null );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, dal.name );
                db.insert( DBHelper.TABLE_NAME_DICTINFO, null, values );
            }
            db.close();
        }
    }

    public static void dictsMoveInfo( Context context, String name,
                                      DictLoc fromLoc, DictLoc toLoc )
    {
        String selection = 
            String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                           name, DBHelper.LOC, fromLoc.ordinal() );
        ContentValues values = new ContentValues();
        values.put( DBHelper.LOC, toLoc.ordinal() );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.update( DBHelper.TABLE_NAME_DICTINFO, values, selection, null );
            db.update( DBHelper.TABLE_NAME_DICTBROWSE, values, selection, null );
            db.close();
        }
    }

    public static void dictsRemoveInfo( Context context, 
                                        DictUtils.DictAndLoc dal )
    {
        String selection = 
            String.format( NAMELOC_FMT, DBHelper.DICTNAME, 
                           dal.name, DBHelper.LOC, dal.loc.ordinal() );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.delete( DBHelper.TABLE_NAME_DICTINFO, selection, null );
            db.delete( DBHelper.TABLE_NAME_DICTBROWSE, selection, null );
            db.close();
        }
    }

    public static boolean gameDBExists( Context context )
    {
        String varName = getVariantDBName();
        boolean exists = new File( Environment.getExternalStorageDirectory(),
                                   varName ).exists();
        if ( !exists ) {
            // try the old one
            exists = new File( Environment.getExternalStorageDirectory(),
                               DBHelper.getDBName() ).exists();
        }
        return exists;
    }

    public static String[] getColumns( SQLiteDatabase db, String name )
    {
        String query = String.format( "SELECT * FROM %s LIMIT 1", name );
        Cursor cursor = db.rawQuery( query, null );
        String[] colNames = cursor.getColumnNames();
        cursor.close();
        return colNames;
    }

    public static void addToStudyList( Context context, String word,
                                       int lang )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.WORD, word );
        values.put( DBHelper.LANGUAGE, lang );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.insert( DBHelper.TABLE_NAME_STUDYLIST, null, values );
            db.close();
        }
    }

    public static int[] studyListLangs( Context context )
    {
        int[] result = null;
        String groupBy = DBHelper.LANGUAGE;
        String selection = null;//DBHelper.LANGUAGE;
        String[] columns = { DBHelper.LANGUAGE };

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_STUDYLIST, columns, 
                                      null, null, groupBy, null, null );
            int count = cursor.getCount();
            result = new int[count];
            if ( 0 < count ) {
                int index = 0;
                int colIndex = cursor.getColumnIndex( DBHelper.LANGUAGE );
                while ( cursor.moveToNext() ) {
                    result[index++] = cursor.getInt(colIndex);
                }
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static String[] studyListWords( Context context, int lang )
    {
        String[] result = null;
        String selection = String.format( "%s = %d", DBHelper.LANGUAGE, lang );
        String[] columns = { DBHelper.WORD };
        String orderBy = DBHelper.WORD;

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_STUDYLIST, columns, 
                                      selection, null, null, null, orderBy );
            int count = cursor.getCount();
            result = new String[count];
            if ( 0 < count ) {
                int index = 0;
                int colIndex = cursor.getColumnIndex( DBHelper.WORD );
                while ( cursor.moveToNext() ) {
                    result[index++] = cursor.getString(colIndex);
                }
            }
            cursor.close();
            db.close();
        }
        return result;
    }

    public static void studyListClear( Context context, int lang, String[] words )
    {
        String selection = String.format( "%s = %d", DBHelper.LANGUAGE, lang );
        if ( null != words ) {
            selection += String.format( " AND %s in ('%s')", DBHelper.WORD,
                                        TextUtils.join("','", words) );
        }

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.delete( DBHelper.TABLE_NAME_STUDYLIST, selection, null );
            db.close();
        }
    }

    public static void studyListClear( Context context, int lang )
    {
        studyListClear( context, lang, null );
    }

    public static void saveXlations( Context context, String locale,
                                     Map<String, String> data, boolean blessed )
    {
        if ( null != data && 0 < data.size() ) {
            long blessedLong = blessed ? 1 : 0;
            Iterator<String> iter = data.keySet().iterator();

            String insertQuery = "insert into %s (%s, %s, %s, %s) "
                + " VALUES (?, ?, ?, ?)";
            insertQuery = String.format( insertQuery, DBHelper.TABLE_NAME_LOC,
                                         DBHelper.KEY, DBHelper.LOCALE,
                                         DBHelper.BLESSED, DBHelper.XLATION );
                                         
            String updateQuery = "update %s set %s = ? "
                + " WHERE %s = ? and %s = ? and %s = ?";
            updateQuery = String.format( updateQuery, DBHelper.TABLE_NAME_LOC,
                                         DBHelper.XLATION, DBHelper.KEY, 
                                         DBHelper.LOCALE, DBHelper.BLESSED );

            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteDatabase db = s_dbHelper.getWritableDatabase();
                SQLiteStatement insertStmt = db.compileStatement( insertQuery );
                SQLiteStatement updateStmt = db.compileStatement( updateQuery );

                while ( iter.hasNext() ) {
                    String key = iter.next();
                    String xlation = data.get( key );
                    // DbgUtils.logf( "adding key %s, xlation %s, locale %s, blessed: %d",
                    //                key, xlation, locale, blessedLong );

                    insertStmt.bindString( 1, key );
                    insertStmt.bindString( 2, locale );
                    insertStmt.bindLong( 3, blessedLong );
                    insertStmt.bindString( 4, xlation );
                    
                    try {
                        insertStmt.execute();
                    } catch ( SQLiteConstraintException sce ) {

                        updateStmt.bindString( 1, xlation );
                        updateStmt.bindString( 2, key );
                        updateStmt.bindString( 3, locale );
                        updateStmt.bindLong( 4, blessedLong );

                        try {
                            updateStmt.execute();
                        } catch ( Exception ex ) {
                            DbgUtils.loge( ex );
                            Assert.fail();
                        }
                    }
                }
                db.close();
            }
        }
    }

    // You can't have an array of paramterized types in java, so we'll let the
    // caller cast.
    public static Object[] getXlations( Context context, String locale )
    {
        HashMap<String, String> local = new HashMap<String, String>();
        HashMap<String, String> blessed = new HashMap<String, String>();

        String selection = String.format( "%s = '%s'", DBHelper.LOCALE, 
                                          locale );
        String[] columns = { DBHelper.KEY, DBHelper.XLATION, DBHelper.BLESSED };

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            Cursor cursor = db.query( DBHelper.TABLE_NAME_LOC, columns, 
                                      selection, null, null, null, null );
            int keyIndex = cursor.getColumnIndex( DBHelper.KEY );
            int valueIndex = cursor.getColumnIndex( DBHelper.XLATION );
            int blessedIndex = cursor.getColumnIndex( DBHelper.BLESSED );
            while ( cursor.moveToNext() ) {
                String key = cursor.getString( keyIndex );
                String value = cursor.getString( valueIndex );
                HashMap<String, String> map =
                    (0 == cursor.getInt( blessedIndex )) ? local : blessed;
                map.put( key, value );
            }
            cursor.close();
            db.close();
        }

        Object result[] = new Object[] { local, blessed };
        return result;
    }

    public static void dropXLations( Context context, String locale )
    {
        String selection = String.format( "%s = '%s'", DBHelper.LOCALE, 
                                          locale );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            db.delete( DBHelper.TABLE_NAME_LOC, selection, null );
            db.close();
        }
    }

    private static void setStringForSync( SQLiteDatabase db, String key, String value )
    {
        String selection = String.format( "%s = '%s'", DBHelper.KEY, key );
        ContentValues values = new ContentValues();
        values.put( DBHelper.VALUE, value );

        long result = db.update( DBHelper.TABLE_NAME_PAIRS,
                                 values, selection, null );
        if ( 0 == result ) {
            values.put( DBHelper.KEY, key );
            db.insert( DBHelper.TABLE_NAME_PAIRS, null, values );
        }
    }

    private static String getStringForSync( SQLiteDatabase db, String key, String dflt )
    {
        String selection = String.format( "%s = '%s'", DBHelper.KEY, key );
        String[] columns = { DBHelper.VALUE };

        Cursor cursor = db.query( DBHelper.TABLE_NAME_PAIRS, columns, 
                                  selection, null, null, null, null );
        Assert.assertTrue( 1 >= cursor.getCount() );
        int indx = cursor.getColumnIndex( DBHelper.VALUE );
        if ( cursor.moveToNext() ) {
            dflt = cursor.getString( indx );
        }
        cursor.close();
        return dflt;
    }

    private static interface Modifier {
        public String modifySync( String curVal );
    }

    private static String getModStringFor( Context context, String key, Modifier proc )
    {
        String result = null;
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            result = getStringForSync( db, key, null );
            result = proc.modifySync( result );
            setStringForSync( db, key, result );
            db.close();
        }
        return result;
    }

    public static void setStringFor( Context context, String key, String value )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();
            setStringForSync( db, key, value );
            db.close();
        }
    }

    public static String getStringFor( Context context, String key, String dflt )
    {
        String selection = String.format( "%s = '%s'", DBHelper.KEY, key );
        String[] columns = { DBHelper.VALUE };

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getReadableDatabase();
            dflt = getStringForSync( db, key, dflt );
            db.close();
        }
        return dflt;
    }

    public static void setIntFor( Context context, String key, int value )
    {
        // DbgUtils.logdf( "DBUtils.setIntFor(key=%s, val=%d)", key, value );
        String asStr = String.format( "%d", value );
        setStringFor( context, key, asStr );
    }

    public static int getIntFor( Context context, String key, int dflt )
    {
        String asStr = getStringFor( context, key, null );
        if ( null != asStr ) {
            dflt = Integer.parseInt( asStr );
        }
        // DbgUtils.logdf( "DBUtils.getIntFor(key=%s)=>%d", key, dflt );
        return dflt;
    }

    public static void setBoolFor( Context context, String key, boolean value )
    {
        // DbgUtils.logdf( "DBUtils.setBoolFor(key=%s, val=%b)", key, value );
        String asStr = String.format( "%b", value );
        setStringFor( context, key, asStr );
    }

    public static boolean getBoolFor( Context context, String key, boolean dflt )
    {
        String asStr = getStringFor( context, key, null );
        if ( null != asStr ) {
            dflt = Boolean.parseBoolean( asStr );
        }
        // DbgUtils.logdf( "DBUtils.getBoolFor(key=%s)=>%b", key, dflt );
        return dflt;
    }

    public static int getIncrementIntFor( Context context, String key, int dflt,
                                          final int incr )
    {
        Modifier proc = new Modifier() {
                public String modifySync( String curVal ) {
                    int val = null == curVal ? 0 : Integer.parseInt( curVal );
                    String newVal = String.format( "%d", val + incr );
                    return newVal;
                }
            };
        String newVal = getModStringFor( context, key, proc );
        int asInt = Integer.parseInt( newVal );
        // DbgUtils.logf( "getIncrementIntFor(%s) => %d", key, asInt );
        return asInt;
    }

    public static void setBytesFor( Context context, String key, byte[] bytes )
    {
        // DbgUtils.logf( "setBytesFor: writing %d bytes", bytes.length );
        String asStr = XwJNI.base64Encode( bytes );
        setStringFor( context, key, asStr );
    }

    public static byte[] getBytesFor( Context context, String key )
    {
        byte[] bytes = null;
        String asStr = getStringFor( context, key, null );
        if ( null != asStr ) {
            bytes = XwJNI.base64Decode( asStr );
            // DbgUtils.logf( "getBytesFor: loaded %d bytes", bytes.length );
        }
        return bytes;
    }

    private static void copyGameDB( Context context, boolean toSDCard )
    {
        String name = DBHelper.getDBName();
        File gamesDB = context.getDatabasePath( name );

        // Use the variant name EXCEPT where we're copying from sdCard and
        // only the older name exists.
        File sdcardDB = new File( Environment.getExternalStorageDirectory(),
                                  getVariantDBName() );
        if ( !toSDCard && !sdcardDB.exists() ) {
            sdcardDB = new File( Environment.getExternalStorageDirectory(),
                                 name );
        }
        
        try {
            File srcDB = toSDCard? gamesDB : sdcardDB;
            if ( srcDB.exists() ) {
                FileInputStream src = new FileInputStream( srcDB );
                FileOutputStream dest = 
                    new FileOutputStream( toSDCard? sdcardDB : gamesDB );
                copyFileStream( dest, src );
                invalGroupsCache();
            }
        } catch( java.io.FileNotFoundException fnfe ) {
            DbgUtils.loge( fnfe );
        }
    }

    private static String getVariantDBName()
    {
        return String.format( "%s_%s", DBHelper.getDBName(),
                              BuildConstants.VARIANT );
    }
    
    // Chat is independent of the GameLock mechanism because it's not
    // touching the SNAPSHOT column.
    private static void saveChatHistory( Context context, long rowid,
                                         String history )
    {
        ContentValues values = new ContentValues();
        if ( null != history ) {
            values.put( DBHelper.CHAT_HISTORY, history );
        } else {
            values.putNull( DBHelper.CHAT_HISTORY );
        }
        values.put( DBHelper.LASTPLAY_TIME, new Date().getTime() );
        updateRow( context, DBHelper.TABLE_NAME_SUM, rowid, values );
    }

    private static void initDB( Context context )
    {
        if ( null == s_dbHelper ) {
            Assert.assertNotNull( context );
            s_dbHelper = new DBHelper( context );
            // force any upgrade
            s_dbHelper.getWritableDatabase().close();
        }
    }

    private static void updateRow( Context context, String table,
                                   long rowid, ContentValues values )
    {
        String selection = String.format( ROW_ID_FMT, rowid );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteDatabase db = s_dbHelper.getWritableDatabase();

            int result = db.update( table, values, selection, null );
            db.close();
            if ( 0 == result ) {
                DbgUtils.logf( "updateRow failed" );
            }
        }
    }

    private static int maxVISID( SQLiteDatabase db )
    {
        int result = 1;
        String query = String.format( "SELECT max(%s) FROM %s", DBHelper.VISID,
                                      DBHelper.TABLE_NAME_SUM );
        Cursor cursor = null;
        try {
            cursor = db.rawQuery( query, null );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = 1 + cursor.getInt( 0 );
            }
        } finally {
            if ( null != cursor ) {
                cursor.close();
            }
        }
        return result;
    }
    
    private static void notifyListeners( long rowid, GameChangeType change )
    {
        synchronized( s_listeners ) {
            Iterator<DBChangeListener> iter = s_listeners.iterator();
            while ( iter.hasNext() ) {
                iter.next().gameSaved( rowid, change );
            }
        }
    }

    // Trivial one-item cache.  Typically bytes are read three times
    // in a row, so this saves two DB accesses per game opened.  Could
    // use a HashMap, but then lots of half-K byte[] chunks would fail
    // to gc.  This is good enough.
    private static byte[] getCached( long rowid )
    {
        byte[] result = s_cachedRowID == rowid ? s_cachedBytes : null;
        return result;
    }

    private static void setCached( long rowid, byte[] bytes )
    {
        s_cachedRowID = rowid;
        s_cachedBytes = bytes;
    }
}

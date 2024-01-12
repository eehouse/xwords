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

import android.content.ContentValues;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
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

import org.eehouse.android.xw4.DBHelper.TABLE_NAMES;
import org.eehouse.android.xw4.DictUtils.DictLoc;
import org.eehouse.android.xw4.DictUtils.ON_SERVER;
import org.eehouse.android.xw4.DlgDelegate.DlgClickNotify.InviteMeans;
import org.eehouse.android.xw4.Utils.ISOCode;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.DictInfo;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Serializable;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;

public class DBUtils {
    private static final String TAG = DBUtils.class.getSimpleName();
    public static final int ROWID_NOTFOUND = -1;
    public static final int ROWIDS_ALL = -2;
    public static final int GROUPID_UNSPEC = -1;
    public static final String KEY_NEWGAMECOUNT = "DBUtils.newGameCount";

    // how many log rows to keep? (0 means off)
    private static final int LOGLIMIT = 0;

    private static final String DICTS_SEP = ",";

    private static final String ROW_ID = "rowid";
    private static final String ROW_ID_FMT = "rowid=%d";
    private static final String NAME_FMT = "%s='%s'";

    private static long s_cachedRowID = ROWID_NOTFOUND;
    private static byte[] s_cachedBytes = null;

    public static enum GameChangeType { GAME_CHANGED, GAME_CREATED,
                                        GAME_DELETED, GAME_MOVED,
    };

    public static interface DBChangeListener {
        public void gameSaved( Context context, long rowid,
                               GameChangeType change );
    }
    private static HashSet<DBChangeListener> s_listeners =
        new HashSet<>();

    public static interface StudyListListener {
        void onWordAdded( String word, ISOCode isoCode );
    }
    private static Set<StudyListListener> s_slListeners
        = new HashSet<>();

    private static SQLiteOpenHelper s_dbHelper = null;
    private static SQLiteDatabase s_db = null;

    public static class HistoryPair {
        private HistoryPair( String p_msg, int p_playerIndx, int p_ts )
        {
            msg = p_msg;
            playerIndx = p_playerIndx;
            ts = p_ts;
        }
        String msg;
        int playerIndx;
        int ts;
    }

    public static GameSummary getSummary( Context context,
                                          GameLock lock )
    {
        long startMS = System.currentTimeMillis();
        initDB( context );
        GameSummary summary = null;
        String[] columns = { ROW_ID,
                             DBHelper.NUM_MOVES, DBHelper.NUM_PLAYERS,
                             DBHelper.MISSINGPLYRS,
                             DBHelper.GAME_OVER, DBHelper.QUASHED, DBHelper.PLAYERS,
                             DBHelper.TURN, DBHelper.TURN_LOCAL, DBHelper.GIFLAGS,
                             DBHelper.CONTYPE, DBHelper.SERVERROLE,
                             DBHelper.ROOMNAME, DBHelper.RELAYID,
                             /*DBHelper.SMSPHONE,*/ DBHelper.SEED,
                             DBHelper.ISOCODE, DBHelper.GAMEID,
                             DBHelper.SCORES,
                             DBHelper.LASTPLAY_TIME, DBHelper.REMOTEDEVS,
                             DBHelper.LASTMOVE, DBHelper.NPACKETSPENDING,
                             DBHelper.EXTRAS, DBHelper.NEXTDUPTIMER,
                             DBHelper.CREATE_TIME, DBHelper.CAN_REMATCH,
        };
        String selection = String.format( ROW_ID_FMT, lock.getRowid() );

        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                summary = new GameSummary();
                summary.nMoves = cursor
                    .getInt( cursor.getColumnIndex(DBHelper.NUM_MOVES) );
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
                summary.turnIsLocal =
                    0 != cursor.getInt(cursor. getColumnIndex(DBHelper.TURN_LOCAL));
                summary.
                    setGiFlags( cursor.getInt(cursor.
                                              getColumnIndex(DBHelper.GIFLAGS))
                                );
                summary.gameID =
                    cursor.getInt(cursor.getColumnIndex(DBHelper.GAMEID) );

                String players = cursor.
                    getString(cursor.getColumnIndex( DBHelper.PLAYERS ));
                summary.readPlayers( context, players );

                // isoCode will be null when game first created
                summary.isoCode =
                    ISOCode.newIf( cursor.getString(cursor.getColumnIndex(DBHelper.ISOCODE)) );

                summary.modtime =
                    cursor.getLong(cursor.
                                   getColumnIndex(DBHelper.LASTPLAY_TIME));
                int tmp = cursor.getInt(cursor.
                                        getColumnIndex(DBHelper.GAME_OVER));
                summary.gameOver = tmp != 0;
                tmp = cursor.getInt(cursor.
                                        getColumnIndex(DBHelper.QUASHED));
                summary.quashed = tmp != 0;
                
                summary.lastMoveTime =
                    cursor.getInt(cursor.getColumnIndex(DBHelper.LASTMOVE));
                summary.dupTimerExpires =
                    cursor.getInt(cursor.getColumnIndex(DBHelper.NEXTDUPTIMER));
                summary.created = cursor
                    .getLong(cursor.getColumnIndex(DBHelper.CREATE_TIME));

                tmp = cursor.getInt( cursor.getColumnIndex( DBHelper.CAN_REMATCH ));
                summary.canRematch = 0 != tmp;

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
                        CommsConnType typ = iter.next();
                        switch ( typ ) {
                        case COMMS_CONN_RELAY:
                            // Can't do this: there are still some relay games
                            // on my devices anyway
                            // Assert.failDbg();
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
                                summary.setRemoteDevs( context, typ,
                                                       cursor.getString( col ) );
                            }
                            break;
                        }
                    }
                }

                col = cursor.getColumnIndex( DBHelper.SERVERROLE );
                tmp = cursor.getInt( col );
                summary.serverRole = DeviceRole.values()[tmp];
            }
            cursor.close();
        }

        if ( null == summary && lock.canWrite() ) {
            summary = GameUtils.summarize( context, lock );
        }
        long endMS = System.currentTimeMillis();

        // Might want to be cacheing this...
        long elapsed = endMS - startMS;
        if ( elapsed > 10 ) {
            Log.d( TAG, "getSummary(rowid=%d) => %s (took>10: %dms)",
                   lock.getRowid(), summary, elapsed );
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
            values.put( DBHelper.TURN_LOCAL, summary.turnIsLocal? 1 : 0 );
            values.put( DBHelper.GIFLAGS, summary.giflags() );
            values.put( DBHelper.PLAYERS,
                        summary.summarizePlayers() );
            Assert.assertTrueNR( null != summary.isoCode );
            values.put( DBHelper.ISOCODE, summary.isoCode.toString() );
            values.put( DBHelper.GAMEID, summary.gameID );
            values.put( DBHelper.GAME_OVER, summary.gameOver? 1 : 0 );
            values.put( DBHelper.QUASHED, summary.quashed? 1 : 0 );
            values.put( DBHelper.LASTMOVE, summary.lastMoveTime );
            values.put( DBHelper.NEXTDUPTIMER, summary.dupTimerExpires );
            values.put( DBHelper.CAN_REMATCH, summary.canRematch?1:0 );

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

            if ( null == summary ) {
                delete( TABLE_NAMES.SUM, selection );
            } else {
                long result = update( TABLE_NAMES.SUM, values, selection );
                Assert.assertTrue( result >= 0 );
            }
            notifyListeners( context, rowid, GameChangeType.GAME_CHANGED );
            invalGroupsCache();
        }

        if ( null != summary ) { // nag time may have changed
            NagTurnReceiver.setNagTimer( context );
        }
    } // saveSummary

    public static int countGamesUsingISOCode( Context context, ISOCode isoCode )
    {
        int result = 0;
        String[] columns = { DBHelper.ISOCODE };
        String selection = String.format( "%s = '%s'", columns[0],
                                          isoCode );
        // null for columns will return whole rows: bad
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );

            result = cursor.getCount();
            cursor.close();
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
        String[] columns = { DBHelper.ISOCODE };
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            result = cursor.getCount();
            cursor.close();
        }
        return result;
    }

    private static int countOpenGamesUsing( Context context,
                                            CommsConnType connTypWith )
    {
        return countOpenGamesUsing( context, connTypWith, null );
    }

    private static int countOpenGamesUsing( Context context,
                                            CommsConnType connTypWith,
                                            CommsConnType connTypWithout )
    {
        int result = 0;
        String[] columns = { DBHelper.CONTYPE };
        String selection = String.format( "%s = 0", DBHelper.GAME_OVER );

        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            int indx = cursor.getColumnIndex( DBHelper.CONTYPE );
            while ( cursor.moveToNext() ) {
                CommsConnTypeSet typs = new CommsConnTypeSet( cursor.getInt(indx) );
                if ( typs.contains( connTypWith ) ) {
                    if ( null == connTypWithout || ! typs.contains( connTypWithout ) ) {
                        ++result;
                    }
                }
            }

            cursor.close();
        }

        if ( 0 < result ) {
            Log.d( TAG, "countOpenGamesUsing(with: %s, without: %s) => %d",
                   connTypWith, connTypWithout, result );
        }
        return result;
    }

    public static int countOpenGamesUsingNBS( Context context )
    {
        int result = countOpenGamesUsing( context, CommsConnType.COMMS_CONN_SMS );
        // Log.d( TAG, "countOpenGamesUsingNBS() => %d", result );
        return result;
    }

    public static class SentInvite implements Serializable {
        InviteMeans mMeans;
        String mTarget;
        Date mTimestamp;

        public SentInvite( InviteMeans means, String target, Date ts )
        {
            mMeans = means;
            mTarget = target;
            mTimestamp = ts;
        }

        @Override
        public boolean equals(Object otherObj)
        {
            boolean result = false;
            if ( otherObj instanceof SentInvite ) {
                SentInvite other = (SentInvite)otherObj;
                result = mMeans == other.mMeans
                    && mTarget.equals(other.mTarget)
                    && mTimestamp.equals(other.mTimestamp);
            }
            return result;
        }
    }

    public static class SentInvitesInfo
        implements Serializable /* Serializable b/c passed as param to alerts */ {
        public long m_rowid;
        private ArrayList<SentInvite> mSents;
        private int m_cachedCount = 0;
        private boolean m_remotesRobots = false;

        @Override
        public boolean equals( Object other )
        {
            boolean result = null != other && other instanceof SentInvitesInfo;
            if ( result ) {
                SentInvitesInfo it = (SentInvitesInfo)other;
                if ( m_rowid == it.m_rowid
                     && it.mSents.size() == mSents.size()
                     && it.m_cachedCount == m_cachedCount ) {
                    for ( int ii = 0; result && ii < mSents.size(); ++ii ) {
                        result = it.mSents.get(ii).equals(mSents.get(ii));
                    }
                }
            }
            // Log.d( TAG, "equals() => %b", result );
            return result;
        }

        private SentInvitesInfo( long rowID )
        {
            m_rowid = rowID;
            mSents = new ArrayList<>();
        }

        private void addEntry( InviteMeans means, String target, Date ts )
        {
            mSents.add( new SentInvite( means, target, ts ) );
            m_cachedCount = -1;
        }

        public String getLastDev( InviteMeans means )
        {
            String result = null;
            for ( SentInvite si : mSents ) {
                if ( means == si.mMeans ) {
                    result = si.mTarget;
                    break;
                }
            }
            return result;
        }

        // There will be lots of duplicates, but we can't detect them all. BUT
        // if means and target are the same it's definitely a dup. So count
        // them all and return the largest number we have. 99% of the time we
        // care only that it's non-0.
        public int getMinPlayerCount()
        {
            if ( -1 == m_cachedCount ) {
                int count = mSents.size();
                Map<InviteMeans, Set<String>> hashes
                    = new HashMap<InviteMeans, Set<String>>();
                int fakeCount = 0; // make all null-targets count for one
                for ( int ii = 0; ii < count; ++ii ) {
                    SentInvite si = mSents.get(ii);
                    InviteMeans means = si.mMeans;
                    Set<String> devs;
                    if ( ! hashes.containsKey( means ) ) {
                        devs = new HashSet<>();
                        hashes.put( means, devs );
                    }
                    devs = hashes.get( means );
                    String target = si.mTarget;
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
            int count = mSents.size();
            if ( 0 == count ) {
                result = LocUtils.getString( context, R.string.no_invites );
            } else {
                List<String> strs = new ArrayList<>();
                for ( SentInvite si: mSents ) {
                    InviteMeans means = si.mMeans;
                    String target = si.mTarget;
                    String timestamp = si.mTimestamp.toString();
                    String msg = null;

                    switch ( means ) {
                    case SMS_DATA:
                        int fmt = R.string.invit_expl_sms_fmt;
                        msg = LocUtils.getString( context, fmt, target, timestamp );
                        break;
                    case SMS_USER:
                        fmt = R.string.invit_expl_usrsms_fmt;
                        msg = LocUtils.getString( context, fmt, timestamp );
                        break;
                    case BLUETOOTH:
                        String devName = BTUtils.nameForAddr( target );
                        msg = LocUtils.getString( context, R.string.invit_expl_bt_fmt,
                                                  devName, timestamp );
                        break;
                    case RELAY:
                        Assert.failDbg();
                        break;
                    case MQTT:
                        String player = XwJNI.kplr_nameForMqttDev( target );
                        if ( null != player ) {
                            msg = LocUtils.getString( context,
                                                      R.string.invit_expl_player_fmt,
                                                      player, timestamp );
                            break;
                        }
                        // else FALLTHRU
                    default:
                        msg = LocUtils.getString( context, R.string.invit_expl_notarget_fmt,
                                                  means.toString(), timestamp );

                    }
                    strs.add( msg );
                }

                result = TextUtils.join( "\n\n", strs );
            }
            return result;
        }

        public String getKPName( Context context )
        {
            String mqttID = null;
            for ( SentInvite si : mSents ) {
                InviteMeans means = si.mMeans;
                if ( means == InviteMeans.MQTT ) {
                    mqttID = si.mTarget;
                    break;
                }
            }

            String result = null;
            if ( null != mqttID ) {
                result = XwJNI.kplr_nameForMqttDev( mqttID );
            }
            Log.d( TAG, "getKPName() => %s", result );
            return result;
        }

        void setRemotesRobots() { m_remotesRobots = true; }
        boolean getRemotesRobots() { return m_remotesRobots; }
    }

    public static SentInvitesInfo getInvitesFor( Context context, long rowid )
    {
        SentInvitesInfo result = new SentInvitesInfo( rowid );

        String[] columns = { DBHelper.MEANS, DBHelper.TARGET,
                             " (strftime('%s', " + DBHelper.TIMESTAMP
                             + ") * 1000) AS " + DBHelper.TIMESTAMP,
        };
        String selection = String.format( "%s = %d", DBHelper.ROW, rowid );
        String orderBy = DBHelper.TIMESTAMP + " DESC";

        synchronized( s_dbHelper ) {
            Cursor cursor = DBHelper.query( s_db, TABLE_NAMES.INVITES, columns,
                                            selection, orderBy );
            if ( 0 < cursor.getCount() ) {
                int indxMns = cursor.getColumnIndex( DBHelper.MEANS );
                int indxTS = cursor.getColumnIndex( DBHelper.TIMESTAMP );
                int indxTrgt = cursor.getColumnIndex( DBHelper.TARGET );

                InviteMeans[] values = InviteMeans.values();
                while ( cursor.moveToNext() ) {
                    int ordinal = cursor.getInt( indxMns );
                    if ( ordinal < values.length ) {
                        InviteMeans means = values[ordinal];
                        Date ts = new Date(cursor.getLong(indxTS));
                        String target = cursor.getString( indxTrgt );
                        result.addEntry( means, target, ts );
                    }
                }
            }
            cursor.close();
        }

        return result;
    }

    public static void recordInviteSent( Context context, long rowid,
                                         InviteMeans means, String target,
                                         boolean dropDupes )
    {
        if ( BuildConfig.NON_RELEASE ) {
            switch ( means ) {
            case EMAIL:
            case NFC:
            case CLIPBOARD:
            case WIFIDIRECT:
            case SMS_USER:
            case QRCODE:
            case MQTT:
            case SMS_DATA:
            case BLUETOOTH:
                break;
            case RELAY:
            default:
                Assert.failDbg();
            }
        }

        String dropTest = null;
        if ( dropDupes ) {
            dropTest = String.format( "%s = %d AND %s = %d",
                                      DBHelper.ROW, rowid,
                                      DBHelper.MEANS, means.ordinal() );
            if ( null != target ) {
                dropTest += String.format( " AND %s = '%s'",
                                           DBHelper.TARGET, target );
            } else {
                // If I'm seeing this, need to check above if a "target is
                // null" test is needed to avoid nuking unintentinally.
                Assert.failDbg();
            }
        }

        ContentValues values = new ContentValues();
        values.put( DBHelper.ROW, rowid );
        values.put( DBHelper.MEANS, means.ordinal() );
        if ( null != target ) {
            values.put( DBHelper.TARGET, target );
        }

        initDB( context );
        synchronized( s_dbHelper ) {
            if ( null != dropTest ) {
                delete( TABLE_NAMES.INVITES, dropTest );
            }
            insert( TABLE_NAMES.INVITES, values );
        }
    }

    private static void setSummaryInt( long rowid, String column, int value )
    {
        ContentValues values = new ContentValues();
        values.put( column, value );
        updateRow( null, TABLE_NAMES.SUM, rowid, values );
    }

    public static void setMsgFlags( Context context, long rowid, int flags )
    {
        setSummaryInt( rowid, DBHelper.HASMSGS, flags );
        notifyListeners( context, rowid, GameChangeType.GAME_CHANGED );
    }

    public static void setExpanded( long rowid, boolean expanded )
    {
        setSummaryInt( rowid, DBHelper.CONTRACTED, expanded?0:1 );
    }

    private static int getSummaryInt( Context context, long rowid, String column,
                                      int dflt )
    {
        int result = dflt;
        String selection = String.format( ROW_ID_FMT, rowid );
        String[] columns = { column };
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result =
                    cursor.getInt( cursor.getColumnIndex(column));
            }
            cursor.close();
        }
        return result;
    }

    public static int getMsgFlags( Context context, long rowid )
    {
        return getSummaryInt( context, rowid, DBHelper.HASMSGS,
                              GameSummary.MSG_FLAGS_NONE );
    }

    public static boolean getExpanded( Context context, long rowid )
    {
        return 0 == getSummaryInt( context, rowid, DBHelper.CONTRACTED, 0 );
    }

    public static boolean gameOver( Context context, long rowid )
    {
        return 0 != getSummaryInt( context, rowid, DBHelper.GAME_OVER, 0 );
    }

    public static void saveThumbnail( Context context, GameLock lock,
                                      Bitmap thumb )
    {
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

            long result = update( TABLE_NAMES.SUM, values, selection );
            Assert.assertTrue( result >= 0 );


            notifyListeners( context, rowid, GameChangeType.GAME_CHANGED );
        }
    }

    public static void clearThumbnails( Context context )
    {
        ContentValues values = new ContentValues();
        values.putNull( DBHelper.THUMBNAIL );
        initDB( context );
        synchronized( s_dbHelper ) {
            long result = update( TABLE_NAMES.SUM, values, null );

            notifyListeners( context, ROWIDS_ALL, GameChangeType.GAME_CHANGED );
        }
    }

    public static HashMap<Long,CommsConnTypeSet>
        getGamesWithSendsPending( Context context )
    {
        HashMap<Long, CommsConnTypeSet> result = new HashMap<>();
        String[] columns = { ROW_ID, DBHelper.CONTYPE };
        String selection = String.format( "%s != %d AND %s > 0 AND %s != %d",
                                          DBHelper.SERVERROLE,
                                          DeviceRole.SERVER_STANDALONE.ordinal(),
                                          DBHelper.NPACKETSPENDING,
                                          DBHelper.GROUPID, getArchiveGroup( context ) );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
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
        }
        return result;
    }

    public static int getGameCountUsing( Context context, CommsConnType typ )
    {
        int result = 0;
        String[] columns = { DBHelper.CONTYPE };
        String selection = String.format( "%s = 0", DBHelper.GAME_OVER );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            int indx = cursor.getColumnIndex( DBHelper.CONTYPE );
            while ( cursor.moveToNext() ) {
                CommsConnTypeSet typs = new CommsConnTypeSet( cursor.getInt(indx) );
                if ( typs.contains( typ ) ) {
                    ++result;
                }
            }
            cursor.close();
        }
        return result;
    }

    public static long[] getRowIDsFor( Context context, int gameID )
    {
        long[] result;
        String[] columns = { ROW_ID };
        String selection = String.format( DBHelper.GAMEID + "=%d", gameID );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            result = new long[cursor.getCount()];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                result[ii] = cursor.getLong( cursor.getColumnIndex(ROW_ID) );
            }
            cursor.close();
        }
        if ( 1 != result.length ) {
            Log.d( TAG, "getRowIDsFor(gameID=%X)=>length %d array", gameID,
                   result.length );
        }
        return result;
    }

    static Map<Long, Integer> getRowIDsAndChannels( Context context, int gameID )
    {
        Map<Long, Integer> result = new HashMap<>();
        String[] columns = { ROW_ID, DBHelper.GIFLAGS };
        String selection = String.format( DBHelper.GAMEID + "=%d", gameID );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            while ( cursor.moveToNext() ) {
                int flags = cursor.getInt( cursor.getColumnIndex( DBHelper.GIFLAGS ) );
                int forceChannel = (flags >> GameSummary.FORCE_CHANNEL_OFFSET)
                    & GameSummary.FORCE_CHANNEL_MASK;
                long rowid = cursor.getLong( cursor.getColumnIndex( ROW_ID ) );
                result.put( rowid, forceChannel );
                // Log.i( TAG, "getRowIDsAndChannels(): added %d => %d",
                //        rowid, forceChannel );
            }
            cursor.close();
        }
        return result;
    }

    public static boolean haveWithRowID( Context context, long rowid )
    {
        boolean result = false;
        String[] columns = { ROW_ID };
        String selection = String.format( ROW_ID + "=%d", rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            Assert.assertTrue( 1 >= cursor.getCount() );
            result = 1 == cursor.getCount();
            cursor.close();
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
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            while ( cursor.moveToNext() ) {
                int col = cursor.getColumnIndex( DBHelper.GAMEID );
                int gameID = cursor.getInt( col );
                col = cursor.getColumnIndex( DBHelper.REMOTEDEVS );
                String devs = cursor.getString( col );
                Log.i( TAG, "gameid %d has remote[s] %s", gameID, devs );

                if ( null != devs && 0 < devs.length() ) {
                    for ( String dev : TextUtils.split( devs, "\n" ) ) {
                        set = map.get( dev );
                        if ( null == set ) {
                            set = new HashSet<>();
                            map.put( dev, set );
                        }
                        set.add( new Integer(gameID) );
                    }
                }
            }
            cursor.close();
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

    public static GameLock saveNewGame( Context context, byte[] bytes,
                                        long groupID, String name )
    {
        Assert.assertTrue( GROUPID_UNSPEC != groupID );
        GameLock lock = null;

        ContentValues values = new ContentValues();
        values.put( DBHelper.SNAPSHOT, bytes );

        long timestamp = new Date().getTime(); // milliseconds since epoch
        values.put( DBHelper.CREATE_TIME, timestamp );
        values.put( DBHelper.LASTPLAY_TIME, timestamp );
        values.put( DBHelper.GROUPID, groupID );
        if ( null != name ) {
            values.put( DBHelper.GAME_NAME, name );
        }

        invalGroupsCache();  // do first in case any listener has cached data

        initDB( context );
        synchronized( s_dbHelper ) {
            values.put( DBHelper.VISID, maxVISID( s_db ) );

            long rowid = insert( TABLE_NAMES.SUM, values );

            setCached( rowid, null ); // force reread

            lock = GameLock.tryLock( rowid );
            Assert.assertNotNull( lock );
            notifyListeners( context, rowid, GameChangeType.GAME_CREATED );
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

        updateRow( context, TABLE_NAMES.SUM, rowid, values );

        setCached( rowid, null ); // force reread
        if ( ROWID_NOTFOUND != rowid ) {      // Means new game?
            notifyListeners( context, rowid, GameChangeType.GAME_CHANGED );
        }
        invalGroupsCache();
        return rowid;
    }

    public static byte[] loadGame( Context context, GameLock lock )
    {
        byte[] result = null;
        long rowid = lock.getRowid();
        Assert.assertTrue( ROWID_NOTFOUND != rowid );
        if ( Quarantine.safeToOpen( rowid ) ) {
            result = getCached( rowid );
            if ( null == result ) {
                String[] columns = { DBHelper.SNAPSHOT };
                String selection = String.format( ROW_ID_FMT, rowid );
                initDB( context );
                synchronized( s_dbHelper ) {
                    Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
                    if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                        result = cursor.getBlob( cursor
                                                 .getColumnIndex(DBHelper.SNAPSHOT));
                    } else {
                        Log.e( TAG, "loadGame: none for rowid=%d", rowid );
                    }
                    cursor.close();
                }
                setCached( rowid, result );
            }
        }
        return result;
    }

    public static void deleteGame( Context context, long rowid )
    {
        try ( GameLock lock = GameLock.lock( rowid, 300 ) ) {
            if ( null != lock ) {
                deleteGame( context, lock );
            } else {
                Log.e( TAG, "deleteGame: unable to lock rowid %d", rowid );
                Assert.failDbg();
            }
        }
    }

    public static void deleteGame( Context context, GameLock lock )
    {
        Assert.assertTrue( lock.canWrite() );
        long rowid = lock.getRowid();
        String selSummaries = String.format( ROW_ID_FMT, rowid );
        String selInvites = String.format( "%s=%d", DBHelper.ROW, rowid );

        initDB( context );
        synchronized( s_dbHelper ) {
            delete( TABLE_NAMES.SUM, selSummaries );

            // Delete invitations too
            delete( TABLE_NAMES.INVITES, selInvites );

            // Delete chats too -- same sel as for invites
            delete( TABLE_NAMES.CHAT, selInvites );

            deleteCurChatsSync( s_db, rowid );

        }
        notifyListeners( context, rowid, GameChangeType.GAME_DELETED );
        invalGroupsCache();
    }

    public static int getVisID( Context context, long rowid )
    {
        int result = ROWID_NOTFOUND;
        String[] columns = { DBHelper.VISID };
        String selection = String.format( ROW_ID_FMT, rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getInt( cursor
                                        .getColumnIndex(DBHelper.VISID));
            }
            cursor.close();
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
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = cursor.getString( cursor
                                           .getColumnIndex(DBHelper.GAME_NAME));
            }
            cursor.close();
        }

        return result;
    }

    public static void setName( Context context, long rowid, String name )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.GAME_NAME, name );
        updateRow( context, TABLE_NAMES.SUM, rowid, values );
    }

    private static HistoryPair[] convertChatString( Context context, long rowid,
                                                    boolean[] playersLocal )
    {
        HistoryPair[] result = null;
        String oldHistory = getChatHistoryStr( context, rowid );
        if ( null != oldHistory ) {
            Log.d( TAG, "convertChatString(): got string: %s", oldHistory );

            ArrayList<ContentValues> valuess = new ArrayList<>();
            ArrayList<HistoryPair> pairs = new ArrayList<>();
            String localPrefix = LocUtils.getString( context, R.string.chat_local_id );
            String rmtPrefix = LocUtils.getString( context, R.string.chat_other_id );
            Log.d( TAG, "convertChatString(): prefixes: \"%s\" and \"%s\"", localPrefix, rmtPrefix );
            String[] msgs = oldHistory.split( "\n" );
            Log.d( TAG, "convertChatString(): split into %d", msgs.length );
            int localPlayerIndx = -1;
            int remotePlayerIndx = -1;
            for ( int ii = playersLocal.length - 1; ii >= 0; --ii ) {
                if ( playersLocal[ii] ) {
                    localPlayerIndx = ii;
                } else {
                    remotePlayerIndx = ii;
                }
            }
            for ( String msg : msgs ) {
                Log.d( TAG, "convertChatString(): msg: %s", msg );
                int indx = -1;
                String prefix = null;
                if ( msg.startsWith( localPrefix ) ) {
                    Log.d( TAG, "convertChatString(): msg: %s starts with %s", msg, localPrefix );
                    prefix = localPrefix;
                    indx = localPlayerIndx;
                } else if ( msg.startsWith( rmtPrefix ) ) {
                    Log.d( TAG, "convertChatString(): msg: %s starts with %s", msg, rmtPrefix );
                    prefix = rmtPrefix;
                    indx = remotePlayerIndx;
                } else {
                    Log.d( TAG, "convertChatString(): msg: %s starts with neither", msg );
                }
                if ( -1 != indx ) {
                    Log.d( TAG, "convertChatString(): removing substring %s; was: %s", prefix, msg );
                    msg = msg.substring( prefix.length(), msg.length() );
                    Log.d( TAG, "convertChatString(): removED substring; now %s", msg );
                    valuess.add( cvForChat( rowid, msg, indx, 0 ) );

                    HistoryPair pair = new HistoryPair( msg, indx, 0 );
                    pairs.add( pair );
                }
            }
            result = pairs.toArray( new HistoryPair[pairs.size()] );

            appendChatHistory( context, valuess );
            // clearChatHistoryString( context, rowid );
        }
        return result;
    }

    public static HistoryPair[] getChatHistory( Context context, long rowid,
                                                boolean[] playersLocal )
    {
        HistoryPair[] result = null;
        String[] columns = { DBHelper.SENDER, DBHelper.MESSAGE, DBHelper.CHATTIME };
        String selection = String.format( "%s=%d", DBHelper.ROW, rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.CHAT, columns, selection );
            if ( 0 < cursor.getCount() ) {
                result = new HistoryPair[cursor.getCount()];
                int msgIndex = cursor.getColumnIndex( DBHelper.MESSAGE );
                int plyrIndex = cursor.getColumnIndex( DBHelper.SENDER );
                int tsIndex = cursor.getColumnIndex( DBHelper.CHATTIME );
                for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                    String msg = cursor.getString( msgIndex );
                    int plyr = cursor.getInt( plyrIndex );
                    int ts = cursor.getInt( tsIndex );
                    HistoryPair pair = new HistoryPair( msg, plyr, ts );
                    result[ii] = pair;
                }
            }
            cursor.close();
        }

        if ( null == result ) {
            result = convertChatString( context, rowid, playersLocal );
        }
        return result;
    }

    private static String formatCurChatKey( long rowid ) {
        return formatCurChatKey( rowid, -1 );
    }

    private static String formatCurChatKey( long rowid, int player ) {
        String playerMatch = 0 <= player ? String.format( "%d", player ) : "%";
        String result = String.format("<<chat/%d/%s>>", rowid, playerMatch );
        return result;
    }

    public static String getCurChat( Context context, long rowid, int player,
                                     int[] startAndEndOut ) {
        String result = null;
        String key = formatCurChatKey( rowid, player );
        String all = getStringFor( context, key, "" );
        String[] parts = TextUtils.split( all, ":" );
        if ( 3 <= parts.length ) {
            result = all.substring( 2 + parts[0].length() + parts[1].length() );
            startAndEndOut[0] = Math.min( result.length(),
                                          Integer.parseInt( parts[0] ) );
            startAndEndOut[1] = Math.min( result.length(),
                                          Integer.parseInt( parts[1] ) );
        }
        Log.d( TAG, "getCurChat(): => %s [%d,%d]", result,
               startAndEndOut[0], startAndEndOut[1] );
        return result;
    }

    public static void setCurChat( Context context, long rowid, int player,
                                   String text, int start, int end ) {
        String key = formatCurChatKey( rowid, player );
        text = String.format( "%d:%d:%s", start, end, text );
        setStringFor( context, key, text );
    }

    private static void deleteCurChatsSync( SQLiteDatabase db, long rowid ) {
        String like = formatCurChatKey( rowid );
        delStringsLikeSync( db, like );
    }

    public static class NeedsNagInfo {
        public long m_rowid;
        public long m_nextNag;
        public long m_lastMoveMillis;
        private boolean m_isSolo;

        public NeedsNagInfo( long rowid, long nextNag, long lastMove,
                             DeviceRole role ) {
            m_rowid = rowid;
            m_nextNag = nextNag;
            m_lastMoveMillis = 1000 * lastMove;
            m_isSolo = DeviceRole.SERVER_STANDALONE == role;
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
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
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
                    DeviceRole role =
                        DeviceRole.values()[cursor.getInt( roleIndex )];
                    result[ii] = new NeedsNagInfo( rowid, nextNag, lastMove, role );
                }
            }

            cursor.close();
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
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( cursor.moveToNext() ) {
                result = cursor.getLong( cursor.getColumnIndex( "min" ) );
            }
            cursor.close();
        }
        return result;
    }

    public static void updateNeedNagging( Context context, NeedsNagInfo[] needNagging )
    {
        String updateQuery = "update %s set %s = ? "
            + " WHERE %s = ? ";
        updateQuery = String.format( updateQuery, DBHelper.TABLE_NAMES.SUM,
                                     DBHelper.NEXTNAG, ROW_ID );

        initDB( context );
        synchronized( s_dbHelper ) {
            SQLiteStatement updateStmt = s_db.compileStatement( updateQuery );

            for ( NeedsNagInfo info : needNagging ) {
                updateStmt.bindLong( 1, info.m_nextNag );
                updateStmt.bindLong( 2, info.m_rowid );
                updateStmt.execute();
            }
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

        @Override
        public String toString()
        {
            return String.format( "GameGroupInfo: {name: %s}", m_name );
        }
    }

    private static Map<Long,GameGroupInfo> s_groupsCache = null;

    private static void invalGroupsCache()
    {
        s_groupsCache = null;
    }

    public static Bitmap getThumbnail( Context context, long rowid )
    {
        Bitmap thumb = null;
        byte[] data = null;
        String[] columns = { DBHelper.THUMBNAIL };
        String selection = String.format( ROW_ID_FMT, rowid );

        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                data = cursor.getBlob( cursor.
                                       getColumnIndex(DBHelper.THUMBNAIL));
            }
            cursor.close();
        }

        if ( null != data ) {
            thumb = BitmapFactory.decodeByteArray( data, 0, data.length );
        }
        return thumb;
    }

    private static HashMap<Long, Integer> getGameCounts( SQLiteDatabase db )
    {
        HashMap<Long, Integer> result = new HashMap<>();
        String query = "SELECT %s, count(%s) as cnt FROM %s GROUP BY %s";
        query = String.format( query, DBHelper.GROUPID, DBHelper.GROUPID,
                               DBHelper.TABLE_NAMES.SUM, DBHelper.GROUPID );

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
    protected static Map<Long,GameGroupInfo> getGroups( Context context )
    {
        Map<Long,GameGroupInfo> result = s_groupsCache;
        if ( null == result ) {
            result = new HashMap<>();

            // Select all groups.  For each group get the number of games in
            // that group.  There should be a way to do that with one query
            // but I can't figure it out.

            String query = "SELECT rowid, groupname as groups_groupname, "
                + " groups.expanded as groups_expanded FROM groups";

            initDB( context );
            synchronized( s_dbHelper ) {

                HashMap<Long, Integer> map = getGameCounts( s_db );

                Cursor cursor = s_db.rawQuery( query, null );
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
                    readTurnInfo( s_db, groupID, ggi );
                }

            }
            s_groupsCache = result;
        }
        // Log.d( TAG, "getGroups() => %s", result );
        return result;
    } // getGroups

    private static void readTurnInfo( SQLiteDatabase db, long groupID,
                                      GameGroupInfo ggi )
    {
        String[] columns = { DBHelper.LASTMOVE, DBHelper.GIFLAGS,
                             DBHelper.TURN };
        String orderBy = DBHelper.LASTMOVE;
        String selection = String.format( "%s=%d", DBHelper.GROUPID, groupID );
        Cursor cursor = DBHelper.query( db, TABLE_NAMES.SUM, columns,
                                        selection, orderBy );

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
            Cursor cursor = query( TABLE_NAMES.SUM, columns, null );
            result = cursor.getCount();
            cursor.close();
        }
        return result;
    }

    // ORDER BY clause that governs display of games in main GamesList view
    private static final String s_getGroupGamesOrderBy =
        TextUtils.join(",", new String[] {
                // Ended games at bottom
                DBHelper.GAME_OVER,
                // games with unread chat messages at top
                "(" + DBHelper.HASMSGS + " & " + GameSummary.MSG_FLAGS_CHAT + ") IS NOT 0 DESC",
                // Games not yet connected at top
                DBHelper.TURN + " is -1 DESC",
                // Games where it's a local player's turn at top
                DBHelper.TURN_LOCAL + " DESC",
                // finally, sort by timestamp of last-made move
                DBHelper.LASTMOVE,
            });

    public static long[] getGroupGames( Context context, long groupID )
    {
        long[] result = {};
        initDB( context );
        String[] columns = { ROW_ID, DBHelper.HASMSGS };
        String selection = String.format( "%s=%d", DBHelper.GROUPID, groupID );
        synchronized( s_dbHelper ) {
            Cursor cursor = s_db.query( TABLE_NAMES.SUM.toString(), columns,
                                        selection, // selection
                                        null, // args
                                        null, // groupBy
                                        null, // having
                                        s_getGroupGamesOrderBy
                                        );
            int index = cursor.getColumnIndex( ROW_ID );
            result = new long[ cursor.getCount() ];
            for ( int ii = 0; cursor.moveToNext(); ++ii ) {
                long rowid = cursor.getInt( index );
                result[ii] = rowid;
            }
            cursor.close();
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
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( cursor.moveToNext() ) {
                int index = cursor.getColumnIndex( DBHelper.GROUPID );
                result = cursor.getLong( index );
            }
            cursor.close();
        }
        return result;
    }

    public static long getAnyGroup( Context context )
    {
        long result = GROUPID_UNSPEC;
        Map<Long,GameGroupInfo> groups = getGroups( context );
        Iterator<Long> iter = groups.keySet().iterator();
        if ( iter.hasNext() ) {
            result = iter.next();
        }
        Assert.assertTrue( GROUPID_UNSPEC != result );
        return result;
    }

    public static long getGroup( Context context, String name )
    {
        long result;
        initDB( context );
        synchronized( s_dbHelper ) {
            result = getGroupImpl( name );
        }
        return result;
    }

    private static long getGroupImpl( String name )
    {
        long result = GROUPID_UNSPEC;
        String[] columns = { ROW_ID };
        String selection = DBHelper.GROUPNAME + " = ?";
        String[] selArgs = { name };

        Cursor cursor = s_db.query( TABLE_NAMES.GROUPS.toString(), columns,
                                    selection, selArgs,
                                    null, // groupBy
                                    null, // having
                                    null // orderby
                                    );
        if ( cursor.moveToNext() ) {
            result = cursor.getLong( cursor.getColumnIndex( ROW_ID ) );
        }
        cursor.close();

        Log.d( TAG, "getGroupImpl(%s) => %d", name, result );
        return result;
    }

    private static long addGroupImpl( String name )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.GROUPNAME, name );
        values.put( DBHelper.EXPANDED, 1 );

        long rowid = insert( TABLE_NAMES.GROUPS, values );
        invalGroupsCache();

        return rowid;
    }

    public static long addGroup( Context context, String name )
    {
        long rowid = GROUPID_UNSPEC;
        if ( null != name && 0 < name.length() ) {
            if ( null == getGroups( context ).get( name ) ) {
                synchronized( s_dbHelper ) {
                    rowid = addGroupImpl( name );
                }
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
            delete( TABLE_NAMES.SUM, selectionGames );
            delete( TABLE_NAMES.GROUPS, selectionGroups );
        }
        invalGroupsCache();
    }

    public static void setGroupName( Context context, long groupid,
                                     String name )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.GROUPNAME, name );
        updateRow( context, TABLE_NAMES.GROUPS, groupid, values );
        invalGroupsCache();
    }

    public static void setGroupExpanded( Context context, long groupid,
                                         boolean expanded )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.EXPANDED, expanded? 1 : 0 );
        updateRow( context, TABLE_NAMES.GROUPS, groupid, values );
        invalGroupsCache();
    }

    public static long getArchiveGroup( Context context )
    {
        String archiveName = LocUtils
            .getString( context, R.string.group_name_archive );
        long archiveGroup = getGroup( context, archiveName );
        if ( DBUtils.GROUPID_UNSPEC == archiveGroup ) {
            archiveGroup = DBUtils.addGroup( context, archiveName );
        }
        return archiveGroup;
    }

    // Change group id of a game
    public static void moveGame( Context context, long rowid, long groupID )
    {
        Assert.assertTrue( GROUPID_UNSPEC != groupID );
        ContentValues values = new ContentValues();
        values.put( DBHelper.GROUPID, groupID );
        updateRow( context, TABLE_NAMES.SUM, rowid, values );
        invalGroupsCache();
        notifyListeners( context, rowid, GameChangeType.GAME_MOVED );
    }

    public static Map<Long, Integer> getDupModeGames( Context context )
    {
        return getDupModeGames( context, ROWID_NOTFOUND );
    }

    // Return all games whose DUP_MODE_MASK bit is set. Return also (as map
    // value) the nextTimer value, which will be negative if the game's
    // paused. As a bit of a hack, set it to 0 if the local player has already
    // committed his turn so caller (DupeModeTimer) will know not to show a
    // notification.
    public static Map<Long, Integer> getDupModeGames( Context context, long rowid )
    {
        // select giflags from summaries where 0x100 & giflags != 0;
        Map<Long, Integer> result = new HashMap<>();
        String[] columns = { ROW_ID, DBHelper.NEXTDUPTIMER, DBHelper.TURN_LOCAL };
        String selection = String.format( "%d & %s != 0",
                                          GameSummary.DUP_MODE_MASK,
                                          DBHelper.GIFLAGS );
        if ( ROWID_NOTFOUND != rowid ) {
            selection += String.format( " AND %s = %d", ROW_ID, rowid );
        }

        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            int count = cursor.getCount();
            int indxRowid = cursor.getColumnIndex( ROW_ID );
            int indxTimer = cursor.getColumnIndex( DBHelper.NEXTDUPTIMER );
            int indxIsLocal = cursor.getColumnIndex( DBHelper.TURN_LOCAL );
            while ( cursor.moveToNext() ) {
                boolean isLocal = 0 != cursor.getInt( indxIsLocal );
                int timer = isLocal ? cursor.getInt( indxTimer ) : 0;
                result.put( cursor.getLong( indxRowid ), timer );
            }
            cursor.close();
        }
        Log.d( TAG, "getDupModeGames(%d) => %s", rowid, result );
        return result;
    }

    private static String getChatHistoryStr( Context context, long rowid )
    {
        String result = null;
        String[] columns = { DBHelper.CHAT_HISTORY };
        String selection = String.format( ROW_ID_FMT, rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.SUM, columns, selection );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result =
                    cursor.getString( cursor
                                      .getColumnIndex(DBHelper
                                                      .CHAT_HISTORY));
            }
            cursor.close();
        }
        return result;
    }

    private static void appendChatHistory( Context context,
                                           ArrayList<ContentValues> valuess )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            for ( ContentValues values : valuess ) {
                insert( TABLE_NAMES.CHAT, values );
            }
        }
    }

    private static ContentValues cvForChat( long rowid, String msg, int plyr, long tsSeconds )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.ROW, rowid );
        values.put( DBHelper.MESSAGE, msg );
        values.put( DBHelper.SENDER, plyr );
        values.put( DBHelper.CHATTIME, tsSeconds );
        return values;
    }

    public static void appendChatHistory( Context context, long rowid,
                                          String msg, int fromPlayer,
                                          long tsSeconds )
    {
        Assert.assertNotNull( msg );
        Assert.assertFalse( -1 == fromPlayer );
        ArrayList<ContentValues> valuess = new ArrayList<>();
        valuess.add( cvForChat( rowid, msg, fromPlayer, tsSeconds ) );
        appendChatHistory( context, valuess );
        Log.i( TAG, "appendChatHistory: inserted \"%s\" from player %d",
               msg, fromPlayer );
    } // appendChatHistory

    public static void clearChatHistory( Context context, long rowid )
    {
        String selection = String.format( "%s = %d", DBHelper.ROW, rowid );
        initDB( context );
        synchronized( s_dbHelper ) {
            delete( TABLE_NAMES.CHAT, selection );

            // for now, remove any old-format history too. Later when it's
            // removed once converted (after that process is completely
            // debugged), this can be removed.
            ContentValues values = new ContentValues();
            values.putNull( DBHelper.CHAT_HISTORY );
            updateRowImpl( TABLE_NAMES.SUM, rowid, values );
        }
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

    protected static void addStudyListChangedListener( StudyListListener lnr )
    {
        synchronized( s_slListeners ) {
            s_slListeners.add( lnr );
        }
    }

    protected static void removeStudyListChangedListener( StudyListListener lnr )
    {
        synchronized( s_slListeners ) {
            s_slListeners.remove( lnr );
        }
    }

    public static boolean copyStream( OutputStream fos, InputStream fis )
    {
        boolean success = false;
        byte[] buf = new byte[1024*8];
        try {
            long totalBytes = 0;
            for ( ; ; ) {
                int nRead = fis.read( buf );
                if ( 0 >= nRead ) {
                    break;
                }
                fos.write( buf, 0, nRead );
                totalBytes += nRead;
            }
            success = true;
            Log.d( TAG, "copyFileStream(): copied %s to %s (%d bytes)",
                   fis, fos, totalBytes );
        } catch( java.io.IOException ioe ) {
            Log.ex( TAG, ioe );
        }
        return success;
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
            int result = update( TABLE_NAMES.DICTINFO, values, selection );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, name );
                long rowid = insert( TABLE_NAMES.DICTINFO, values );
                Assert.assertTrue( rowid > 0 || !BuildConfig.DEBUG );
            }
        }
    }

    public static DictInfo dictsGetInfo( Context context, String name )
    {
        DictInfo result = null;
        String[] columns = { DBHelper.ISOCODE,
                             DBHelper.LANGNAME,
                             DBHelper.WORDCOUNT,
                             DBHelper.MD5SUM,
                             DBHelper.FULLSUM,
                             DBHelper.ON_SERVER,
                             /*DBHelper.LOC*/ };
        String selection = String.format( NAME_FMT, DBHelper.DICTNAME, name );
        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.DICTINFO, columns, selection );
            if ( 1 == cursor.getCount() && cursor.moveToFirst() ) {
                result = new DictInfo();
                result.name = name;
                result.isoCodeStr
                    = cursor.getString( cursor.getColumnIndex(DBHelper.ISOCODE));
                result.wordCount =
                    cursor.getInt( cursor.getColumnIndex(DBHelper.WORDCOUNT));
                result.md5Sum =
                    cursor.getString( cursor.getColumnIndex(DBHelper.MD5SUM));
                result.fullSum =
                    cursor.getString( cursor.getColumnIndex(DBHelper.FULLSUM));
                result.langName =
                    cursor.getString( cursor.getColumnIndex(DBHelper.LANGNAME));

                int onServer = cursor.getInt( cursor.getColumnIndex(DBHelper.ON_SERVER) );
                result.onServer = ON_SERVER.values()[onServer];

                // int loc = cursor.getInt(cursor.getColumnIndex(DBHelper.LOC));
                // Log.d( TAG, "dictsGetInfo(): read sum %s/loc %d for %s", result.md5Sum,
                //        loc, name );
             }
            cursor.close();
        }

        if ( null != result ) {
            if ( null == result.fullSum ) { // force generation
                result = null;
            }
        }

        // Log.d( TAG, "dictsGetInfo(%s) => %s", name, result );
        return result;
    }

    public static void dictsSetInfo( Context context, DictUtils.DictAndLoc dal,
                                     DictInfo info )
    {
        Assert.assertTrueNR( null != info.isoCode() );

        String selection =
            String.format( NAME_FMT, DBHelper.DICTNAME, dal.name );
        ContentValues values = new ContentValues();

        values.put( DBHelper.ISOCODE, info.isoCode().toString() );
        values.put( DBHelper.LANGNAME, info.langName );
        values.put( DBHelper.WORDCOUNT, info.wordCount );
        values.put( DBHelper.MD5SUM, info.md5Sum );
        values.put( DBHelper.FULLSUM, info.fullSum );
        values.put( DBHelper.LOCATION, dal.loc.ordinal() );

        initDB( context );
        synchronized( s_dbHelper ) {
            int result = update( TABLE_NAMES.DICTINFO, values, selection );
            if ( 0 == result ) {
                values.put( DBHelper.DICTNAME, dal.name );
                long rowid = insert( TABLE_NAMES.DICTINFO, values );
                Assert.assertTrueNR( 0 < rowid );
            }
        }
    }

    public static void dictsMoveInfo( Context context, String name,
                                      DictLoc fromLoc, DictLoc toLoc )
    {
        String selection =
            String.format( DBHelper.DICTNAME + "='%s' AND " + DBHelper.LOCATION + "=%d",
                           name, toLoc.ordinal() );
        ContentValues values = new ContentValues();
        values.put( DBHelper.LOCATION, toLoc.ordinal() );

        initDB( context );
        synchronized( s_dbHelper ) {
            update( TABLE_NAMES.DICTINFO, values, selection );
            update( TABLE_NAMES.DICTBROWSE, values, selection );
        }
    }

    public static void dictsRemoveInfo( Context context, String name )
    {
        String selection = String.format( "%s=?", DBHelper.DICTNAME );
        String[] args = { name };

        initDB( context );
        synchronized( s_dbHelper ) {
            int removed = delete( TABLE_NAMES.DICTINFO, selection, args );
            // Log.d( TAG, "removed %d rows from %s", removed, DBHelper.TABLE_NAME_DICTINFO );
            removed = delete( TABLE_NAMES.DICTBROWSE, selection, args );
            // Log.d( TAG, "removed %d rows from %s", removed, DBHelper.TABLE_NAME_DICTBROWSE );
        }
    }

    public static void updateServed( Context context, DictUtils.DictAndLoc dal,
                                     boolean served )
    {
        // For some reason, loc is sometimes wrong. So just flag the thing
        // wherever it is.
        String selection =
            String.format( DBHelper.DICTNAME + "='%s' ", dal.name );
        ContentValues values = new ContentValues();
        ON_SERVER onServer = served ? ON_SERVER.YES : ON_SERVER.NO;
        values.put( DBHelper.ON_SERVER, onServer.ordinal() );

        initDB( context );
        synchronized( s_dbHelper ) {
            int count = update( TABLE_NAMES.DICTINFO, values, selection );
            Log.d( TAG, "update(%s) => %d rows affected", selection, count );
            Assert.assertTrueNR( count > 0 );
        }
    }

    public static void addToStudyList( Context context, String word,
                                       ISOCode isoCode )
    {
        ContentValues values = new ContentValues();
        values.put( DBHelper.WORD, word );
        values.put( DBHelper.ISOCODE, isoCode.toString() );

        initDB( context );
        synchronized( s_dbHelper ) {
            insert( TABLE_NAMES.STUDYLIST, values );
        }
        notifyStudyListListeners( word, isoCode );
    }

    public static ISOCode[] studyListLangs( Context context )
    {
        ISOCode[] result = null;
        String[] columns = { DBHelper.ISOCODE };
        String groupBy = columns[0];

        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = s_db.query( TABLE_NAMES.STUDYLIST.toString(), columns,
                                        null, null, groupBy, null, null );
            int count = cursor.getCount();
            result = new ISOCode[count];
            if ( 0 < count ) {
                int index = 0;
                int colIndex = cursor.getColumnIndex( columns[0] );
                while ( cursor.moveToNext() ) {
                    result[index++] = new ISOCode(cursor.getString(colIndex));
                }
            }
            cursor.close();
        }
        return result;
    }

    public static String[] studyListWords( Context context, ISOCode isoCode )
    {
        String[] result = null;
        String selection = String.format( "%s = '%s'", DBHelper.ISOCODE, isoCode );
        String[] columns = { DBHelper.WORD };
        String orderBy = columns[0];

        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = DBHelper.query( s_db, TABLE_NAMES.STUDYLIST, columns,
                                            selection, orderBy );
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
        }
        return result;
    }

    public static void studyListClear( Context context, ISOCode isoCode, String[] words )
    {
        String selection = String.format( "%s = '%s'", DBHelper.ISOCODE, isoCode );
        if ( null != words ) {
            selection += String.format( " AND %s in ('%s')", DBHelper.WORD,
                                        TextUtils.join("','", words) );
        }

        initDB( context );
        synchronized( s_dbHelper ) {
            delete( TABLE_NAMES.STUDYLIST, selection );
        }
    }

    public static void studyListClear( Context context, ISOCode isoCode  )
    {
        studyListClear( context, isoCode, null );
    }

    public static void saveXlations( Context context, String locale,
                                     Map<String, String> data, boolean blessed )
    {
        if ( null != data && 0 < data.size() ) {
            long blessedLong = blessed ? 1 : 0;
            Iterator<String> iter = data.keySet().iterator();

            String insertQuery = "insert into %s (%s, %s, %s, %s) "
                + " VALUES (?, ?, ?, ?)";
            insertQuery = String.format( insertQuery, TABLE_NAMES.LOC,
                                         DBHelper.KEY, DBHelper.LOCALE,
                                         DBHelper.BLESSED, DBHelper.XLATION );

            String updateQuery = "update %s set %s = ? "
                + " WHERE %s = ? and %s = ? and %s = ?";
            updateQuery = String.format( updateQuery, TABLE_NAMES.LOC,
                                         DBHelper.XLATION, DBHelper.KEY,
                                         DBHelper.LOCALE, DBHelper.BLESSED );

            initDB( context );
            synchronized( s_dbHelper ) {
                SQLiteStatement insertStmt = s_db.compileStatement( insertQuery );
                SQLiteStatement updateStmt = s_db.compileStatement( updateQuery );

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
                            Log.ex( TAG, ex );
                            Assert.failDbg();
                        }
                    }
                }
            }
        }
    }

    // You can't have an array of paramterized types in java, so we'll let the
    // caller cast.
    public static Object[] getXlations( Context context, String locale )
    {
        HashMap<String, String> local = new HashMap<>();
        HashMap<String, String> blessed = new HashMap<>();

        String selection = String.format( "%s = '%s'", DBHelper.LOCALE,
                                          locale );
        String[] columns = { DBHelper.KEY, DBHelper.XLATION, DBHelper.BLESSED };

        initDB( context );
        synchronized( s_dbHelper ) {
            Cursor cursor = query( TABLE_NAMES.LOC, columns, selection );
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
            delete( TABLE_NAMES.LOC, selection );
        }
    }

    private static void setStringForSync( SQLiteDatabase db, String key, String value )
    {
        String selection = String.format( "%s = '%s'", DBHelper.KEY, key );
        ContentValues values = new ContentValues();
        values.put( DBHelper.VALUE, value );

        long result = DBHelper.update( db, TABLE_NAMES.PAIRS, values, selection );
        if ( 0 == result ) {
            values.put( DBHelper.KEY, key );
            DBHelper.insert( db, TABLE_NAMES.PAIRS, values );
        }
    }

    private static void delStringsLikeSync( SQLiteDatabase db, String like )
    {
        String selection = String.format( "%s LIKE '%s'", DBHelper.KEY, like );
        delete( db, TABLE_NAMES.PAIRS, selection, null );
    }

    private static String getStringForSyncSel( SQLiteDatabase db, String selection )
    {
        String result = null;
        String[] columns = { DBHelper.VALUE, };
        // If there are multiple matches, we want to use the newest. At least
        // that's the right move where a devID's key has been changed with
        // each upgrade.
        String orderBy = ROW_ID + " DESC";

        Cursor cursor = DBHelper.query( db, TABLE_NAMES.PAIRS, columns, selection, orderBy );
        // Log.d( TAG, "getStringForSyncSel(selection=%s)", selection );
        boolean tooMany = BuildConfig.DEBUG && 1 < cursor.getCount();

        if ( cursor.moveToNext() ) {
            result = cursor.getString( cursor.getColumnIndex(DBHelper.VALUE) );
        }
        cursor.close();

        return result;
    }

    private static String getStringForSync( SQLiteDatabase db, String key,
                                            String keyEndsWith, String dflt )
    {
        String selection = String.format( "%s = '%s'", DBHelper.KEY, key );
        boolean found = false;

        String oneResult = getStringForSyncSel( db, selection );
        if ( null == oneResult && null != keyEndsWith ) {
            selection = String.format( "%s LIKE '%%%s'", DBHelper.KEY, keyEndsWith );
            oneResult = getStringForSyncSel( db, selection );
            // Log.d( TAG, "getStringForSync() LIKE case: %s => %s", keyEndsWith, oneResult );
            if ( null != oneResult ) {
                setStringForSync( db, key, oneResult ); // store so won't need LIKE in future
            }
        }

        if ( null != oneResult ) {
            dflt = oneResult;
        }

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
            result = getStringForSync( s_db, key, null, null );
            result = proc.modifySync( result );
            setStringForSync( s_db, key, result );
        }
        return result;
    }

    public static void setStringFor( Context context, String key, String value )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            setStringForSync( s_db, key, value );
        }
    }

    public static String getStringFor( Context context, String key )
    {
        return getStringFor( context, key, null );
    }

    public static String getStringFor( Context context, String key, String dflt )
    {
        return getStringFor( context, key, null, dflt );
    }

    public static String getStringFor( Context context, String key,
                                       String keyEndsWith, String dflt )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            dflt = getStringForSync( s_db, key, keyEndsWith, dflt );
        }
        return dflt;
    }

    public static void setIntFor( Context context, String key, int value )
    {
        // Log.d( TAG, "DBUtils.setIntFor(key=%s, val=%d)", key, value );
        String asStr = String.format( "%d", value );
        setStringFor( context, key, asStr );
    }

    public static int getIntFor( Context context, String key, int dflt )
    {
        String asStr = getStringFor( context, key, null );
        if ( null != asStr ) {
            dflt = Integer.parseInt( asStr );
        }
        // Log.d( TAG, "DBUtils.getIntFor(key=%s)=>%d", key, dflt );
        return dflt;
    }

    public static void setLongFor( Context context, String key, long value )
    {
        // Log.d( TAG, "DBUtils.setIntFor(key=%s, val=%d)", key, value );
        String asStr = String.format( "%d", value );
        setStringFor( context, key, asStr );
    }

    public static long getLongFor( Context context, String key, long dflt )
    {
        String asStr = getStringFor( context, key, null );
        if ( null != asStr ) {
            dflt = Long.parseLong( asStr );
        }
        // Log.d( TAG, "DBUtils.getIntFor(key=%s)=>%d", key, dflt );
        return dflt;
    }

    public static void setBoolFor( Context context, String key, boolean value )
    {
        // Log.df( "DBUtils.setBoolFor(key=%s, val=%b)", key, value );
        String asStr = String.format( "%b", value );
        setStringFor( context, key, asStr );
    }

    public static boolean getBoolFor( Context context, String key, boolean dflt )
    {
        String asStr = getStringFor( context, key, null );
        if ( null != asStr ) {
            dflt = Boolean.parseBoolean( asStr );
        }
        // Log.df( "DBUtils.getBoolFor(key=%s)=>%b", key, dflt );
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
        String asStr = Utils.base64Encode( bytes );
        setStringFor( context, key, asStr );
    }

    public static byte[] getBytesFor( Context context, String key )
    {
        return getBytesFor( context, key, null );
    }

    public static byte[] getBytesFor( Context context, String key, String keyEndsWith )
    {
        byte[] bytes = null;
        String asStr = getStringFor( context, key, keyEndsWith, null );
        if ( null != asStr ) {
            bytes = Utils.base64Decode( asStr );
        }
        return bytes;
    }

    public static Serializable getSerializableFor( Context context, String key )
    {
        Serializable value = null;
        String str64 = getStringFor( context, key, "" );
        if ( str64 != null ) {
            value = (Serializable)Utils.string64ToSerializable( str64 );
        }
        return value;
    }

    public static void setSerializableFor( Context context, String key,
                                           Serializable value )
    {
        String str64 = null == value ? "" : Utils.serializableToString64( value );
        setStringFor( context, key, str64 );
    }

    public static void appendLog( String tag, String msg )
    {
        appendLog( XWApp.getContext(), msg );
    }

    private static void appendLog( Context context, String msg )
    {
        if ( 0 < LOGLIMIT ) {
            ContentValues values = new ContentValues();
            values.put( DBHelper.MESSAGE, msg );

            initDB( context );
            synchronized( s_dbHelper ) {
                long rowid = insert( TABLE_NAMES.LOGS, values );

                if ( 0 == (rowid % (LOGLIMIT / 10)) ) {
                    String where =
                        String.format( "not rowid in (select rowid from %s order by TIMESTAMP desc limit %d)",
                                       TABLE_NAMES.LOGS, LOGLIMIT );
                    int nGone = delete( TABLE_NAMES.LOGS, where );
                    Log.i( TAG, "appendLog(): deleted %d rows", nGone );
                }
            }
        }
    }

    // Copy my .apk to the Downloads directory, from which a user could more
    // easily share it with somebody else. Should be blocked for apks
    // installed from the Play store since viral distribution isn't allowed,
    // but might be helpful in other cases. Need to figure out how to expose
    // it, and how to recommend transmissions. E.g. gmail doesn't let me
    // attach an .apk even if I rename it.
    static void copyApkToDownloads( Context context )
    {
        try {
            String myName = context.getPackageName();
            PackageManager pm = context.getPackageManager();
            ApplicationInfo appInfo = pm.getApplicationInfo( myName, 0 );

            File srcPath = new File( appInfo.publicSourceDir );
            File destPath = Environment
                .getExternalStoragePublicDirectory( Environment.DIRECTORY_DOWNLOADS );
            destPath = new File( destPath, context.getString(R.string.app_name) + ".apk" );

            FileInputStream src = new FileInputStream( srcPath );
            FileOutputStream dest = new FileOutputStream( destPath );
            copyStream( dest, src );
        } catch ( Exception ex ) {
            Log.e( TAG, "copyApkToDownloads(): got ex: %s", ex );
        }
    }

    private static String getVariantDBName()
    {
        return String.format( "%s_%s", DBHelper.getDBName(),
                              BuildConfig.FLAVOR );
    }

    // private static void clearChatHistoryString( Context context, long rowid )
    // {
    //     ContentValues values = new ContentValues();
    //     values.putNull( DBHelper.CHAT_HISTORY );
    //     updateRow( context, DBHelper.TABLE_NAMES.SUM, rowid, values );
    // }

    private static void showHiddenGames( Context context, SQLiteDatabase db )
    {
        Log.d( TAG, "showHiddenGames()" );
        String query = "select " + ROW_ID + " from summaries WHERE NOT groupid"
            + " IN (SELECT " + ROW_ID + " FROM groups);";
        List<String> ids = null;
        Cursor cursor = db.rawQuery( query, null );
        if ( 0 < cursor.getCount() ) {
            ids = new ArrayList<>();
            int indx = cursor.getColumnIndex( ROW_ID );
            while ( cursor.moveToNext() ) {
                long rowid = cursor.getLong( indx );
                ids.add( String.format("%d", rowid ) );
            }
        }
        cursor.close();

        if ( null != ids ) {
            String name = LocUtils.getString( context, R.string.recovered_group );
            long groupid = getGroupImpl( name );
            if ( GROUPID_UNSPEC == groupid ) {
                groupid = addGroupImpl( name );
            }

            query = String.format( "UPDATE summaries SET groupid = %d"
                                   + " WHERE rowid IN (%s);", groupid,
                                   TextUtils.join(",", ids ) );
            db.execSQL( query );
        }
    }

    private static void initDB( Context context )
    {
        synchronized( DBUtils.class ) {
            if ( null == s_dbHelper ) {
                Assert.assertNotNull( context );
                s_dbHelper = new DBHelper( context );
                // force any upgrade
                s_dbHelper.getWritableDatabase().close();
                s_db = s_dbHelper.getWritableDatabase();

                // Workaround for bug somewhere. Run this once on startup
                // before anything else uses the db.
                showHiddenGames( context, s_db );
            }
        }
    }

    static void hideGames( Context context, long rowid )
    {
        if ( BuildConfig.NON_RELEASE ) {
            int nonID = 500 + (Utils.nextRandomInt() % 1000);
            String query = String.format( "UPDATE summaries set GROUPID = %d"
                                          + " WHERE rowid = %d", nonID, rowid );
            initDB( context );
            synchronized( s_dbHelper ) {
                s_db.execSQL( query );
            }
        }
    }

    private static int updateRowImpl( TABLE_NAMES table,
                                      long rowid, ContentValues values )
    {
        String selection = String.format( ROW_ID_FMT, rowid );
        return DBHelper.update( s_db, table, values, selection );
    }


    private static void updateRow( Context context, TABLE_NAMES table,
                                   long rowid, ContentValues values )
    {
        initDB( context );
        synchronized( s_dbHelper ) {
            int result = updateRowImpl( table, rowid, values );
            if ( 0 == result ) {
                Log.w( TAG, "updateRow failed" );
            }
        }
    }

    private static int maxVISID( SQLiteDatabase db )
    {
        int result = 1;
        String query = String.format( "SELECT max(%s) FROM %s", DBHelper.VISID,
                                      TABLE_NAMES.SUM );
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

    private static void notifyStudyListListeners( String word, ISOCode isoCode )
    {
        synchronized( s_slListeners ) {
            for ( StudyListListener listener : s_slListeners ) {
                listener.onWordAdded( word, isoCode );
            }
        }
    }

    private static void notifyListeners( Context context, long rowid,
                                         GameChangeType change )
    {
        synchronized( s_listeners ) {
            Iterator<DBChangeListener> iter = s_listeners.iterator();
            while ( iter.hasNext() ) {
                iter.next().gameSaved( context, rowid, change );
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

    private static Cursor query( TABLE_NAMES table, String[] columns, String selection )
    {
        return DBHelper.query( s_db, table, columns, selection );
    }

    private static int delete( SQLiteDatabase db, TABLE_NAMES table, String selection, String[] args )
    {
        return db.delete( table.toString(), selection, args );
    }

    private static int delete( SQLiteDatabase db, TABLE_NAMES table, String selection )
    {
        return delete( db, table, selection, null );
    }

    private static int delete( TABLE_NAMES table, String selection )
    {
        return delete( s_db, table, selection, null );
    }

    private static int delete( TABLE_NAMES table, String selection, String[] args )
    {
        return delete( s_db, table, selection, args );
    }

    private static int update( TABLE_NAMES table, ContentValues values, String selection )
    {
        return DBHelper.update( s_db, table, values, selection );
    }

    private static long insert( TABLE_NAMES table, ContentValues values )
    {
        return DBHelper.insert( s_db, table, values );
    }
}

/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2015 by Eric House (xwords@eehouse.org).  All rights
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
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.provider.Telephony;
import android.text.Html;
import android.text.TextUtils;
import android.view.Display;

import org.eehouse.android.xw4.jni.CommonPrefs;
import org.eehouse.android.xw4.jni.CommsAddrRec;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnType;
import org.eehouse.android.xw4.jni.CommsAddrRec.CommsConnTypeSet;
import org.eehouse.android.xw4.jni.CurGameInfo;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;
import org.eehouse.android.xw4.jni.DrawCtx;
import org.eehouse.android.xw4.jni.GameSummary;
import org.eehouse.android.xw4.jni.JNIThread;
import org.eehouse.android.xw4.jni.JNIUtilsImpl;
import org.eehouse.android.xw4.jni.LastMoveInfo;
import org.eehouse.android.xw4.jni.TransportProcs;
import org.eehouse.android.xw4.jni.UtilCtxt;
import org.eehouse.android.xw4.jni.UtilCtxtImpl;
import org.eehouse.android.xw4.jni.XwJNI;
import org.eehouse.android.xw4.jni.XwJNI.GamePtr;
import org.eehouse.android.xw4.loc.LocUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;

public class GameUtils {
    private static final String TAG = GameUtils.class.getSimpleName();

    public static final String INTENT_KEY_ROWID = "rowid";

    interface ResendDoneProc {
        void onResendDone( Context context, int numSent );
    }

    private static Integer s_minScreen;
    // Used to determine whether to resend all messages on networking coming
    // back up.  The length of the array determines the number of times in the
    // interval we'll do a send.
    private static Map<CommsConnType, long[]> s_sendTimes = new HashMap<>();
    private static final long RESEND_INTERVAL_SECS = 60 * 5; // 5 minutes

    public static class NoSuchGameException extends RuntimeException {
        private long m_rowID;
        public NoSuchGameException( long rowid ) {
            m_rowID = rowid;
            Log.i( TAG, "NoSuchGameException(rowid=%d)", rowid);
            // DbgUtils.printStack( TAG );
        }
    }

    public static class BackMoveResult {
        LastMoveInfo m_lmi;     // instantiated on demand
        String m_chat;
        String m_chatFrom;
        long m_chatTs;
    }

    private static Object s_syncObj = new Object();

    public static byte[] savedGame( Context context, long rowid )
    {
        byte[] result = null;
        try ( GameLock lock = GameLock.tryLockRO( rowid ) ) {
            if ( null != lock ) {
                result = savedGame( context, lock );
            }
        }

        if ( null == result ) {
            String msg = "savedGame(): unable to get lock; holder dump: "
                + GameLock.getHolderDump( rowid );
            Log.d( TAG, msg );
            if ( BuildConfig.NON_RELEASE ) {
                Utils.emailAuthor( context, msg );
            }
            throw new NoSuchGameException( rowid );
        }

        return result;
    }

    public static byte[] savedGame( Context context, GameLock lock )
    {
        return DBUtils.loadGame( context, lock );
    } // savedGame

    /**
     * Open an existing game, and use its gi and comms addr as the
     * basis for a new one.
     */
    public static GameLock resetGame( Context context, GameLock lockSrc,
                                      GameLock lockDest, long groupID,
                                      boolean juggle )
    {
        CurGameInfo gi = new CurGameInfo( context );
        CommsAddrRec addr = null;

        try ( GamePtr gamePtr = loadMakeGame( context, gi, lockSrc ) ) {
            if ( XwJNI.game_hasComms( gamePtr ) ) {
                addr = XwJNI.comms_getAddr( gamePtr );
            }
        }

        try ( GamePtr gamePtr = XwJNI
              .initNew( gi, (UtilCtxt)null, (DrawCtx)null,
                        CommonPrefs.get( context ), (TransportProcs)null ) ) {

            if ( juggle ) {
                gi.juggle();
            }

            if ( null != addr ) {
                XwJNI.comms_augmentHostAddr( gamePtr, addr );
            }

            if ( null == lockDest ) {
                if ( DBUtils.GROUPID_UNSPEC == groupID ) {
                    groupID = DBUtils.getGroupForGame( context, lockSrc.getRowid() );
                }
                long rowid = saveNewGame( context, gamePtr, gi, groupID );
                lockDest = GameLock.tryLock( rowid );
            } else {
                saveGame( context, gamePtr, gi, lockDest, true );
            }
            summarize( context, lockDest, gamePtr, gi );
            DBUtils.saveThumbnail( context, lockDest, null );
        }
        return lockDest;
    } // resetGame

    public static boolean resetGame( Context context, long rowidIn )
    {
        boolean success = false;
        try ( GameLock lock = GameLock.lock( rowidIn, 500 ) ) {
            if ( null != lock ) {
                tellDied( context, lock, true );
                resetGame( context, lock, lock, DBUtils.GROUPID_UNSPEC, false );

                Utils.cancelNotification( context, rowidIn );
                success = true;
            } else {
                DbgUtils.toastNoLock( TAG, context, rowidIn,
                                      "resetGame(): rowid %d", rowidIn );
            }
        }
        return success;
    }

    private static int setFromFeedImpl( FeedUtilsImpl feedImpl )
    {
        int result = GameSummary.MSG_FLAGS_NONE;
        if ( feedImpl.m_gotChat ) {
            result |= GameSummary.MSG_FLAGS_CHAT;
        }
        if ( feedImpl.m_gotMsg ) {
            result |= GameSummary.MSG_FLAGS_TURN;
        }
        if ( feedImpl.m_gameOver ) {
            result |= GameSummary.MSG_FLAGS_GAMEOVER;
        }
        return result;
    }

    private static GameSummary summarize( Context context, GameLock lock,
                                          GamePtr gamePtr, CurGameInfo gi )
    {
        GameSummary summary = new GameSummary( gi );
        XwJNI.game_summarize( gamePtr, summary );

        DBUtils.saveSummary( context, lock, summary );
        return summary;
    }

    public static GameSummary summarize( Context context, GameLock lock )
    {
        GameSummary result = null;
        CurGameInfo gi = new CurGameInfo( context );
        try ( GamePtr gamePtr = loadMakeGame( context, gi, lock ) ) {
            if ( null != gamePtr ) {
                result = summarize( context, lock, gamePtr, gi );
            }
        }
        return result;
    }

    public static GameSummary getSummary( Context context, long rowid,
                                          long maxMillis )
    {
        GameSummary result = null;
        try ( JNIThread thread = JNIThread.getRetained( rowid ) ) {
            if ( null != thread ) {
                result = DBUtils.getSummary( context, thread.getLock() );
            } else {
                try ( GameLock lock = GameLock.lockRO( rowid, maxMillis ) ) {
                    if ( null != lock ) {
                        result = DBUtils.getSummary( context, lock );
                    }
                } catch ( GameLock.GameLockedException gle ) {
                    if ( false && BuildConfig.DEBUG ) {
                        String dump = GameLock.getHolderDump( rowid );
                        Log.d( TAG, "getSummary() got gle: %s; cur owner: %s",
                               gle, dump );

                        String msg = "getSummary() unable to lock; owner: " + dump;
                        Log.e( TAG, msg );
                    }
                }
            }
        }
        return result;
    }

    public static GameSummary getSummary( Context context, long rowid )
    {
        return getSummary( context, rowid, 0L );
    }

    public static long dupeGame( Context context, long rowidIn )
    {
        return dupeGame( context, rowidIn, DBUtils.GROUPID_UNSPEC );
    }

    public static long dupeGame( Context context, long rowidIn, long groupID )
    {
        long result = DBUtils.ROWID_NOTFOUND;

        try ( JNIThread thread = JNIThread.getRetained( rowidIn ) ) {
            if ( null != thread ) {
                result = dupeGame( context, thread.getLock(), groupID );
            } else {
                try ( GameLock lockSrc = GameLock.lockRO( rowidIn, 300 ) ) {
                    if ( null != lockSrc ) {
                        result = dupeGame( context, lockSrc, groupID );
                    }
                } catch ( GameLock.GameLockedException gle ) {
                }
            }
        }

        if ( DBUtils.ROWID_NOTFOUND == result ) {
            Log.d( TAG, "dupeGame: unable to open rowid %d", rowidIn );
        }
        return result;
    }

    private static long dupeGame( Context context, GameLock lock, long groupID )
    {
        long result;
        boolean juggle = CommonPrefs.getAutoJuggle( context );
        try ( GameLock lockDest = resetGame( context, lock,
                                             null, groupID,
                                             juggle ) ) {
            result = lockDest.getRowid();
        }
        return result;
    }

    public static void deleteGame( Context context, GameLock lock,
                                   boolean informNow, boolean skipTell )
    {
        if ( null != lock ) {
            if ( !skipTell ) {
                tellDied( context, lock, informNow );
            }
            Utils.cancelNotification( context, lock.getRowid() );
            DBUtils.deleteGame( context, lock );
        } else {
            Log.e( TAG, "deleteGame(): null lock; doing nothing" );
        }
    }

    public static boolean deleteGame( Context context, long rowid,
                                      boolean informNow, boolean skipTell )
    {
        boolean success;
        // does this need to be synchronized?
        try ( GameLock lock = GameLock.tryLock( rowid ) ) {
            if ( null != lock ) {
                deleteGame( context, lock, informNow, skipTell );
                success = true;
            } else {
                DbgUtils.toastNoLock( TAG, context, rowid,
                                      "deleteGame(): rowid %d",
                                      rowid );
                success = false;
            }
        }
        return success;
    }

    public static void deleteGroup( Context context, long groupid )
    {
        int nSuccesses = 0;
        long[] rowids = DBUtils.getGroupGames( context, groupid );
        for ( int ii = rowids.length - 1; ii >= 0; --ii ) {
            if ( deleteGame( context, rowids[ii], ii == 0, false ) ) {
                ++nSuccesses;
            }
        }
        if ( rowids.length == nSuccesses ) {
            DBUtils.deleteGroup( context, groupid );
        }
    }

    public static String getName( Context context, long rowid )
    {
        String result = DBUtils.getName( context, rowid );
        if ( null == result || 0 == result.length() ) {
            int visID = DBUtils.getVisID( context, rowid );
            result = LocUtils.getString( context, R.string.game_fmt, visID );
        }
        return result;
    }

    public static String makeDefaultName( Context context )
    {
        int count = DBUtils.getIncrementIntFor( context, DBUtils.KEY_NEWGAMECOUNT, 0, 1 );
        return LocUtils.getString( context, R.string.game_fmt, count );
    }

    public static GamePtr loadMakeGame( Context context, GameLock lock )
    {
        return loadMakeGame( context, new CurGameInfo( context ), lock );
    }
    
    public static GamePtr loadMakeGame( Context context, CurGameInfo gi,
                                        TransportProcs tp, GameLock lock )
    {
        return loadMakeGame( context, gi, null, tp, lock );
    }

    public static GamePtr loadMakeGame( Context context, CurGameInfo gi,
                                        GameLock lock )
    {
        return loadMakeGame( context, gi, null, null, lock );
    }

    public static GamePtr loadMakeGame( Context context, CurGameInfo gi,
                                        UtilCtxt util, TransportProcs tp,
                                        GameLock lock )
    {
        byte[] stream = savedGame( context, lock );
        return loadMakeGame( context, gi, util, tp, stream, lock.getRowid() );
    }

    private static CurGameInfo giFromStream( Context context, byte[] stream )
    {
        CurGameInfo gi = null;
        if ( null != stream ) {
            gi = new CurGameInfo( context );
            XwJNI.gi_from_stream( gi, stream );
        }
        return gi;
    }

    private static GamePtr loadMakeGame( Context context, CurGameInfo gi,
                                         UtilCtxt util, TransportProcs tp,
                                         byte[] stream, long rowid )
    {
        GamePtr gamePtr = null;

        if ( null == stream ) {
            Log.w( TAG, "loadMakeGame: no saved game!");
        } else {
            XwJNI.gi_from_stream( gi, stream );
            String[] dictNames = gi.dictNames();
            DictUtils.DictPairs pairs = DictUtils.openDicts( context, dictNames );
            if ( pairs.anyMissing( dictNames ) ) {
                Log.w( TAG, "loadMakeGame() failing: dicts %s unavailable",
                       TextUtils.join( ",", dictNames ) );
            } else {
                String langName = gi.langName( context );
                gamePtr = XwJNI.initFromStream( rowid, stream, gi, util, null,
                                                CommonPrefs.get(context),
                                                tp );
                if ( null == gamePtr ) {
                    gamePtr = XwJNI.initNew( gi, (UtilCtxt)null, null,
                                             CommonPrefs.get(context), null );
                }
            }
        }
        return gamePtr;
    }

    public static Bitmap loadMakeBitmap( Context context, long rowid )
    {
        Bitmap thumb = null;
        try ( GameLock lock = GameLock.tryLockRO( rowid ) ) {
            if ( null != lock ) {
                CurGameInfo gi = new CurGameInfo( context );
                try ( GamePtr gamePtr = loadMakeGame( context, gi, lock ) ) {
                    if ( null != gamePtr ) {
                        thumb = takeSnapshot( context, gamePtr, gi );
                        DBUtils.saveThumbnail( context, lock, thumb );
                    }
                }
            }
        }
        return thumb;
    }

    public static Bitmap takeSnapshot( Context context, GamePtr gamePtr,
                                       CurGameInfo gi )
    {
        Bitmap thumb = null;
        if ( XWPrefs.getThumbEnabled( context ) ) {
            int nCols = gi.boardSize;
            int pct = XWPrefs.getThumbPct( context );
            Assert.assertTrue( 0 < pct );

            if ( null == s_minScreen ) {
                if ( context instanceof Activity ) {
                    Activity activity = (Activity)context;
                    Display display =
                        activity.getWindowManager().getDefaultDisplay();
                    int width = display.getWidth();
                    int height = display.getHeight();
                    s_minScreen = new Integer( Math.min( width, height ) );
                }
            }
            if ( null != s_minScreen ) {
                int dim = s_minScreen * pct / 100;
                int size = dim - (dim % nCols);

                thumb = Bitmap.createBitmap( size, size,
                                             Bitmap.Config.ARGB_8888 );
                ThumbCanvas canvas = new ThumbCanvas( context, thumb );
                XwJNI.board_drawSnapshot( gamePtr, canvas, size, size );
            }
        }
        return thumb;
    }

    // force applies only to relay
    public static void resendAllIf( Context context, CommsConnType filter )
    {
        resendAllIf( context, filter, false, false );
    }

    public static void resendAllIf( Context context, CommsConnType filter,
                                    boolean force, boolean showUI )
    {
        ResendDoneProc proc = null;
        if ( showUI ) {
            proc = new ResendDoneProc() {
                    @Override
                    public void onResendDone( Context context, int nSent )
                    {
                        String msg = LocUtils
                            .getQuantityString( context,
                                                R.plurals.resent_msgs_fmt,
                                                nSent, nSent );
                        DbgUtils.showf( context, msg );
                    }
                };
        }
        resendAllIf( context, filter, force, proc );
    }

    public static void resendAllIf( Context context, CommsConnType filter,
                                    boolean force, ResendDoneProc proc )
    {
        long now = Utils.getCurSeconds();

        // Note: HashMap permits null keys! So no need to test for null. BTW,
        // here null filter means "all".
        long[] sendTimes = s_sendTimes.get( filter );
        if ( null == sendTimes ) {
            sendTimes = new long[] { 0, 0, 0, 0 };
            s_sendTimes.put( filter, sendTimes );
        }

        if ( !force ) {
            long oldest = sendTimes[sendTimes.length - 1];
            long age = now - oldest;
            force = RESEND_INTERVAL_SECS < age;
            Log.d( TAG, "resendAllIf(): based on last send age of %d sec, doit = %b",
                   age, force );
        }

        if ( force ) {
            System.arraycopy( sendTimes, 0, /* src */
                              sendTimes, 1, /* dest */
                              sendTimes.length - 1 );
            sendTimes[0] = now;

            new Resender( context, filter, proc ).start();
        }
    }

    public static long saveGame( Context context, GamePtr gamePtr,
                                 CurGameInfo gi, GameLock lock,
                                 boolean setCreate )
    {
        byte[] stream = XwJNI.game_saveToStream( gamePtr, gi );
        return saveGame( context, stream, lock, setCreate );
    }

    public static long saveNewGame( Context context, GamePtr gamePtr,
                                    CurGameInfo gi, long groupID )
    {
        byte[] stream = XwJNI.game_saveToStream( gamePtr, gi );
        long rowid;
        try ( GameLock lock = DBUtils.saveNewGame( context, stream, groupID, null ) ) {
            rowid = lock.getRowid();
        }
        return rowid;
    }

    public static long saveGame( Context context, byte[] bytes,
                                 GameLock lock, boolean setCreate )
    {
        return DBUtils.saveGame( context, lock, bytes, setCreate );
    }

    public static GameLock saveNewGame( Context context, byte[] bytes )
    {
        return saveNewGame( context, bytes, DBUtils.GROUPID_UNSPEC );
    }

    public static GameLock saveNewGame( Context context, byte[] bytes,
                                        long groupID )
    {
        return DBUtils.saveNewGame( context, bytes, groupID, null );
    }

    public static long saveNew( Context context, CurGameInfo gi, long groupID,
                                String gameName )
    {
        if ( DBUtils.GROUPID_UNSPEC == groupID ) {
            groupID = XWPrefs.getDefaultNewGameGroup( context );
        }

        long rowid = DBUtils.ROWID_NOTFOUND;
        byte[] bytes = XwJNI.gi_to_stream( gi );
        if ( null != bytes ) {
            try ( GameLock lock = DBUtils.saveNewGame( context, bytes, groupID,
                                                       gameName ) ) {
                rowid = lock.getRowid();
            }
        }
        return rowid;
    }

    public static long makeNewMultiGame( Context context, NetLaunchInfo nli )
    {
        return makeNewMultiGame( context, nli, (MultiMsgSink)null,
                                 (UtilCtxt)null );
    }

    public static long makeNewMultiGame( Context context, NetLaunchInfo nli,
                                         MultiMsgSink sink, UtilCtxt util )
    {
        Log.d( TAG, "makeNewMultiGame(nli=%s)", nli.toString() );
        CommsAddrRec addr = nli.makeAddrRec( context );

        return makeNewMultiGame( context, sink, util, DBUtils.GROUPID_UNSPEC,
                                 addr, new int[] {nli.lang},
                                 new String[] { nli.dict }, null, nli.nPlayersT,
                                 nli.nPlayersH, nli.forceChannel,
                                 nli.inviteID(), nli.gameID(),
                                 nli.gameName, false, nli.remotesAreRobots );
    }

    public static long makeNewMultiGame( Context context, long groupID,
                                         String gameName )
    {
        return makeNewMultiGame( context, groupID, null, 0, null,
                                 (CommsConnTypeSet)null, gameName );
    }

    public static long makeNewMultiGame( Context context, long groupID,
                                         String dict, int lang, String jsonData,
                                         CommsConnTypeSet addrSet,
                                         String gameName )
    {
        String inviteID = makeRandomID();
        return makeNewMultiGame( context, groupID, inviteID, dict, lang,
                                 jsonData, addrSet, gameName );
    }

    private static long makeNewMultiGame( Context context, long groupID,
                                          String inviteID, String dict,
                                          int lang, String jsonData,
                                          CommsConnTypeSet addrSet,
                                          String gameName )
    {
        int[] langArray = {lang};
        String[] dictArray = {dict};
        if ( null == addrSet ) {
            addrSet = XWPrefs.getAddrTypes( context );
        }

        // Silently add this to any networked game if our device supports
        // it. comms is unhappy if we later pass in a message using an address
        // type the game doesn't have in its set.
        if ( NFCUtils.nfcAvail( context )[0] ) {
            addrSet.add( CommsConnType.COMMS_CONN_NFC );
        }

        CommsAddrRec addr = new CommsAddrRec( addrSet );
        addr.populate( context );
        int forceChannel = 0;
        return makeNewMultiGame( context, (MultiMsgSink)null, (UtilCtxt)null,
                                 groupID, addr, langArray, dictArray, jsonData,
                                 2, 1, forceChannel, inviteID, 0, gameName,
                                 true, false );
    }

    private static long makeNewMultiGame( Context context, MultiMsgSink sink,
                                          UtilCtxt util, long groupID,
                                          CommsAddrRec addr,
                                          int[] lang, String[] dict,
                                          String jsonData,
                                          int nPlayersT, int nPlayersH,
                                          int forceChannel, String inviteID,
                                          int gameID, String gameName,
                                          boolean isHost, boolean localsRobots )
    {
        long rowid = DBUtils.ROWID_NOTFOUND;

        Assert.assertNotNull( inviteID );
        CurGameInfo gi = new CurGameInfo( context, inviteID );
        gi.setFrom( jsonData );
        gi.setLang( context, lang[0], dict[0] );
        gi.forceChannel = forceChannel;
        lang[0] = gi.dictLang;
        dict[0] = gi.dictName;
        gi.setNPlayers( nPlayersT, nPlayersH, localsRobots );
        gi.juggle();
        if ( 0 != gameID ) {
            gi.gameID = gameID;
        }
        if ( isHost ) {
            gi.serverRole = DeviceRole.SERVER_ISSERVER;
        }
        // Will need to add a setNPlayers() method to gi to make this
        // work
        Assert.assertTrue( gi.nPlayers == nPlayersT );
        rowid = saveNew( context, gi, groupID, gameName );
        if ( null != sink ) {
            sink.setRowID( rowid );
        }

        if ( DBUtils.ROWID_NOTFOUND != rowid ) {
            // Use tryLock in case we're on UI thread. It's guaranteed to
            // succeed because we just created the rowid.
            try ( GameLock lock = GameLock.tryLock( rowid ) ) {
                Assert.assertNotNull( lock );
                applyChanges( context, sink, gi, util, addr, null, lock, false );
            }
        }

        return rowid;
    }

    // @SuppressLint({ "NewApi", "NewApi", "NewApi", "NewApi" })
    // @SuppressWarnings("deprecation")
    // @TargetApi(11)
    public static void inviteURLToClip( Context context, NetLaunchInfo nli )
    {
        Uri gameUri = nli.makeLaunchUri( context );
        String asStr = gameUri.toString();

        int sdk = Build.VERSION.SDK_INT;
        if ( sdk < Build.VERSION_CODES.HONEYCOMB ) {
            android.text.ClipboardManager clipboard =
                (android.text.ClipboardManager)
                context.getSystemService(Context.CLIPBOARD_SERVICE);
            clipboard.setText( asStr );
        } else {
            android.content.ClipboardManager clipboard =
                (android.content.ClipboardManager)
                context.getSystemService(Context.CLIPBOARD_SERVICE);
            String label = LocUtils.getString( context, R.string.clip_label );
            android.content.ClipData clip = android.content.ClipData
                .newPlainText( label, asStr );
            clipboard.setPrimaryClip( clip );
        }

        Utils.showToast( context, R.string.invite_copied );
    }

    public static void launchEmailInviteActivity( Activity activity, NetLaunchInfo nli )
    {
        String message = makeInviteMessage( activity, nli, R.string.invite_htm_fmt );
        if ( null != message ) {
            Intent intent = new Intent();
            intent.setAction( Intent.ACTION_SEND );
            String subject = null != nli.room ?
                LocUtils.getString( activity, R.string.invite_subject_fmt,
                                    nli.room )
                : LocUtils.getString( activity, R.string.invite_subject );
            intent.putExtra( Intent.EXTRA_SUBJECT, subject );
            intent.putExtra( Intent.EXTRA_TEXT, Html.fromHtml(message) );

            File attach = null;
            File tmpdir = BuildConfig.ATTACH_SUPPORTED ?
                DictUtils.getDownloadDir( activity ) : null;
            if ( null != tmpdir ) { // no attachment
                attach = makeJsonFor( tmpdir, nli );
            }

            if ( null == attach ) { // no attachment
                intent.setType( "message/rfc822");
            } else {
                String mime = LocUtils.getString( activity, R.string.invite_mime );
                intent.setType( mime );
                Uri uri = Uri.fromFile( attach );
                intent.putExtra( Intent.EXTRA_STREAM, uri );
            }

            String choiceType = LocUtils.getString( activity, R.string.invite_chooser_email );
            String chooserMsg =
                LocUtils.getString( activity, R.string.invite_chooser_fmt,
                                    choiceType );
            activity.startActivity( Intent.createChooser( intent, chooserMsg ) );
        }
    }

    // There seems to be no standard on how to launch an SMS app to send a
    // message. So let's gather here the stuff that works, and try in order
    // until something succeeds.
    //
    // And, added later and without the ability to test all of these, let's
    // not include a phone number.
    public static void launchSMSInviteActivity( Activity activity,
                                                NetLaunchInfo nli )
    {
        String message = makeInviteMessage( activity, nli,
                                            R.string.invite_sms_fmt );
        if ( null != message ) {
            boolean succeeded = false;
            String defaultSmsPkg = Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT
                ? Telephony.Sms.getDefaultSmsPackage(activity)
                : Settings.Secure.getString(activity.getContentResolver(),
                                            "sms_default_application");
            Log.d( TAG, "launchSMSInviteActivity(): default app: %s", defaultSmsPkg );

            outer:
            for ( int ii = 0; !succeeded; ++ii ) {
                Intent intent;
                switch ( ii ) {
                case 0:         // test case: QKSMS
                    intent = new Intent( Intent.ACTION_SEND )
                        .setPackage( defaultSmsPkg )
                        .setType( "text/plain" )
                        .putExtra( Intent.EXTRA_TEXT, message )
                        .putExtra( "sms_body", message )
                        ;
                    break;
                case 1:         // test case: Signal
                    intent = new Intent( Intent.ACTION_SENDTO )
                        .putExtra("sms_body", message)
                        .setPackage( defaultSmsPkg )
                        ;
                    break;
                case 2:
                    intent = new Intent( Intent.ACTION_VIEW )
                        .putExtra( "sms_body", message )
                        ;
                    break;
                default:
                    break outer;
                }
                try {
                    if ( intent.resolveActivity(activity.getPackageManager()) != null) {
                        activity.startActivity( intent );
                        succeeded = true;
                    }
                } catch ( Exception ex ) {
                    Log.e( TAG, "launchSMSInviteActivity(): ex: %s", ex );
                }
            }

            if ( !succeeded ) {
                DbgUtils.showf( activity, R.string.sms_invite_fail );
            }
        }
    }

    private static String makeInviteMessage( Activity activity, NetLaunchInfo nli,
                                             int fmtID )
    {
        String result = null;
        Uri gameUri = nli.makeLaunchUri( activity );
        String msgString = null == gameUri ? null : gameUri.toString();
        if ( null != msgString ) {
            result = LocUtils.getString( activity, fmtID, msgString );
        }
        return result;
    }

    public static String[] dictNames( Context context, GameLock lock )
    {
        String[] result = null;
        byte[] stream = savedGame( context, lock );
        CurGameInfo gi = giFromStream( context, stream );
        if ( null != gi ) {
            result = gi.dictNames();
        }
        return result;
    }

    public static String[] dictNames( Context context, long rowid,
                                      int[] missingLang )
    {
        String[] result = null;
        byte[] stream = savedGame( context, rowid );
        CurGameInfo gi = giFromStream( context, stream );
        if ( null != gi ) {
            if ( null != missingLang ) {
                missingLang[0] = gi.dictLang;
            }
            result = gi.dictNames();
        }
        return result;
    }

    public static String[] dictNames( Context context, long rowid )
    {
        return dictNames( context, rowid, null );
    }

    public static boolean gameDictsHere( Context context, long rowid )
    {
        return gameDictsHere( context, rowid, null, null );
    }

    public static boolean gameDictsHere( Context context, GameLock lock )
    {
        String[] gameDicts = dictNames( context, lock );
        return null != gameDicts && gameDictsHere( context, null, gameDicts );
    }

    // Return true if all dicts present.  Return list of those that
    // are not.
    public static boolean gameDictsHere( Context context, long rowid,
                                         String[][] missingNames,
                                         int[] missingLang )
    {
        String[] gameDicts = dictNames( context, rowid, missingLang );
        return null != gameDicts
            && gameDictsHere( context, missingNames, gameDicts );
    }

    public static boolean gameDictsHere( Context context,
                                         String[][] missingNames,
                                         String[] gameDicts )
    {
        HashSet<String> missingSet;
        DictUtils.DictAndLoc[] installed = DictUtils.dictList( context );

        missingSet = new HashSet<>( Arrays.asList( gameDicts ) );
        missingSet.remove( null );
        boolean allHere = 0 != missingSet.size(); // need some non-null!
        if ( allHere ) {
            for ( DictUtils.DictAndLoc dal : installed ) {
                missingSet.remove( dal.name );
            }
            allHere = 0 == missingSet.size();
        } else {
            Log.w( TAG, "gameDictsHere: game has no dicts!" );
        }
        if ( null != missingNames ) {
            missingNames[0] =
                missingSet.toArray( new String[missingSet.size()] );
        }

        return allHere;
    }

    public static String newName( Context context )
    {
        return "untitled";
        // String name = null;
        // Integer num = 1;
        // int ii;
        // long[] rowids = DBUtils.gamesList( context );
        // String fmt = context.getString( R.string.gamef );

        // while ( name == null ) {
        //     name = String.format( fmt + XWConstants.GAME_EXTN, num );
        //     for ( ii = 0; ii < files.length; ++ii ) {
        //         if ( files[ii].equals(name) ) {
        //             ++num;
        //             name = null;
        //         }
        //     }
        // }
        // return name;
    }

    private static boolean isGame( String file )
    {
        return file.endsWith( XWConstants.GAME_EXTN );
    }

    private static Bundle makeLaunchExtras( long rowid )
    {
        Bundle bundle = new Bundle();
        bundle.putLong( INTENT_KEY_ROWID, rowid );
        return bundle;
    }

    public static void launchGame( Delegator delegator, long rowid )
    {
        launchGame( delegator, rowid, null );
    }

    public static void launchGame( Delegator delegator, long rowid,
                                   Bundle moreExtras )
    {
        Bundle extras = makeLaunchExtras( rowid );
        if ( null != moreExtras ) {
            extras.putAll( moreExtras );
        }

        delegator.addFragment( BoardFrag.newInstance( delegator ), extras );
    }

    private static class FeedUtilsImpl extends UtilCtxtImpl {
        private Context m_context;
        private long m_rowid;
        public String m_chat;
        public long m_ts;
        public boolean m_gotMsg;
        public boolean m_gotChat;
        public String m_chatFrom;
        public boolean m_gameOver;

        public FeedUtilsImpl( Context context, long rowid )
        {
            super( context );
            m_context = context;
            m_rowid = rowid;
            m_gotMsg = false;
            m_gameOver = false;
        }

        @Override
        public void showChat( String msg, int fromIndx, String fromName, int tsSeconds )
        {
            DBUtils.appendChatHistory( m_context, m_rowid, msg, fromIndx, tsSeconds );
            m_gotChat = true;
            m_chatFrom = fromName;
            m_chat = msg;
            m_ts = tsSeconds;
        }

        @Override
        public void turnChanged( int newTurn )
        {
            m_gotMsg = true;
        }

        @Override
        public void notifyGameOver()
        {
            m_gameOver = true;
        }
    }

    public static boolean feedMessage( Context context, long rowid, byte[] msg,
                                       CommsAddrRec ret, MultiMsgSink sink,
                                       BackMoveResult bmr, boolean[] isLocalOut )
    {
        Assert.assertTrue( DBUtils.ROWID_NOTFOUND != rowid );
        boolean draw = false;
        Assert.assertTrue( -1 != rowid );
        if ( null != msg ) {
            // timed lock: If a game is opened by BoardActivity just
            // as we're trying to deliver this message to it it'll
            // have the lock and we'll never get it.  Better to drop
            // the message than fire the hung-lock assert.  Messages
            // belong in local pre-delivery storage anyway.
            try ( GameLock lock = GameLock.lock( rowid, 150 ) ) {
                if ( null != lock ) {
                    CurGameInfo gi = new CurGameInfo( context );
                    FeedUtilsImpl feedImpl = new FeedUtilsImpl( context, rowid );
                    try ( GamePtr gamePtr = loadMakeGame( context, gi, feedImpl, sink, lock ) ) {
                        if ( null != gamePtr ) {
                            XwJNI.comms_resendAll( gamePtr, false, false );

                            Assert.assertNotNull( ret );
                            draw = XwJNI.game_receiveMessage( gamePtr, msg, ret );
                            XwJNI.comms_ackAny( gamePtr );

                            // update gi to reflect changes due to messages
                            XwJNI.game_getGi( gamePtr, gi );

                            if ( draw && XWPrefs.getThumbEnabled( context ) ) {
                                Bitmap bitmap = takeSnapshot( context, gamePtr, gi );
                                DBUtils.saveThumbnail( context, lock, bitmap );
                            }

                            if ( null != bmr ) {
                                if ( null != feedImpl.m_chat ) {
                                    bmr.m_chat = feedImpl.m_chat;
                                    bmr.m_chatFrom = feedImpl.m_chatFrom;
                                    bmr.m_chatTs = feedImpl.m_ts;
                                } else {
                                    bmr.m_lmi = XwJNI.model_getPlayersLastScore( gamePtr, -1 );
                                }
                            }

                            saveGame( context, gamePtr, gi, lock, false );
                            GameSummary summary = summarize( context, lock,
                                                             gamePtr, gi );
                            if ( null != isLocalOut ) {
                                isLocalOut[0] = 0 <= summary.turn
                                    && gi.players[summary.turn].isLocal;
                            }
                        }

                        int flags = setFromFeedImpl( feedImpl );
                        if ( GameSummary.MSG_FLAGS_NONE != flags ) {
                            draw = true;
                            int curFlags = DBUtils.getMsgFlags( context, rowid );
                            DBUtils.setMsgFlags( context, rowid, flags | curFlags );
                        }
                    }
                }
            } catch ( GameLock.GameLockedException gle ) {
                DbgUtils.toastNoLock( TAG, context, rowid,
                                      "feedMessage(): dropping message"
                                      + " for %d", rowid );
            }
        }
        return draw;
    }

    // This *must* involve a reset if the language is changing!!!
    // Which isn't possible right now, so make sure the old and new
    // dict have the same langauge code.
    public static boolean replaceDicts( Context context, long rowid,
                                        String oldDict, String newDict )
    {
        boolean success;
        try ( GameLock lock = GameLock.lock( rowid, 300 ) ) {
            success = null != lock;
            if ( !success ) {
                DbgUtils.toastNoLock( TAG, context, rowid,
                                      "replaceDicts(): rowid %d",
                                      rowid );
            } else {
                byte[] stream = savedGame( context, lock );
                CurGameInfo gi = giFromStream( context, stream );
                success = null != gi;
                if ( !success ) {
                    Log.e( TAG, "replaceDicts(): unable to load rowid %d", rowid );
                } else {
                    // first time required so dictNames() will work
                    gi.replaceDicts( context, newDict );

                    try ( GamePtr gamePtr =
                          XwJNI.initFromStream( rowid, stream, gi, null, null,
                                                CommonPrefs.get( context ), null ) ) {
                        // second time required as game_makeFromStream can overwrite
                        gi.replaceDicts( context, newDict );

                        saveGame( context, gamePtr, gi, lock, false );

                        summarize( context, lock, gamePtr, gi );
                    }
                }
            }
        }
        return success;
    } // replaceDicts

    public static void applyChanges( Context context, CurGameInfo gi,
                                     CommsAddrRec car,
                                     Map<CommsConnType, boolean[]> disab,
                                     GameLock lock, boolean forceNew )
    {
        applyChanges( context, (MultiMsgSink)null, gi, (UtilCtxt)null, car,
                      disab, lock, forceNew );
    }

    public static void applyChanges( Context context, MultiMsgSink sink,
                                     CurGameInfo gi, UtilCtxt util,
                                     CommsAddrRec car,
                                     Map<CommsConnType, boolean[]> disab,
                                     GameLock lock, boolean forceNew )
    {
        // This should be a separate function, commitChanges() or
        // somesuch.  But: do we have a way to save changes to a gi
        // that don't reset the game, e.g. player name for standalone
        // games?
        boolean madeGame = false;
        CommonPrefs cp = CommonPrefs.get( context );

        if ( forceNew ) {
            tellDied( context, lock, true );
        } else {
            byte[] stream = savedGame( context, lock );
            // Will fail if there's nothing in the stream but a gi.
            try ( GamePtr gamePtr = XwJNI
                  .initFromStream( lock.getRowid(), stream,
                                   new CurGameInfo(context),
                                   null, null, cp, null ) ) {
                if ( null != gamePtr ) {
                    applyChanges( context, sink, gi, car, disab,
                                  lock, gamePtr );
                    madeGame = true;
                }
            }
        }

        if ( forceNew || !madeGame ) {
            try ( GamePtr gamePtr = XwJNI.initNew( gi, util, (DrawCtx)null,
                                                   cp, sink ) ) {
                if ( null != gamePtr ) {
                    applyChanges( context, sink, gi, car, disab, lock, gamePtr );
                }
            }
        }
    }

    private static void applyChanges( Context context, MultiMsgSink sink,
                                      CurGameInfo gi, CommsAddrRec car,
                                      Map<CommsConnType, boolean[]> disab,
                                      GameLock lock, GamePtr gamePtr )
    {
        if ( null != car ) {
            XwJNI.comms_augmentHostAddr( gamePtr, car );
        }

        if ( BuildConfig.DEBUG && null != disab ) {
            for ( CommsConnType typ : disab.keySet() ) {
                boolean[] bools = disab.get( typ );
                XwJNI.comms_setAddrDisabled( gamePtr, typ, false, bools[0] );
                XwJNI.comms_setAddrDisabled( gamePtr, typ, true, bools[1] );
            }
        }

        if ( null != sink ) {
            JNIThread.tryConnect( gamePtr, gi );
        }

        saveGame( context, gamePtr, gi, lock, false );

        GameSummary summary = new GameSummary( gi );
        XwJNI.game_summarize( gamePtr, summary );
        DBUtils.saveSummary( context, lock, summary );
    } // applyChanges

    public static String formatGameID( int gameID )
    {
        Assert.assertTrue( 0 != gameID );
        // substring: Keep it short so fits in SMS better
        return String.format( "%X", gameID ).substring( 0, 5 );
    }

    public static String makeRandomID()
    {
        int rint = newGameID();
        return formatGameID( rint );
    }

    public static int newGameID()
    {
        int rint;
        do {
            rint = Utils.nextRandomInt();
        } while ( 0 == rint );
        Log.i( TAG, "newGameID()=>%X (%d)", rint, rint );
        return rint;
    }

    public static void postMoveNotification( Context context, long rowid,
                                             BackMoveResult bmr,
                                             boolean isTurnNow )
    {
        if ( null != bmr ) {
            Intent intent = GamesListDelegate.makeRowidIntent( context, rowid );
            String msg = null;
            int titleID = 0;
            if ( null != bmr.m_chat ) {
                titleID = R.string.notify_chat_title_fmt;
                if ( null != bmr.m_chatFrom ) {
                    msg = LocUtils
                        .getString( context, R.string.notify_chat_body_fmt,
                                    bmr.m_chatFrom, bmr.m_chat );
                } else {
                    msg = bmr.m_chat;
                }
            } else if ( null != bmr.m_lmi ) {
                if ( isTurnNow ) {
                    titleID = R.string.notify_title_turn_fmt;
                } else {
                    titleID = R.string.notify_title_fmt;
                }
                msg = bmr.m_lmi.format( context );
            }

            if ( 0 != titleID ) {
                String title = LocUtils.getString( context, titleID,
                                                   getName( context, rowid ) );
                Utils.postNotification( context, intent, title, msg, rowid );
            }
        } else {
            Log.d( TAG, "postMoveNotification(): posting nothing for lack"
                   + " of brm" );
        }
    }

    public static void postInvitedNotification( Context context, int gameID,
                                                String body, long rowid )
    {
        Intent intent = GamesListDelegate.makeGameIDIntent( context, gameID );
        Utils.postNotification( context, intent, R.string.invite_notice_title,
                                body, rowid );
    }

    private static void tellDied( Context context, GameLock lock,
                                  boolean informNow )
    {
        GameSummary summary = DBUtils.getSummary( context, lock );
        if ( null == summary ) {
            Log.e( TAG, "tellDied(): can't get summary" );
        } else if ( DeviceRole.SERVER_STANDALONE != summary.serverRole ) {
            int gameID = summary.gameID;

            try ( GamePtr gamePtr = loadMakeGame( context, lock ) ) {
                if ( null != gamePtr ) {
                    Assert.assertTrue( XwJNI.game_hasComms( gamePtr )
                                       || !BuildConfig.DEBUG );
                    CommsAddrRec[] addrs = XwJNI.comms_getAddrs( gamePtr );
                    for ( CommsAddrRec addr : addrs ) {
                        CommsConnTypeSet conTypes = addr.conTypes;
                        for ( CommsConnType typ : conTypes ) {
                            switch ( typ ) {
                            case COMMS_CONN_RELAY:
                                // see below
                                break;
                            case COMMS_CONN_BT:
                                BTUtils.gameDied( context, addr.bt_btAddr, gameID );
                                break;
                            case COMMS_CONN_SMS:
                                NBSProto.gameDied( context, gameID, addr.sms_phone );
                                break;
                            case COMMS_CONN_P2P:
                                WiDirService.gameDied( addr.p2p_addr, gameID );
                                break;
                            case COMMS_CONN_MQTT:
                                MQTTUtils.gameDied( context, addr.mqtt_devID, gameID );
                                break;
                            }
                        }
                    }

                    // comms doesn't have a relay address for us until the game's
                    // in play (all devices registered, at least.) To enable
                    // deleting on relay half-games that we created but nobody
                    // joined, special-case this one.
                    if ( summary.inRelayGame() ) {
                        tellRelayDied( context, summary, informNow );
                    }
                }
            }
        }
    }

    private static void tellRelayDied( Context context, GameSummary summary,
                                       boolean informNow )
    {
        if ( null != summary.relayID ) {
            DBUtils.addDeceased( context, summary.relayID, summary.seed );
            if ( informNow ) {
                NetUtils.informOfDeaths( context );
            }
        }
    }

    private static File makeJsonFor( File dir, NetLaunchInfo nli )
    {
        File result = null;
        if ( BuildConfig.ATTACH_SUPPORTED ) {
            byte[] data = nli.makeLaunchJSON().getBytes();

            File file = new File( dir, String.format("invite_%d", nli.gameID() ));
            try {
                FileOutputStream fos = new FileOutputStream( file );
                fos.write( data, 0, data.length );
                fos.close();
                result = file;
            } catch ( Exception ex ) {
                Log.ex( TAG, ex );
            }
        }
        return result;
    }

    private static class Resender extends Thread {
        private Context m_context;
        private ResendDoneProc m_doneProc;
        private CommsConnType m_filter;
        private Handler m_handler;

        public Resender( Context context, CommsConnType filter,
                         ResendDoneProc proc )
        {
            m_context = context;
            m_filter = filter;
            m_doneProc = proc;
            if ( null != proc ) {
                m_handler = new Handler();
            }
        }

        @Override
        public void run()
        {
            int nSentTotal = 0;
            HashMap<Long,CommsConnTypeSet> games
                = DBUtils.getGamesWithSendsPending( m_context );

            Iterator<Long> iter = games.keySet().iterator();
            while ( iter.hasNext() ) {
                long rowid = iter.next();

                // If we're looking for a specific type, check
                if ( null != m_filter ) {
                    CommsConnTypeSet gameSet = games.get( rowid );
                    if ( gameSet != null && ! gameSet.contains( m_filter ) ) {
                        continue;
                    }
                }

                try ( GameLock lock = GameLock.tryLockRO( rowid ) ) {
                    if ( null != lock ) {
                        CurGameInfo gi = new CurGameInfo( m_context );
                        MultiMsgSink sink = new MultiMsgSink( m_context, rowid );
                        try ( GamePtr gamePtr = loadMakeGame( m_context, gi, sink, lock ) ) {
                            if ( null != gamePtr ) {
                                int nSent = XwJNI.comms_resendAll( gamePtr, true,
                                                                   m_filter, false );
                                nSentTotal += sink.numSent();
                                // Log.d( TAG, "Resender.doInBackground(): sent %d "
                                //        + "messages for rowid %d (total now %d)",
                                //        nSent, rowid, nSentTotal );
                            } else {
                                Log.d( TAG, "Resender.doInBackground(): loadMakeGame()"
                                       + " failed for rowid %d", rowid );
                            }
                        }
                    } else {
                        try ( JNIThread thread = JNIThread.getRetained( rowid ) ) {
                            if ( null != thread ) {
                                thread.handle( JNIThread.JNICmd.CMD_RESEND, false,
                                               false, false );
                            } else {
                                Log.w( TAG, "Resender.doInBackground: unable to unlock %d",
                                       rowid );
                            }
                        }
                    }
                }
            }

            if ( null != m_doneProc ) {
                final int fSentTotal = nSentTotal;
                m_handler
                    .post( new Runnable() {
                            @Override
                            public void run() {
                                m_doneProc.onResendDone( m_context, fSentTotal );
                            }
                        });
            }
        }
    }
}

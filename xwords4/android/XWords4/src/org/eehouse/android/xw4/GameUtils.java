/* -*- compile-command: "cd ../../../../../; ant debug install"; -*- */
/*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.Html;
import android.text.TextUtils;
import java.io.File;
import java.io.FileOutputStream;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.concurrent.locks.Lock;
import org.json.JSONArray;
import org.json.JSONObject;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameUtils {

    public static final String INVITED = "invited";
    public static final String INTENT_KEY_ROWID = "rowid";
    public static final String INTENT_FORRESULT_ROWID = "forresult";

    private static final long GROUPID_UNSPEC = -1;

    public static class NoSuchGameException extends RuntimeException {
        public NoSuchGameException() {
            super();            // superfluous
            DbgUtils.logf( "creating NoSuchGameException");
        }
    }

    private static Object s_syncObj = new Object();

    public static byte[] savedGame( Context context, long rowid )
    {
        GameLock lock = new GameLock( rowid, false ).lock();
        byte[] result = savedGame( context, lock );
        lock.unlock();

        if ( null == result ) {
            throw new NoSuchGameException();
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
                                      GameLock lockDest, boolean juggle )
    {
        CurGameInfo gi = new CurGameInfo( context );
        CommsAddrRec addr = null;

        // loadMakeGame, if making a new game, will add comms as long
        // as DeviceRole.SERVER_STANDALONE != gi.serverRole
        int gamePtr = loadMakeGame( context, gi, lockSrc );
        String[] dictNames = gi.dictNames();
        DictUtils.DictPairs pairs = DictUtils.openDicts( context, dictNames );
        
        if ( XwJNI.game_hasComms( gamePtr ) ) {
            addr = new CommsAddrRec();
            XwJNI.comms_getAddr( gamePtr, addr );
            if ( CommsAddrRec.CommsConnType.COMMS_CONN_NONE == addr.conType ) {
                String relayName = XWPrefs.getDefaultRelayHost( context );
                int relayPort = XWPrefs.getDefaultRelayPort( context );
                XwJNI.comms_getInitialAddr( addr, relayName, relayPort );
            }
        }
        XwJNI.game_dispose( gamePtr );

        gamePtr = XwJNI.initJNI();
        XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get( context ), 
                                CommonPrefs.get( context ), dictNames,
                                pairs.m_bytes,  pairs.m_paths, gi.langName() );
                                
        if ( juggle ) {
            gi.juggle();
        }

        if ( null != addr ) {
            XwJNI.comms_setAddr( gamePtr, addr );
        }

        if ( null == lockDest ) {
            long groupID = DBUtils.getGroupForGame( context, lockSrc.getRowid() );
            long rowid = saveNewGame( context, gamePtr, gi, groupID );
            lockDest = new GameLock( rowid, true ).lock();
        } else {
            saveGame( context, gamePtr, gi, lockDest, true );
        }
        summarizeAndClose( context, lockDest, gamePtr, gi );

        return lockDest;
    } // resetGame

    public static void resetGame( Context context, long rowidIn )
    {
        GameLock lock = new GameLock( rowidIn, true ).lock( 500 );
        if ( null != lock ) {
            tellDied( context, lock, true );
            resetGame( context, lock, lock, false );
            lock.unlock();

            Utils.cancelNotification( context, (int)rowidIn );
        } else {
            DbgUtils.logf( "resetGame: unable to open rowid %d", rowidIn );
        }
    }

    private static GameSummary summarizeAndClose( Context context, 
                                                  GameLock lock,
                                                  int gamePtr, CurGameInfo gi )
    {
        return summarizeAndClose( context, lock, gamePtr, gi, null );
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

    private static GameSummary summarizeAndClose( Context context, 
                                                  GameLock lock,
                                                  int gamePtr, CurGameInfo gi,
                                                  FeedUtilsImpl feedImpl )
    {
        GameSummary summary = new GameSummary( context, gi );
        XwJNI.game_summarize( gamePtr, summary );

        if ( null != feedImpl ) {
            summary.pendingMsgLevel |= setFromFeedImpl( feedImpl );
        }

        DBUtils.saveSummary( context, lock, summary );

        XwJNI.game_dispose( gamePtr );
        return summary;
    }

    public static GameSummary summarize( Context context, GameLock lock )
    {
        CurGameInfo gi = new CurGameInfo( context );
        int gamePtr = loadMakeGame( context, gi, lock );

        return summarizeAndClose( context, lock, gamePtr, gi );
    }

    public static long dupeGame( Context context, long rowidIn )
    {
        long rowid = DBUtils.ROWID_NOTFOUND;
        GameLock lockSrc = new GameLock( rowidIn, false ).lock( 300 );
        if ( null != lockSrc ) {
            boolean juggle = CommonPrefs.getAutoJuggle( context );
            GameLock lockDest = resetGame( context, lockSrc, null, juggle );
            rowid = lockDest.getRowid();
            lockDest.unlock();
            lockSrc.unlock();
        } else {
            DbgUtils.logf( "dupeGame: unable to open rowid %d", rowidIn );
        }
        return rowid;
    }

    public static boolean deleteGame( Context context, long rowid, 
                                      boolean informNow )
    {
        boolean success;
        // does this need to be synchronized?
        GameLock lock = new GameLock( rowid, true );
        if ( lock.tryLock() ) {
            tellDied( context, lock, informNow );
            Utils.cancelNotification( context, (int)rowid );
            DBUtils.deleteGame( context, lock );
            lock.unlock();
            success = true;
        } else {
            DbgUtils.logf( "deleteGame: unable to delete rowid %d", rowid );
            success = false;
        }
        return success;
    }

    public static void deleteGroup( Context context, long groupid )
    {
        int nSuccesses = 0;
        long[] rowids = DBUtils.getGroupGames( context, groupid );
        for ( int ii = rowids.length - 1; ii >= 0; --ii ) {
            if ( deleteGame( context, rowids[ii], ii == 0 ) ) {
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
            result = context.getString( R.string.gamef, visID );
        }
        return result;
    }

    public static int loadMakeGame( Context context, CurGameInfo gi, 
                                    GameLock lock )
    {
        return loadMakeGame( context, gi, null, null, lock );
    }

    public static int loadMakeGame( Context context, CurGameInfo gi, 
                                    UtilCtxt util, TransportProcs tp, 
                                    GameLock lock )
    {
        int gamePtr = 0;

        byte[] stream = savedGame( context, lock );
        if ( null == stream ) {
            DbgUtils.logf( "loadMakeGame: no saved game!");
        } else {
            XwJNI.gi_from_stream( gi, stream );
            String[] dictNames = gi.dictNames();
            DictUtils.DictPairs pairs = DictUtils.openDicts( context, dictNames );
            if ( pairs.anyMissing( dictNames ) ) {
                DbgUtils.logf( "loadMakeGame() failing: dicts %s unavailable", 
                               TextUtils.join( ",", dictNames ) );
            } else {
                gamePtr = XwJNI.initJNI();

                String langName = gi.langName();
                boolean madeGame = 
                    XwJNI.game_makeFromStream( gamePtr, stream, gi, 
                                               dictNames, pairs.m_bytes, 
                                               pairs.m_paths, langName,
                                               util, JNIUtilsImpl.get( context ), 
                                               CommonPrefs.get(context),
                                               tp);
                if ( !madeGame ) {
                    XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(context), 
                                            CommonPrefs.get(context), dictNames,
                                            pairs.m_bytes, pairs.m_paths, 
                                            langName );
                }
            }
        }
        return gamePtr;
    }

    public static long saveGame( Context context, int gamePtr, 
                                 CurGameInfo gi, GameLock lock,
                                 boolean setCreate )
    {
        byte[] stream = XwJNI.game_saveToStream( gamePtr, gi );
        return saveGame( context, stream, lock, setCreate );
    }

    public static long saveNewGame( Context context, int gamePtr,
                                    CurGameInfo gi, long groupID )
    {
        byte[] stream = XwJNI.game_saveToStream( gamePtr, gi );
        GameLock lock = DBUtils.saveNewGame( context, stream, groupID );
        long rowid = lock.getRowid();
        lock.unlock();
        return rowid;
    }

    public static long saveGame( Context context, byte[] bytes, 
                                 GameLock lock, boolean setCreate )
    {
        return DBUtils.saveGame( context, lock, bytes, setCreate );
    }

    public static GameLock saveNewGame( Context context, byte[] bytes )
    {
        return DBUtils.saveNewGame( context, bytes );
    }

    public static long saveNew( Context context, CurGameInfo gi )
    {
        return saveNew( context, gi, GROUPID_UNSPEC );
    }

    public static long saveNew( Context context, CurGameInfo gi, long groupID )
    {
        if ( GROUPID_UNSPEC == groupID ) {
            groupID = XWPrefs.getDefaultNewGameGroup( context );
        }

        long rowid = DBUtils.ROWID_NOTFOUND;
        byte[] bytes = XwJNI.gi_to_stream( gi );
        if ( null != bytes ) {
            GameLock lock = DBUtils.saveNewGame( context, bytes, groupID );
            rowid = lock.getRowid();
            lock.unlock();
        }
        return rowid;
    }

    private static long makeNewMultiGame( Context context, long groupID, 
                                          CommsAddrRec addr,
                                          int[] lang, String[] dict,
                                          int nPlayersT, int nPlayersH, 
                                          String inviteID, int gameID,
                                          boolean isHost )
    {
        long rowid = -1;

        CurGameInfo gi = new CurGameInfo( context, true );
        gi.setLang( lang[0], dict[0] );
        lang[0] = gi.dictLang;
        dict[0] = gi.dictName;
        gi.setNPlayers( nPlayersT, nPlayersH );
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
        rowid = saveNew( context, gi, groupID );

        if ( DBUtils.ROWID_NOTFOUND != rowid ) {
            GameLock lock = new GameLock( rowid, true ).lock();
            applyChanges( context, gi, addr, inviteID, lock, false );
            lock.unlock();
        }

        return rowid;
    }

    public static long makeNewNetGame( Context context, long groupID,
                                       String room, String inviteID, int[] lang,
                                       String[] dict, int nPlayersT, 
                                       int nPlayersH )
    {
        long rowid = -1;
        String relayName = XWPrefs.getDefaultRelayHost( context );
        int relayPort = XWPrefs.getDefaultRelayPort( context );
        CommsAddrRec addr = new CommsAddrRec( relayName, relayPort );
        addr.ip_relay_invite = room;

        return makeNewMultiGame( context, groupID, addr, lang, dict, 
                                 nPlayersT, nPlayersH, inviteID, 0, false );
    }

    public static long makeNewNetGame( Context context, long groupID, 
                                       String room, String inviteID, int lang, 
                                       String dict, int nPlayers )
    {
        int[] langarr = { lang };
        String[] dictArr = { dict };
        return makeNewNetGame( context, groupID, room, inviteID, langarr, 
                               dictArr, nPlayers, 1 );
    }

    public static long makeNewNetGame( Context context, NetLaunchInfo info )
    {
        return makeNewNetGame( context, GROUPID_UNSPEC, info.room, 
                               info.inviteID, info.lang, info.dict, 
                               info.nPlayersT );
    }

    public static long makeNewBTGame( Context context, int gameID, 
                                      CommsAddrRec addr, int lang, 
                                      int nPlayersT, int nPlayersH )
    {
        return makeNewBTGame( context, GROUPID_UNSPEC, gameID, addr, lang, 
                              nPlayersT, nPlayersH );
    }
    
    public static long makeNewBTGame( Context context, long groupID, 
                                      int gameID, CommsAddrRec addr, int lang, 
                                      int nPlayersT, int nPlayersH )
    {
        long rowid = -1;
        int[] langa = { lang };
        boolean isHost = null == addr;
        if ( isHost ) { 
            addr = new CommsAddrRec( null, null );
        }
        return makeNewMultiGame( context, groupID, addr, langa, null, 
                                 nPlayersT, nPlayersH, null, gameID, isHost );
    }

    public static long makeNewSMSGame( Context context, int gameID, 
                                       CommsAddrRec addr, 
                                       int lang, String dict, int nPlayersT, 
                                       int nPlayersH )
    {
        return makeNewSMSGame( context, GROUPID_UNSPEC, gameID, addr, 
                               lang, dict, nPlayersT, nPlayersH );
    }

    public static long makeNewSMSGame( Context context, long groupID,
                                       int gameID, CommsAddrRec addr, 
                                       int lang, String dict, int nPlayersT, 
                                       int nPlayersH )
    {
        long rowid = -1;
        int[] langa = { lang };
        String[] dicta = { dict };
        boolean isHost = null == addr;
        if ( isHost ) { 
            addr = new CommsAddrRec(CommsAddrRec.CommsConnType.COMMS_CONN_SMS);
        }
        return makeNewMultiGame( context, groupID, addr, langa, dicta, 
                                 nPlayersT, nPlayersH, null, gameID, isHost );
    }

    public static void launchInviteActivity( Context context, 
                                             boolean choseEmail,
                                             String room, String inviteID,
                                             int lang, String dict, 
                                             int nPlayers )
    {
        if ( null == inviteID ) {
            inviteID = makeRandomID();
        }
        Uri gameUri = NetLaunchInfo.makeLaunchUri( context, room, inviteID,
                                                   lang, dict, nPlayers );

        if ( null != gameUri ) {
            int fmtId = choseEmail? R.string.invite_htmf : R.string.invite_txtf;
            int choiceID;
            String message = context.getString( fmtId, gameUri.toString() );

            Intent intent = new Intent();
            if ( choseEmail ) {
                intent.setAction( Intent.ACTION_SEND );
                String subject =
                    Utils.format( context, R.string.invite_subjectf, room );
                intent.putExtra( Intent.EXTRA_SUBJECT, subject );
                intent.putExtra( Intent.EXTRA_TEXT, Html.fromHtml(message) );

                File attach = null;
                File tmpdir = XWApp.ATTACH_SUPPORTED ? 
                    DictUtils.getDownloadDir( context ) : null;
                if ( null != tmpdir ) { // no attachment
                    attach = makeJsonFor( tmpdir, room, inviteID, lang, 
                                          dict, nPlayers );
                }

                if ( null == attach ) { // no attachment
                    intent.setType( "message/rfc822");
                } else {
                    String mime = context.getString( R.string.invite_mime );
                    intent.setType( mime );
                    Uri uri = Uri.fromFile( attach );
                    intent.putExtra( Intent.EXTRA_STREAM, uri );
                }

                choiceID = R.string.invite_chooser_email;
            } else {
                intent.setAction( Intent.ACTION_VIEW );
                intent.setType( "vnd.android-dir/mms-sms" );
                intent.putExtra( "sms_body", message );
                choiceID = R.string.invite_chooser_sms;
            }

            String choiceType = context.getString( choiceID );
            String chooserMsg = 
                Utils.format( context, R.string.invite_chooserf, choiceType );
            context.startActivity( Intent.createChooser( intent, chooserMsg ) );
        }
    }

    public static String[] dictNames( Context context, long rowid,
                                      int[] missingLang ) 
    {
        byte[] stream = savedGame( context, rowid );
        CurGameInfo gi = new CurGameInfo( context );
        XwJNI.gi_from_stream( gi, stream );
        if ( null != missingLang ) {
            missingLang[0] = gi.dictLang;
        }
        return gi.dictNames();
    }

    public static String[] dictNames( Context context, long rowid ) 
    {
        return dictNames( context, rowid, null );
    }
    
    public static boolean gameDictsHere( Context context, long rowid )
    {
        return gameDictsHere( context, rowid, null, null );
    }

    // Return true if all dicts present.  Return list of those that
    // are not.
    public static boolean gameDictsHere( Context context, long rowid,
                                         String[][] missingNames, 
                                         int[] missingLang )
    {
        String[] gameDicts = dictNames( context, rowid, missingLang );
        HashSet<String> missingSet;
        DictUtils.DictAndLoc[] installed = DictUtils.dictList( context );

        missingSet = new HashSet<String>( Arrays.asList( gameDicts ) );
        missingSet.remove( null );
        boolean allHere = 0 != missingSet.size(); // need some non-null!
        if ( allHere ) {
            for ( DictUtils.DictAndLoc dal : installed ) {
                missingSet.remove( dal.name );
            }
            allHere = 0 == missingSet.size();
        } else {
            DbgUtils.logf( "gameDictsHere: game has no dicts!" );
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

    public static void launchGame( Activity activity, long rowid,
                                   boolean invited )
    {
        Intent intent = new Intent( activity, BoardActivity.class );
        intent.putExtra( INTENT_KEY_ROWID, rowid );
        if ( invited ) {
            intent.putExtra( INVITED, true );
        }
        activity.startActivity( intent );
    }

    public static void launchGame( Activity activity, long rowid)
    {
        launchGame( activity, rowid, false );
    }

    public static void launchGameAndFinish( Activity activity, long rowid )
    {
        launchGame( activity, rowid );
        activity.finish();
    }

    private static class FeedUtilsImpl extends UtilCtxtImpl {
        private Context m_context;
        private long m_rowid;
        public boolean m_gotMsg;
        public boolean m_gotChat;
        public boolean m_gameOver;

        public FeedUtilsImpl( Context context, long rowid )
        {
            super( context );
            m_context = context;
            m_rowid = rowid;
            m_gotMsg = false;
            m_gameOver = false;
        }
        public void showChat( String msg )
        {
            DBUtils.appendChatHistory( m_context, m_rowid, msg, false );
            m_gotChat = true;
        }
        public void turnChanged( int newTurn )
        {
            m_gotMsg = true;
        }

        public void notifyGameOver()
        {
            m_gameOver = true;
        }
    }

    public static boolean feedMessages( Context context, long rowid,
                                        byte[][] msgs, CommsAddrRec ret,
                                        MultiMsgSink sink )
    {
        boolean draw = false;
        Assert.assertTrue( -1 != rowid );
        if ( null != msgs ) {
            // timed lock: If a game is opened by BoardActivity just
            // as we're trying to deliver this message to it it'll
            // have the lock and we'll never get it.  Better to drop
            // the message than fire the hung-lock assert.  Messages
            // belong in local pre-delivery storage anyway.
            GameLock lock = new GameLock( rowid, true ).lock( 150 );
            if ( null != lock ) {
                CurGameInfo gi = new CurGameInfo( context );
                FeedUtilsImpl feedImpl = new FeedUtilsImpl( context, rowid );
                int gamePtr = loadMakeGame( context, gi, feedImpl, sink, lock );
                if ( 0 != gamePtr ) {
                    XwJNI.comms_resendAll( gamePtr, false, false );

                    for ( byte[] msg : msgs ) {
                        draw = XwJNI.game_receiveMessage( gamePtr, msg, ret )
                            || draw;
                    }
                    XwJNI.comms_ackAny( gamePtr );

                    // update gi to reflect changes due to messages
                    XwJNI.game_getGi( gamePtr, gi );
                    saveGame( context, gamePtr, gi, lock, false );
                    summarizeAndClose( context, lock, gamePtr, gi, feedImpl );

                    int flags = setFromFeedImpl( feedImpl );
                    if ( GameSummary.MSG_FLAGS_NONE != flags ) {
                        draw = true;
                        DBUtils.setMsgFlags( rowid, flags );
                    }
                }
                lock.unlock();
            }
        }
        return draw;
    } // feedMessages

    public static boolean feedMessage( Context context, long rowid, byte[] msg,
                                       CommsAddrRec ret, MultiMsgSink sink )
    {
        Assert.assertTrue( DBUtils.ROWID_NOTFOUND != rowid );
        byte[][] msgs = new byte[1][];
        msgs[0] = msg;
        return feedMessages( context, rowid, msgs, ret, sink );
    }

    // This *must* involve a reset if the language is changing!!!
    // Which isn't possible right now, so make sure the old and new
    // dict have the same langauge code.
    public static boolean replaceDicts( Context context, long rowid,
                                        String oldDict, String newDict )
    {
        GameLock lock = new GameLock( rowid, true ).lock(300);
        boolean success = null != lock;
        if ( success ) {
            byte[] stream = savedGame( context, lock );
            CurGameInfo gi = new CurGameInfo( context );
            XwJNI.gi_from_stream( gi, stream );

            // first time required so dictNames() will work
            gi.replaceDicts( newDict );

            String[] dictNames = gi.dictNames();
            DictUtils.DictPairs pairs = DictUtils.openDicts( context, 
                                                             dictNames );
        
            int gamePtr = XwJNI.initJNI();
            XwJNI.game_makeFromStream( gamePtr, stream, gi, dictNames, 
                                       pairs.m_bytes, pairs.m_paths,
                                       gi.langName(), 
                                       JNIUtilsImpl.get(context), 
                                       CommonPrefs.get( context ) );
            // second time required as game_makeFromStream can overwrite
            gi.replaceDicts( newDict );

            saveGame( context, gamePtr, gi, lock, false );

            summarizeAndClose( context, lock, gamePtr, gi );

            lock.unlock();
        } else {
            DbgUtils.logf( "replaceDicts: unable to open rowid %d", rowid );
        }
        return success;
    } // replaceDicts

    public static void applyChanges( Context context, CurGameInfo gi, 
                                     CommsAddrRec car, GameLock lock, 
                                     boolean forceNew )
    {
        applyChanges( context, gi, car, null, lock, forceNew );
    }

    public static void applyChanges( Context context, CurGameInfo gi, 
                                     CommsAddrRec car, String inviteID, 
                                     GameLock lock, boolean forceNew )
    {
        // This should be a separate function, commitChanges() or
        // somesuch.  But: do we have a way to save changes to a gi
        // that don't reset the game, e.g. player name for standalone
        // games?
        String[] dictNames = gi.dictNames();
        DictUtils.DictPairs pairs = DictUtils.openDicts( context, dictNames );
        String langName = gi.langName();
        int gamePtr = XwJNI.initJNI();
        boolean madeGame = false;
        CommonPrefs cp = CommonPrefs.get( context );

        if ( forceNew ) {
            tellDied( context, lock, true );
        } else {
            byte[] stream = savedGame( context, lock );
            // Will fail if there's nothing in the stream but a gi.
            madeGame = XwJNI.game_makeFromStream( gamePtr, stream, 
                                                  new CurGameInfo(context), 
                                                  dictNames, pairs.m_bytes,
                                                  pairs.m_paths, langName,
                                                  JNIUtilsImpl.get(context),
                                                  cp );
        }

        if ( forceNew || !madeGame ) {
            XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(context), 
                                    cp, dictNames, pairs.m_bytes, 
                                    pairs.m_paths, langName );
        }

        if ( null != car ) {
            XwJNI.comms_setAddr( gamePtr, car );
        }

        saveGame( context, gamePtr, gi, lock, false );

        GameSummary summary = new GameSummary( context, gi );
        XwJNI.game_summarize( gamePtr, summary );
        DBUtils.saveSummary( context, lock, summary, inviteID );

        XwJNI.game_dispose( gamePtr );
    } // applyChanges

    public static void doConfig( Activity activity, long rowid, Class clazz )
    {
        Intent intent = new Intent( activity, clazz );
        intent.setAction( Intent.ACTION_EDIT );
        intent.putExtra( INTENT_KEY_ROWID, rowid );
        activity.startActivity( intent );
    }

    public static String makeRandomID()
    {
        int rint = newGameID();
        return String.format( "%X", rint ).substring( 0, 4 );
    }

    public static int newGameID()
    {
        int rint;
        do {
            rint = Utils.nextRandomInt();
        } while ( 0 == rint );
        DbgUtils.logf( "newGameID()=>%X", rint );
        return rint;
    }

    private static void tellDied( Context context, GameLock lock, 
                                  boolean informNow )
    {
        GameSummary summary = DBUtils.getSummary( context, lock );
        switch( summary.conType ) {
        case COMMS_CONN_RELAY:
            tellRelayDied( context, summary, informNow );
            break;
        case COMMS_CONN_BT:
            BTService.gameDied( context, summary.gameID );
            break;
        case COMMS_CONN_SMS:
            if ( null != summary.remoteDevs ) {
                for ( String dev : summary.remoteDevs ) {
                    SMSService.gameDied( context, summary.gameID, dev );
                }
            }
            break;
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

    private static File makeJsonFor( File dir, String room, String inviteID,
                                     int lang, String dict, int nPlayers )
    {
        File result = null;
        if ( XWApp.ATTACH_SUPPORTED ) {
            JSONObject json = new JSONObject();
            try {
                json.put( MultiService.ROOM, room );
                json.put( MultiService.INVITEID, inviteID );
                json.put( MultiService.LANG, lang );
                json.put( MultiService.DICT, dict );
                json.put( MultiService.NPLAYERST, nPlayers );
                byte[] data = json.toString().getBytes();

                File file = new File( dir, 
                                      String.format("invite_%s", room ) );
                FileOutputStream fos = new FileOutputStream( file );
                fos.write( data, 0, data.length );
                fos.close();
                result = file;
            } catch ( Exception ex ) {
                DbgUtils.loge( ex );
            }
        }
        return result;
    }

}

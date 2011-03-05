/* -*- compile-command: "cd ../../../../../; ant install"; -*- */
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
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import android.net.Uri;
import java.util.ArrayList;
import android.content.res.AssetManager;
import java.util.concurrent.locks.ReentrantReadWriteLock;
import java.util.concurrent.locks.Lock;
import java.util.HashMap;

import junit.framework.Assert;

import org.eehouse.android.xw4.jni.*;
import org.eehouse.android.xw4.jni.CurGameInfo.DeviceRole;

public class GameUtils {

    // Implements read-locks and write-locks per game.  A read lock is
    // obtainable when other read locks are granted but not when a
    // write lock is.  Write-locks are exclusive.
    public static class GameLock {
        private String m_path;
        private boolean m_isForWrite;
        private int m_lockCount;
        // StackTraceElement[] m_lockTrace;

        // This will leak empty ReentrantReadWriteLock instances for
        // now.
        private static HashMap<String, GameLock> 
            s_locks = new HashMap<String,GameLock>();

        public GameLock( String path, boolean isForWrite ) 
        {
            m_path = path;
            m_isForWrite = isForWrite;
            m_lockCount = 0;
            // Utils.logf( "GameLock.GameLock(%s,%s) done", m_path, 
            //             m_isForWrite?"T":"F" );
        }

        // This could be written to allow multiple read locks.  Let's
        // see if not doing that causes problems.
        public boolean tryLock()
        {
            boolean gotIt = false;
            synchronized( s_locks ) {
                GameLock owner = s_locks.get( m_path );
                if ( null == owner ) { // unowned
                    Assert.assertTrue( 0 == m_lockCount );
                    s_locks.put( m_path, this );
                    ++m_lockCount;
                    gotIt = true;
                    
                    // StackTraceElement[] trace = Thread.currentThread().
                    //     getStackTrace();
                    // m_lockTrace = new StackTraceElement[trace.length];
                    // System.arraycopy( trace, 0, m_lockTrace, 0, trace.length );
                } else if ( this == owner && ! m_isForWrite ) {
                    Assert.assertTrue( 0 == m_lockCount );
                    ++m_lockCount;
                    gotIt = true;
                }
            }
            return gotIt;
        }
        
        public GameLock lock()
        {
            long stopTime = System.currentTimeMillis() + 1000;
            // Utils.logf( "GameLock.lock(%s)", m_path );
            // Utils.printStack();
            for ( ; ; ) {
                if ( tryLock() ) {
                    break;
                }
                // Utils.logf( "GameLock.lock() failed; sleeping" );
                // Utils.printStack();
                try {
                    Thread.sleep( 25 ); // milliseconds
                } catch( InterruptedException ie ) {
                    Utils.logf( "GameLock.lock(): %s", ie.toString() );
                    break;
                }
                if ( System.currentTimeMillis() >= stopTime ) {
                    // Utils.printStack( m_lockTrace );
                    Assert.fail();
                }
            }
            // Utils.logf( "GameLock.lock(%s) done", m_path );
            return this;
        }

        public void unlock()
        {
            // Utils.logf( "GameLock.unlock(%s)", m_path );
            synchronized( s_locks ) {
                Assert.assertTrue( this == s_locks.get(m_path) );
                if ( 1 == m_lockCount ) {
                    s_locks.remove( m_path );
                } else {
                    Assert.assertTrue( !m_isForWrite );
                }
                --m_lockCount;
            }
            // Utils.logf( "GameLock.unlock(%s) done", m_path );
        }

        public String getPath() 
        {
            return m_path;
        }

        // used only for asserts
        public boolean canWrite()
        {
            return m_isForWrite && 1 == m_lockCount;
        }
    }

    private static Object s_syncObj = new Object();

    public static byte[] savedGame( Context context, String path )
    {
        GameLock lock = new GameLock( path, false ).lock();
        byte[] result = savedGame( context, lock );
        lock.unlock();
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
    public static void resetGame( Context context, GameLock lockSrc, 
                                  GameLock lockDest )
    {
        int gamePtr = XwJNI.initJNI();
        CurGameInfo gi = new CurGameInfo( context );
        CommsAddrRec addr = null;

        // loadMakeGame, if makinga new game, will add comms as long
        // as DeviceRole.SERVER_STANDALONE != gi.serverRole
        loadMakeGame( context, gamePtr, gi, lockSrc );
        byte[] dictBytes = GameUtils.openDict( context, gi.dictName );
        
        if ( XwJNI.game_hasComms( gamePtr ) ) {
            addr = new CommsAddrRec( context );
            XwJNI.comms_getAddr( gamePtr, addr );
            if ( CommsAddrRec.CommsConnType.COMMS_CONN_NONE == addr.conType ) {
                String relayName = CommonPrefs.getDefaultRelayHost( context );
                int relayPort = CommonPrefs.getDefaultRelayPort( context );
                XwJNI.comms_getInitialAddr( addr, relayName, relayPort );
            }
        }
        XwJNI.game_dispose( gamePtr );

        gi.setInProgress( false );

        gamePtr = XwJNI.initJNI();
        XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                CommonPrefs.get( context ), dictBytes, 
                                gi.dictName );
        if ( null != addr ) {
            XwJNI.comms_setAddr( gamePtr, addr );
        }

        saveGame( context, gamePtr, gi, lockDest, true );
        summarizeAndClose( context, lockDest, gamePtr, gi );
    } // resetGame

    public static void resetGame( Context context, String pathIn )
    {
        GameLock lock = new GameLock( pathIn, true )
            .lock();
        tellRelayDied( context, lock, true );
        resetGame( context, lock, lock );
        lock.unlock();
    }

    private static GameSummary summarizeAndClose( Context context, 
                                                  GameLock lock,
                                                  int gamePtr, CurGameInfo gi )
    {
        return summarizeAndClose( context, lock, gamePtr, gi, null );
    }

    private static GameSummary summarizeAndClose( Context context, 
                                                  GameLock lock,
                                                  int gamePtr, CurGameInfo gi,
                                                  FeedUtilsImpl feedImpl )
    {
        GameSummary summary = new GameSummary( gi );
        XwJNI.game_summarize( gamePtr, summary );

        if ( null != feedImpl ) {
            if ( feedImpl.m_gotChat ) {
                summary.pendingMsgLevel |= GameSummary.MSG_FLAGS_CHAT;
            } 
            if ( feedImpl.m_gotMsg ) {
                summary.pendingMsgLevel |= GameSummary.MSG_FLAGS_TURN;
            }
            if ( feedImpl.m_gameOver ) {
                summary.pendingMsgLevel |= GameSummary.MSG_FLAGS_GAMEOVER;
            }
        }

        DBUtils.saveSummary( context, lock, summary );

        XwJNI.game_dispose( gamePtr );
        return summary;
    }

    public static GameSummary summarize( Context context, GameLock lock )
    {
        int gamePtr = XwJNI.initJNI();
        CurGameInfo gi = new CurGameInfo( context );
        loadMakeGame( context, gamePtr, gi, lock );

        return summarizeAndClose( context, lock, gamePtr, gi );
    }

    public static String dupeGame( Context context, String pathIn )
    {
        GameLock lockSrc = new GameLock( pathIn, false ).lock();
        String newName = newName( context );
        GameLock lockDest = 
            new GameLock( newName, true ).lock();
        resetGame( context, lockSrc, lockDest );
        lockDest.unlock();
        lockSrc.unlock();
        return newName;
    }

    public static void deleteGame( Context context, String path, boolean informNow )
    {
        // does this need to be synchronized?
        GameLock lock = new GameLock( path, true );
        if ( lock.tryLock() ) {
            tellRelayDied( context, lock, informNow );
            DBUtils.deleteGame( context, lock );
            lock.unlock();
        }
    }

    public static void loadMakeGame( Context context, int gamePtr, 
                                     CurGameInfo gi, GameLock lock )
    {
        loadMakeGame( context, gamePtr, gi, null, lock );
    }

    public static void loadMakeGame( Context context, int gamePtr, 
                                     CurGameInfo gi, UtilCtxt util,
                                     GameLock lock )
    {
        byte[] stream = savedGame( context, lock );
        XwJNI.gi_from_stream( gi, stream );
        byte[] dictBytes = GameUtils.openDict( context, gi.dictName );

        boolean madeGame = XwJNI.game_makeFromStream( gamePtr, stream, 
                                                      JNIUtilsImpl.get(), gi, 
                                                      dictBytes, gi.dictName,
                                                      util, 
                                                      CommonPrefs.get(context));
        if ( !madeGame ) {
            XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                    CommonPrefs.get(context), dictBytes, 
                                    gi.dictName );
        }
    }

    public static void saveGame( Context context, int gamePtr, 
                                 CurGameInfo gi, GameLock lock,
                                 boolean setCreate )
    {
        byte[] stream = XwJNI.game_saveToStream( gamePtr, gi );
        saveGame( context, stream, lock, setCreate );
    }

    public static void saveGame( Context context, int gamePtr, 
                                 CurGameInfo gi )
    {
        String path = newName( context );
        GameLock lock = 
            new GameLock( path, true ).lock();
        saveGame( context, gamePtr, gi, lock, false );
        lock.unlock();
    }

    public static void saveGame( Context context, byte[] bytes, 
                                 GameLock lock, boolean setCreate )
    {
        DBUtils.saveGame( context, lock, bytes, setCreate );
    }

    public static GameLock saveGame( Context context, byte[] bytes )
    {
        String name = newName( context );
        GameLock lock = 
            new GameLock( name, true ).lock();
        saveGame( context, bytes, lock, false );
        return lock;
    }

    public static boolean gameDictHere( Context context, String path )
    {
        return gameDictHere( context, path, null, null );
    }

    public static boolean gameDictHere( Context context, String path, 
                                        String[] missingName, 
                                        int[] missingLang )
    {
        byte[] stream = savedGame( context, path );
        CurGameInfo gi = new CurGameInfo( context );
        XwJNI.gi_from_stream( gi, stream );
        String dictName = removeDictExtn( gi.dictName );
        if ( null != missingName ) {
            missingName[0] = dictName;
        }
        if ( null != missingLang ) {
            missingLang[0] = gi.dictLang;
        }

        boolean exists = false;
        for ( String name : dictList( context ) ) {
            if ( name.equals( dictName ) ){
                exists = true;
                break;
            }
        }
        return exists;
    }

    public static boolean gameDictHere( Context context, int indx, 
                                        String[] name, int[] lang )
    {
        String path = DBUtils.gamesList( context )[indx];
        return gameDictHere( context, path, name, lang );
    }

    public static String newName( Context context ) 
    {
        String name = null;
        Integer num = 1;
        int ii;
        String[] files = DBUtils.gamesList( context );
        String fmt = context.getString( R.string.gamef );

        while ( name == null ) {
            name = String.format( fmt + XWConstants.GAME_EXTN, num );
            for ( ii = 0; ii < files.length; ++ii ) {
                if ( files[ii].equals(name) ) {
                    ++num;
                    name = null;
                }
            }
        }
        return name;
    }

    public static String[] dictList( Context context )
    {
        ArrayList<String> al = new ArrayList<String>();

        for ( String file : getAssets( context ) ) {
            if ( isDict( file ) ) {
                al.add( removeDictExtn( file ) );
            }
        }

        for ( String file : context.fileList() ) {
            if ( isDict( file ) ) {
                al.add( removeDictExtn( file ) );
            }
        }

        return al.toArray( new String[al.size()] );
    }

    public static boolean dictExists( Context context, String name )
    {
        boolean exists = dictIsBuiltin( context, name );
        if ( !exists ) {
            name = addDictExtn( name );
            try {
                FileInputStream fis = context.openFileInput( name );
                fis.close();
                exists = true;
            } catch ( java.io.FileNotFoundException fnf ) {
            } catch ( java.io.IOException ioe ) {
            }
        }
        return exists;
    }

    public static boolean dictIsBuiltin( Context context, String name )
    {
        boolean builtin = false;
        name = addDictExtn( name );

        for ( String file : getAssets( context ) ) {
            if ( file.equals( name ) ) {
                builtin = true;
                break;
            }
        }

        return builtin;
    }

    public static void deleteDict( Context context, String name )
    {
        context.deleteFile( addDictExtn( name ) );
    }

    public static byte[] openDict( Context context, String name )
    {
        byte[] bytes = null;

        name = addDictExtn( name );

        try {
            AssetManager am = context.getAssets();
            InputStream dict = am.open( name, 
                            android.content.res.AssetManager.ACCESS_RANDOM );

            int len = dict.available(); // this may not be the full length!
            bytes = new byte[len];
            int nRead = dict.read( bytes, 0, len );
            if ( nRead != len ) {
                Utils.logf( "**** warning ****; read only " + nRead + " of " 
                            + len + " bytes." );
            }
            // check that with len bytes we've read the whole file
            Assert.assertTrue( -1 == dict.read() );
        } catch ( java.io.IOException ee ){
            Utils.logf( "%s failed to open; likely not built-in", name );
        }

        // not an asset?  Try storage
        if ( null == bytes ) {
            try {
                FileInputStream fis = context.openFileInput( name );
                int len = (int)fis.getChannel().size();
                bytes = new byte[len];
                fis.read( bytes, 0, len );
                fis.close();
            } catch ( java.io.FileNotFoundException fnf ) {
                Utils.logf( fnf.toString() );
            } catch ( java.io.IOException ioe ) {
                Utils.logf( ioe.toString() );
            }
        }
        
        return bytes;
    }

    public static boolean saveDict( Context context, String name, InputStream in )
    {
        boolean success = false;
        try {
            FileOutputStream fos = context.openFileOutput( name,
                                                           Context.MODE_PRIVATE );
            byte[] buf = new byte[1024];
            int nRead;
            while( 0 <= (nRead = in.read( buf, 0, buf.length )) ) {
                fos.write( buf, 0, nRead );
            }
            fos.close();
            success = true;
        } catch ( java.io.FileNotFoundException fnf ) {
            Utils.logf( "saveDict: FileNotFoundException: %s", fnf.toString() );
        } catch ( java.io.IOException ioe ) {
            Utils.logf( "saveDict: IOException: %s", ioe.toString() );
            deleteDict( context, name );
        }
        return success;
    } 

    private static boolean isGame( String file )
    {
        return file.endsWith( XWConstants.GAME_EXTN );
    }

    private static boolean isDict( String file )
    {
        return file.endsWith( XWConstants.DICT_EXTN );
    }

    public static String gameName( Context context, String path )
    {
        return path.substring( 0, path.lastIndexOf( XWConstants.GAME_EXTN ) );
    }

    public static void launchGame( Activity activity, String path )
    {
        File file = new File( path );
        Uri uri = Uri.fromFile( file );
        Intent intent = new Intent( Intent.ACTION_EDIT, uri,
                                    activity, BoardActivity.class );
        activity.startActivity( intent );
    }

    public static void launchGameAndFinish( Activity activity, String path )
    {
        launchGame( activity, path );
        activity.finish();
    }

    private static class FeedUtilsImpl extends UtilCtxtImpl {
        private Context m_context;
        private String m_path;
        public boolean m_gotMsg;
        public boolean m_gotChat;
        public boolean m_gameOver;

        public FeedUtilsImpl( Context context, String path )
        {
            m_context = context;
            m_path = path;
            m_gotMsg = false;
            m_gameOver = false;
        }
        public void showChat( String msg )
        {
            DBUtils.appendChatHistory( m_context, m_path, msg, false );
            m_gotChat = true;
        }
        public void turnChanged()
        {
            m_gotMsg = true;
        }

        public void notifyGameOver()
        {
            m_gameOver = true;
        }
    }

    public static boolean feedMessages( Context context, String relayID,
                                        byte[][] msgs )
    {
        boolean draw = false;
        String path = DBUtils.getPathFor( context, relayID );
        if ( null != path ) {
            int gamePtr = XwJNI.initJNI();
            CurGameInfo gi = new CurGameInfo( context );
            FeedUtilsImpl feedImpl = new FeedUtilsImpl( context, path );
            GameLock lock = new GameLock( path, true );
            if ( lock.tryLock() ) {
                loadMakeGame( context, gamePtr, gi, feedImpl, lock );

                for ( byte[] msg : msgs ) {
                    draw = XwJNI.game_receiveMessage( gamePtr, msg ) || draw;
                }

                // update gi to reflect changes due to messages
                XwJNI.game_getGi( gamePtr, gi );
                saveGame( context, gamePtr, gi, lock, false );
                summarizeAndClose( context, lock, gamePtr, gi, feedImpl );

                int flags = GameSummary.MSG_FLAGS_NONE;
                if ( feedImpl.m_gotChat ) {
                    flags |= GameSummary.MSG_FLAGS_CHAT;
                } 
                if ( feedImpl.m_gotMsg ) {
                    flags |= GameSummary.MSG_FLAGS_TURN;
                }
                if ( feedImpl.m_gameOver ) {
                    flags |= GameSummary.MSG_FLAGS_GAMEOVER;
                }
                if ( GameSummary.MSG_FLAGS_NONE != flags ) {
                    draw = true;
                    DBUtils.setMsgFlags( path, flags );
                }
                lock.unlock();
            }
        }
        Utils.logf( "feedMessages=>%s", draw?"true":"false" );
        return draw;
    }

    // This *must* involve a reset if the language is changing!!!
    // Which isn't possible right now, so make sure the old and new
    // dict have the same langauge code.
    public static void replaceDict( Context context, String path,
                                    String dict )
    {
        GameLock lock = new GameLock( path, true ).lock();
        byte[] stream = savedGame( context, lock );
        CurGameInfo gi = new CurGameInfo( context );
        byte[] dictBytes = GameUtils.openDict( context, dict );

        int gamePtr = XwJNI.initJNI();
        XwJNI.game_makeFromStream( gamePtr, stream, 
                                   JNIUtilsImpl.get(), gi,
                                   dictBytes, dict,         
                                   CommonPrefs.get( context ) );
        gi.dictName = dict;

        saveGame( context, gamePtr, gi, lock, false );

        summarizeAndClose( context, lock, gamePtr, gi );

        lock.unlock();
    }

    public static void applyChanges( Context context, CurGameInfo gi, 
                                     CommsAddrRec car, GameLock lock,
                                     boolean forceNew )
    {
        // This should be a separate function, commitChanges() or
        // somesuch.  But: do we have a way to save changes to a gi
        // that don't reset the game, e.g. player name for standalone
        // games?
        byte[] dictBytes = GameUtils.openDict( context, gi.dictName );
        int gamePtr = XwJNI.initJNI();
        boolean madeGame = false;
        CommonPrefs cp = CommonPrefs.get( context );

        if ( forceNew ) {
            tellRelayDied( context, lock, true );
        } else {
            byte[] stream = GameUtils.savedGame( context, lock );
            // Will fail if there's nothing in the stream but a gi.
            madeGame = XwJNI.game_makeFromStream( gamePtr, stream, 
                                                  JNIUtilsImpl.get(),
                                                  new CurGameInfo(context), 
                                                  dictBytes, gi.dictName, cp );
        }

        if ( forceNew || !madeGame ) {
            gi.setInProgress( false );
            XwJNI.game_makeNewGame( gamePtr, gi, JNIUtilsImpl.get(), 
                                    cp, dictBytes, gi.dictName );
        }

        if ( null != car ) {
            XwJNI.comms_setAddr( gamePtr, car );
        }

        saveGame( context, gamePtr, gi, lock, false );

        GameSummary summary = new GameSummary( gi );
        XwJNI.game_summarize( gamePtr, summary );
        DBUtils.saveSummary( context, lock, summary );

        XwJNI.game_dispose( gamePtr );
    } // applyChanges

    public static void doConfig( Activity activity, String path, Class clazz )
    {
        Uri uri = Uri.fromFile( new File(path) );
        Intent intent = new Intent( Intent.ACTION_EDIT, uri, activity, clazz );
        activity.startActivity( intent );
    }

    public static String removeDictExtn( String str )
    {
        if ( str.endsWith( XWConstants.DICT_EXTN ) ) {
            int indx = str.lastIndexOf( XWConstants.DICT_EXTN );
            str = str.substring( 0, indx );
        }
        return str;
    }

    private static String addDictExtn( String str ) 
    {
        if ( ! str.endsWith( XWConstants.DICT_EXTN ) ) {
            str += XWConstants.DICT_EXTN;
        }
        return str;
    }

    private static String[] getAssets( Context context )
    {
        try {
            AssetManager am = context.getAssets();
            return am.list("");
        } catch( java.io.IOException ioe ) {
            Utils.logf( ioe.toString() );
            return new String[0];
        }
    }
    
    private static void tellRelayDied( Context context, GameLock lock,
                                       boolean informNow )
    {
        GameSummary summary = DBUtils.getSummary( context, lock );
        if ( null != summary.relayID ) {
            DBUtils.addDeceased( context, summary.relayID, summary.seed );
            if ( informNow ) {
                NetUtils.informOfDeaths( context );
            }
        }
    }

}

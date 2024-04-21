/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.os.Handler;

import java.util.Formatter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

import androidx.annotation.NonNull;

// Implements read-locks and write-locks per game.  A read lock is
// obtainable when other read locks are granted but not when a
// write lock is.  Write-locks are exclusive.
//
// Let's try representing a lock with something serializable. Let's not have
// one public object per game, but rather let lots of objects represent the
// same state. That way a lock can be grabbed by one thread or object (think
// GamesListDelegate) and held for as long as it takes the game that's opened
// to be closed. During that time it can be shared among objects
// (BoardDelegate and JNIThread, etc.) that know how to manage their
// interactions. (Note that I'm not doing this now -- found a way around it --
// but that the capability is still worth having, and so the change is going
// in. Having getFor() be public, and there being GameLock instances out there
// whose state was "unlocked", was just dumb.)
//
// So the class everybody sees (GameLock) is not stored. The rowid it
// holds is a key to a private Hash of state.

public class GameLock implements AutoCloseable {
    private static final String TAG = GameLock.class.getSimpleName();

    private static final boolean GET_OWNER_STACK = BuildConfig.NON_RELEASE;
    private static final boolean DEBUG_LOCKS = false;

    // private static final long ASSERT_TIME = 2000;
    private static final long THROW_TIME = 1000;
    final private long m_rowid;
    private Owner m_owner;
    final private GameLockState m_state;

    private static class Owner {
        Thread mThread;
        String mTrace;
        long mStamp;

        Owner()
        {
            mThread = Thread.currentThread();
            if ( GET_OWNER_STACK ) {
                mTrace = android.util.Log.getStackTraceString(new Exception());
            } else {
                mTrace = "<untracked>";
            }
            setStamp();
        }

        @Override
        public String toString()
        {
            long ageMS = System.currentTimeMillis() - mStamp;
            return String.format( "Owner: {age: %dms (since %d); thread: {%s}; stack: {%s}}",
                                  ageMS, mStamp, mThread, mTrace );
        }

        void setStamp() { mStamp = System.currentTimeMillis(); }
    }

    private static class GameLockState {
        long mRowid;
        private Set<Owner> mOwners = new HashSet<>();
        private boolean mReadOnly;

        GameLockState( long rowid ) { mRowid = rowid; }

        private void add( Owner owner )
        {
            synchronized( mOwners ) {
                Assert.assertFalse( mOwners.contains( owner ) );
                mOwners.add( owner );
            }
        }

        private void remove( Owner owner )
        {
            synchronized ( mOwners ) {
                Assert.assertTrue( mOwners.contains( owner ) );
                mOwners.remove( owner );

                if ( DEBUG_LOCKS ) {
                    Log.d( TAG, "remove(): %d owners left", mOwners.size() );
                }

                if ( 0 == mOwners.size() ) {
                    mOwners.notifyAll();
                }
            }
        }

        // We grant a lock IFF:
        // * Count is 0
        // OR
        // * existing locks are ReadOnly and this request is readOnly
        // OR
        // * the requesting thread already holds the lock (later...)
        // // This could be written to allow multiple read locks.  Let's
        // // see if not doing that causes problems.

        private GameLock tryLockImpl( boolean readOnly )
        {
            GameLock result = null;
            synchronized ( mOwners ) {
                if ( DEBUG_LOCKS ) {
                    Log.d( TAG, "%s.tryLockImpl(ro=%b)", this, readOnly );
                }
                // Thread thisThread = Thread.currentThread();
                boolean grant = false;
                if ( 0 == mOwners.size() ) {
                    grant = true;
                } else if ( mReadOnly && readOnly ) {
                    grant = true;
                    // } else if ( thisThread == mOwnerThread ) {
                    //     grant = true;
                }

                if ( grant ) {
                    mReadOnly = readOnly;
                    result = new GameLock( this, mRowid );
                }
            }
            return result;
        }

        private GameLock tryLockRO()
        {
            GameLock result = tryLockImpl( true );
            logIfNull( result, "tryLockRO()" );
            return result;
        }

        private GameLock lockImpl( long timeoutMS, boolean readOnly ) throws InterruptedException
        {
            GameLock result = null;
            long startMS = System.currentTimeMillis();
            long endMS = startMS + timeoutMS;
            synchronized ( mOwners ) {
                for ( ; ; ) {
                    result = tryLockImpl( readOnly );
                    if ( null != result ) {
                        break;
                    }
                    long now = System.currentTimeMillis();
                    if ( now >= endMS ) {
                        throw new GameLockedException();
                    }
                    mOwners.wait( endMS - now );
                }
            }

            if ( DEBUG_LOCKS ) {
                long tookMS = System.currentTimeMillis() - startMS;
                Log.d( TAG, "%s.lockImpl() returning after %d ms", this, tookMS );
            }
            return result;
        }

        // Version that's allowed to return null -- if maxMillis > 0
        private GameLock lock( long maxMillis ) throws GameLockedException
        {
            Assert.assertTrue( maxMillis <= THROW_TIME );
            GameLock result = null;
            try {
                result = lockImpl( maxMillis, false );
            } catch (InterruptedException ex) {
                Log.d( TAG, "lock(): got %s", ex.getMessage() );
            }
            if ( DEBUG_LOCKS ) {
                Log.d( TAG, "%s.lock(%d) => %s", this, maxMillis, result );
            }
            logIfNull( result, "lock(maxMillis=%d)", maxMillis );
            return result;
        }

        private GameLock tryLock()
        {
            GameLock result = tryLockImpl( false );
            logIfNull( result, "tryLock()" );
            return result;
        }

        public GameLock lock() throws InterruptedException
        {
            if ( BuildConfig.DEBUG ) {
                DbgUtils.assertOnUIThread( false );
            }
            GameLock result = lockImpl( Long.MAX_VALUE, false );
            Assert.assertNotNull( result );
            return result;
        }

        private GameLock lockRO( long maxMillis )
        {
            Assert.assertTrue( maxMillis <= THROW_TIME );
            GameLock lock = null;
            try {
                lock = lockImpl( maxMillis, true );
            } catch ( InterruptedException ex ) {
            }

            logIfNull( lock, "lockRO(maxMillis=%d)", maxMillis );
            return lock;
        }

        private void unlock( Owner owner )
        {
            if ( DEBUG_LOCKS ) {
                Log.d( TAG, "%s.unlock()", this );
            }
            remove( owner );
            if ( DEBUG_LOCKS ) {
                Log.d( TAG, "%s.unlock() DONE", this );
            }
        }

        private boolean canWrite()
        {
            boolean result = !mReadOnly; // && 1 == mLockCount[0];
            if ( !result ) {
                Log.w( TAG, "%s.canWrite(): => false", this );
            }
            return result;
        }

        @Override
        public String toString()
        {
            return String.format("{this: %H; rowid: %d; count: %d; ro: %b}",
                                 this, mRowid, mOwners.size(), mReadOnly);
        }

        private void logIfNull( GameLock result, String fmt, Object... args )
        {
            Assert.assertVarargsNotNullNR(args);
            if ( BuildConfig.DEBUG && null == result ) {
                String func = new Formatter().format( fmt, args ).toString();
                Log.d( TAG, "%s.%s => null", this, func );

                final long now = System.currentTimeMillis();
                long minStamp = now;
                for ( Iterator<Owner> iter = mOwners.iterator(); iter.hasNext(); ) {
                    Owner owner = iter.next();
                    long stamp = owner.mStamp;
                    if ( stamp < minStamp ) {
                        minStamp = stamp;
                    }
                }

                Log.d( TAG, "Unable to lock; would-be owner: %s; %s",
                       new Owner(), getHolderDump(mRowid) );

                long heldMS = now - minStamp;
                if ( heldMS > (60 * 1000) ) { // 1 minute's a long time
                    DbgUtils.showf( "GameLock: logged owner held for %d seconds!", heldMS / 1000 );
                }
            }
        }
    }

    public static class GameLockedException extends RuntimeException {}

    private static Map<Long, GameLockState> sLockMap = new HashMap<>();
    private static GameLockState getFor( long rowid )
    {
        GameLockState result = null;
        synchronized ( sLockMap ) {
            if ( sLockMap.containsKey( rowid ) ) {
                result = sLockMap.get( rowid );
            }
            if ( null == result ) {
                result = new GameLockState( rowid );
                sLockMap.put( rowid, result );
            }
        }
        return result;
    }

    private GameLock( GameLockState state, Owner owner, long rowid )
    {
        m_state = state;
        m_owner = owner;
        m_rowid = rowid;

        m_state.add( owner );
    }

    private GameLock( GameLockState state, long rowid )
    {
        this( state, new Owner(), rowid );
    }

    private void setOwner( Owner owner )
    {
        m_state.add( owner );   // first so doesn't drop to 0
        m_state.remove( m_owner );
        m_owner = owner;
        owner.setStamp();
    }

    public static GameLock tryLock( long rowid )
    {
        return getFor( rowid ).tryLock();
    }

    public static GameLock tryLockRO(long rowid)
    {
        return getFor( rowid ).tryLockRO();
    }

    @NonNull
    public static GameLock lock(long rowid) throws InterruptedException
    {
        return getFor( rowid ).lock();
    }

    public static GameLock lock( long rowid, long maxMillis ) throws GameLockedException
    {
        return getFor( rowid ).lock( maxMillis );
    }

    public static GameLock lockRO( long rowid, long maxMillis ) throws GameLockedException
    {
        return getFor( rowid ).lockRO( maxMillis );
    }

    public void release()
    {
        m_state.unlock( m_owner );
    }

    public GameLock retain()
    {
        return new GameLock( m_state, m_rowid );
    }

    @Override
    public void close()
    {
        release();
    }
    
    public long getRowid()
    {
        return m_rowid;
    }

    public interface GotLockProc {
        public void gotLock( GameLock lock );
    }

    // Meant to be called from UI thread, returning immediately, but when it
    // gets the lock, or time runs out, calls the callback (using the Handler
    // passed in) with the lock or null.
    public static void getLockThen( final long rowid,
                                    final long maxMillis,
                                    final Handler handler,
                                    final GotLockProc proc )
    {
        // capture caller thread and stack
        final Owner owner = new Owner();

        new Thread( new Runnable() {
                @Override
                public void run() {
                    GameLock lock = null;
                    if ( false && 0 == Utils.nextRandomInt() % 5 ) {
                        Log.d( TAG, "testing return-null case" );
                        try {
                            Thread.sleep( maxMillis );
                        } catch ( Exception ex) {}
                    } else {
                        try {
                            lock = getFor( rowid ).lockImpl( maxMillis, false );
                            owner.setStamp();
                            lock.setOwner( owner );
                        } catch ( GameLockedException | InterruptedException gle ) {}
                    }

                    final GameLock fLock = lock;
                    handler.post( new Runnable() {
                            @Override
                            public void run() {
                                proc.gotLock( fLock );
                            }
                        } );
                }
            } ).start();
    }

    public static String getHolderDump( long rowid )
    {
        String result;
        GameLockState state = getFor( rowid );
        synchronized ( state.mOwners ) {
            result = String.format( "Showing %d owners: ", state.mOwners.size() );
            for ( Iterator<Owner> iter = state.mOwners.iterator(); iter.hasNext(); ) {
                Owner owner = iter.next();
                result += owner.toString();
            }
        }
        return result;
    }

    // used only for asserts
    public boolean canWrite()
    {
        return m_state.canWrite();
    }
}

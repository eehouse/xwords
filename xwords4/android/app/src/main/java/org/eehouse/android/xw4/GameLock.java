/* -*- compile-command: "find-and-gradle.sh insXw4Deb"; -*- */
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

import java.io.Serializable;
import java.lang.ref.WeakReference;
import java.util.Formatter;
import java.util.HashMap;
import java.util.Map;
import java.util.Stack;

import android.support.annotation.NonNull;

// Implements read-locks and write-locks per game.  A read lock is
// obtainable when other read locks are granted but not when a
// write lock is.  Write-locks are exclusive.
public class GameLock implements AutoCloseable {
    private static final String TAG = GameLock.class.getSimpleName();

    private static final boolean DEBUG_LOCKS = false;
    // private static final long ASSERT_TIME = 2000;
    private static final long THROW_TIME = 1000;
    private long m_rowid;
    private Stack<Owner> mOwners = new Stack<>();
    private boolean mReadOnly;

    private static class Owner {
        Thread mThread;
        String mTrace;

        Owner()
        {
            mThread = Thread.currentThread();
            // mTrace = mThread.getStackTrace();
            mTrace = android.util.Log.getStackTraceString(new Exception());
        }

        @Override
        public String toString()
        {
            return String.format( "Owner: {%s/%s}", mThread, mTrace );
        }
    }

    @Override
    public String toString()
    {
        return String.format("{this: %H; rowid: %d; count: %d; ro: %b}",
                             this, m_rowid, mOwners.size(), mReadOnly);
    }

    public static class GameLockedException extends RuntimeException {}

    private static Map<Long, WeakReference<GameLock>> sLockMap = new HashMap<>();
    public static GameLock getFor( long rowid )
    {
        GameLock result = null;
        synchronized ( sLockMap ) {
            if ( sLockMap.containsKey( rowid ) ) {
                result = sLockMap.get( rowid ).get();
            }
            if ( null == result ) {
                result = new GameLock( rowid );
                sLockMap.put( rowid, new WeakReference(result) );
            }
        }
        return result;
    }

    private GameLock( long rowid ) { m_rowid = rowid; }

    // We grant a lock IFF:
    // * Count is 0
    // OR
    // * existing locks are ReadOnly and this request is readOnly
    // OR
    // * the requesting thread already holds the lock (later...)
    private GameLock tryLockImpl( boolean readOnly )
    {
        GameLock result = null;
        synchronized ( mOwners ) {
            if ( DEBUG_LOCKS ) {
                Log.d( TAG, "%s.tryLockImpl(ro=%b)", this, readOnly );
            }
            // Thread thisThread = Thread.currentThread();
            boolean grant = false;
            if ( mOwners.empty() ) {
                grant = true;
            } else if ( mReadOnly && readOnly ) {
                grant = true;
            // } else if ( thisThread == mOwnerThread ) {
            //     grant = true;
            }

            if ( grant ) {
                mOwners.push( new Owner() );
                mReadOnly = readOnly;
                result = this;
            }
        }
        return result;
    }
    
    // // This could be written to allow multiple read locks.  Let's
    // // see if not doing that causes problems.
    public GameLock tryLock()
    {
        GameLock result = tryLockImpl( false );
        logIfNull( result, "tryLock()" );
        return result;
    }

    public GameLock tryLockRO()
    {
        GameLock result = tryLockImpl( true );
        logIfNull( result, "tryLockRO()" );
        return result;
    }

    private GameLock lockImpl( long timeoutMS, boolean readOnly ) throws InterruptedException
    {
        long startMS = System.currentTimeMillis();
        long endMS = startMS + timeoutMS;
        synchronized ( mOwners ) {
            while ( null == tryLockImpl( readOnly ) ) {
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
        return this;
    }

    @NonNull
    public GameLock lock() throws InterruptedException
    {
        if ( BuildConfig.DEBUG ) {
            DbgUtils.assertOnUIThread( false );
        }
        GameLock result = lockImpl( Long.MAX_VALUE, false );
        Assert.assertNotNull( result );
        return result;
    }

    // Version that's allowed to return null -- if maxMillis > 0
    public GameLock lock( long maxMillis ) throws GameLockedException
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

    public GameLock lockRO( long maxMillis )
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

    public void unlock()
    {
        synchronized ( mOwners ) {
            if ( DEBUG_LOCKS ) {
                Log.d( TAG, "%s.unlock()", this );
            }
            Thread oldThread = mOwners.pop().mThread;

            // It's ok for different threads to hold the same RO lock
            if ( !mReadOnly && oldThread != Thread.currentThread() ) {
                Log.e( TAG, "unlock(): unequal threads: %s => %s", oldThread,
                       Thread.currentThread() );
                Assert.fail();
            }
            
            if ( mOwners.empty() ) {
                mOwners.notifyAll();
            }
            if ( DEBUG_LOCKS ) {
                Log.d( TAG, "%s.unlock() DONE", this );
            }
        }
    }

    @Override
    public void close()
    {
        unlock();
    }
    
    public long getRowid()
    {
        return m_rowid;
    }

    // used only for asserts
    public boolean canWrite()
    {
        boolean result = !mReadOnly; // && 1 == mLockCount[0];
        if ( !result ) {
            Log.w( TAG, "%s.canWrite(): => false", this );
        }
        return result;
    }

    private void logIfNull( GameLock result, String fmt, Object... args )
    {
        if ( DEBUG_LOCKS && null == result ) {
            String func = new Formatter().format( fmt, args ).toString();
            Log.d( TAG, "%s.%s => null", this, func );
            Log.d( TAG, "Unable to lock; cur owner: %s; would-be owner: %s",
                       mOwners.peek(), new Owner() );
        }
    }
}

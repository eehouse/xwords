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

import java.util.HashMap;

import junit.framework.Assert;

// Implements read-locks and write-locks per game.  A read lock is
// obtainable when other read locks are granted but not when a
// write lock is.  Write-locks are exclusive.
public class GameLock {
    private long m_rowid;
    private boolean m_isForWrite;
    private int m_lockCount;
    StackTraceElement[] m_lockTrace;

    private static HashMap<Long, GameLock> 
        s_locks = new HashMap<Long,GameLock>();

    public GameLock( long rowid, boolean isForWrite ) 
    {
        m_rowid = rowid;
        m_isForWrite = isForWrite;
        m_lockCount = 0;
        if ( XWApp.DEBUG_LOCKS ) {
            DbgUtils.logf( "GameLock.GameLock(rowid:%d,isForWrite:%b)=>"
                           + "this: %H", rowid, isForWrite, this );
            DbgUtils.printStack();
        }
    }

    // This could be written to allow multiple read locks.  Let's
    // see if not doing that causes problems.
    public boolean tryLock()
    {
        boolean gotIt = false;
        synchronized( s_locks ) {
            GameLock owner = s_locks.get( m_rowid );
            if ( null == owner ) { // unowned
                Assert.assertTrue( 0 == m_lockCount );
                s_locks.put( m_rowid, this );
                ++m_lockCount;
                gotIt = true;
                    
                if ( XWApp.DEBUG_LOCKS ) {
                    StackTraceElement[] trace = Thread.currentThread().
                        getStackTrace();
                    m_lockTrace = new StackTraceElement[trace.length];
                    System.arraycopy( trace, 0, m_lockTrace, 0, trace.length );
                }
            } else if ( this == owner && ! m_isForWrite ) {
                Assert.assertTrue( 0 == m_lockCount );
                ++m_lockCount;
                gotIt = true;
            }
        }
        return gotIt;
    }
        
    // Wait forever (but may assert if too long)
    public GameLock lock()
    {
        return this.lock( 0 );
    }

    // Version that's allowed to return null -- if maxMillis > 0
    public GameLock lock( long maxMillis )
    {
        GameLock result = null;
        final long assertTime = 2000;
        Assert.assertTrue( maxMillis < assertTime );
        long sleptTime = 0;

        if ( XWApp.DEBUG_LOCKS ) {
            DbgUtils.logf( "lock %H (rowid:%d, maxMillis=%d)", this, m_rowid, maxMillis );
        }

        for ( ; ; ) {
            if ( tryLock() ) {
                result = this;
                break;
            }
            if ( XWApp.DEBUG_LOCKS ) {
                DbgUtils.logf( "GameLock.lock() %H failed; sleeping", this );
                DbgUtils.printStack();
            }
            try {
                Thread.sleep( 25 ); // milliseconds
                sleptTime += 25;
            } catch( InterruptedException ie ) {
                DbgUtils.loge( ie );
                break;
            }

            if ( XWApp.DEBUG_LOCKS ) {
                DbgUtils.logf( "GameLock.lock() %H awake; "
                               + "sleptTime now %d millis", this, sleptTime );
            }

            if ( 0 < maxMillis && sleptTime >= maxMillis ) {
                break;
            } else if ( sleptTime >= assertTime ) {
                if ( XWApp.DEBUG_LOCKS ) {
                    DbgUtils.logf( "lock %H overlocked. lock holding stack:", 
                                   this );
                    DbgUtils.printStack( m_lockTrace );
                    DbgUtils.logf( "lock %H seeking stack:", this );
                    DbgUtils.printStack();
                }
                Assert.fail();
            }
        }
        // DbgUtils.logf( "GameLock.lock(%s) done", m_path );
        return result;
    }

    public void unlock()
    {
        // DbgUtils.logf( "GameLock.unlock(%s)", m_path );
        synchronized( s_locks ) {
            Assert.assertTrue( this == s_locks.get(m_rowid) );
            if ( 1 == m_lockCount ) {
                s_locks.remove( m_rowid );
            } else {
                Assert.assertTrue( !m_isForWrite );
            }
            --m_lockCount;

            if ( XWApp.DEBUG_LOCKS ) {
                DbgUtils.logf( "GameLock.unlock: this: %H (rowid:%d) unlocked", 
                               this, m_rowid );
            }
        }
    }

    public long getRowid() 
    {
        return m_rowid;
    }

    // used only for asserts
    public boolean canWrite()
    {
        return m_isForWrite && 1 == m_lockCount;
    }
}

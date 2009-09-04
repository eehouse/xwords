/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <assert.h>
#include "timermgr.h"
#include "xwrelay_priv.h"
#include "configs.h"
#include "mlock.h"

TimerMgr::TimerMgr()
    : m_nextFireTime(0)
{
    pthread_mutex_init( &m_timersMutex, NULL );
}

/* static */TimerMgr* 
TimerMgr::GetTimerMgr()
{
    static TimerMgr* mgr = NULL;
    if ( mgr == NULL ) {
        mgr = new TimerMgr();
    }
    return mgr;
}

void
TimerMgr::SetTimer( time_t inMillis, TimerProc proc, void* closure,
                    int interval )
{
    logf( XW_LOGINFO, "%s: uptime = %ld", __func__, uptime() );
    TimerInfo ti;
    ti.proc = proc;
    ti.closure = closure;
    ti.when = uptime() + inMillis;
    ti.interval = interval;

    MutexLock ml( &m_timersMutex );

    if ( getTimer( proc, closure ) ) {
        clearTimerImpl( proc, closure );
    }

    m_timers.push_back( ti );

    figureNextFire();
    logf( XW_LOGINFO, "setTimer done" );
}

time_t 
TimerMgr::GetPollTimeout()
{
    MutexLock ml( &m_timersMutex );

    time_t tout = m_nextFireTime;
    if ( tout == 0 ) {
        tout = -1;
    } else {
        tout -= uptime();
        if ( tout < 0 ) {
            tout = 0;
        }
        tout *= 1000;
    }
    return tout;
} /* GetPollTimeout */

bool
TimerMgr::getTimer( TimerProc proc, void* closure )
{
    /* Don't call this unless have the lock!!! */
    list<TimerInfo>::iterator iter;
    for ( iter = m_timers.begin(); iter != m_timers.end(); ++iter ) {
        if ( (*iter).proc == proc
             && (*iter).closure == closure ) {
            return true;
        }
    }

    return false;
} /* getTimer */

void
TimerMgr::figureNextFire()
{
    /* Don't call this unless have the lock!!! */
    time_t t = 0x7FFFFFFF;
    time_t cur = uptime();

    list<TimerInfo>::iterator iter = m_timers.begin();

    while ( iter != m_timers.end() ) {
        time_t when = iter->when;

        if ( when == 0 ) {
            if ( iter->interval ) {
                when = iter->when = cur + iter->interval;
            } else {
                m_timers.erase(iter);
                iter = m_timers.begin();
                continue;
            }
        }

        if ( when < t ) {
            t = when;
        }
        ++iter;
    }

    m_nextFireTime = t == 0x7FFFFFFF? 0 : t;
} /* figureNextFire */

void
TimerMgr::ClearTimer( TimerProc proc, void* closure )
{
    MutexLock ml( &m_timersMutex );
    clearTimerImpl( proc, closure );
}

void
TimerMgr::FireElapsedTimers()
{
    time_t curTime = uptime();

    vector<TimerProc> procs;
    vector<void*> closures;
    {
        MutexLock ml( &m_timersMutex );
        /* loop until we get through without firing a single one.  Only fire one
           per run, though, since calling erase invalidates the iterator.
           PENDING: all this needs a mutex!!!! */
        list<TimerInfo>::iterator iter;
        for ( iter = m_timers.begin(); iter != m_timers.end(); ++iter ) {
            TimerInfo* tip = &(*iter);
            if ( tip->when <= curTime ) {

                procs.push_back(tip->proc);
                closures.push_back(tip->closure);

                if ( tip->interval ) {
                    tip->when += tip->interval;
                } else {
                    tip->when = 0;      /* flag for removal */
                }
            }
        }
    }

    vector<TimerProc>::iterator iter1 = procs.begin();
    vector<void*>::iterator iter2 = closures.begin();
    while ( iter1 != procs.end() ) {
        (*iter1++)(*iter2++);
    }

    MutexLock ml( &m_timersMutex );
    figureNextFire();
} /* fireElapsedTimers */

void
TimerMgr::clearTimerImpl( TimerProc proc, void* closure )
{
    list<TimerInfo>::iterator iter;
    for ( iter = m_timers.begin(); iter != m_timers.end(); ++iter ) {
        TimerInfo* tip = &(*iter);
        if ( tip->proc == proc && tip->closure == closure ) {
            m_timers.erase(iter);
            break;
        }
    }
}

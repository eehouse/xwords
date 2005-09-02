/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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

#ifndef _TIMERMGR_H_
#define _TIMERMGR_H_

#include "list"

#include "xwrelay_priv.h"

using namespace std;

typedef void (*TimerProc)( void* closure );


class TimerMgr {

 public:
  static TimerMgr* getTimerMgr();

  void setTimer( time_t inMillis, TimerProc proc, void* closure,
                 int interval ); /* 0 means non-recurring */
  void clearTimer( TimerProc proc, void* closure );
  
  time_t getPollTimeout();
  void fireElapsedTimers();

 private:

  typedef struct {
      TimerProc proc;
      void* closure;
      time_t when;
      int interval;
  } TimerInfo;
  

  TimerMgr();
  static void sighandler( int signal );

  /* run once we have the mutex */
  void clearTimerImpl( TimerProc proc, void* closure );
  int getTimer( TimerProc proc, void* closure );
  void figureNextFire();
  
  pthread_mutex_t m_timersMutex;
  list<TimerInfo> m_timers;

  time_t m_nextFireTime;
  time_t m_heartbeat;
};

#endif

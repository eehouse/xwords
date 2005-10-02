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

#ifndef _CONFIGS_H_
#define _CONFIGS_H_

#include <string>
#include "xwrelay_priv.h"

class RelayConfigs {

 public:
    static void InitConfigs( const char* confFile );
    static RelayConfigs* GetConfigs();

    ~RelayConfigs() {}

    int    GetPort() { return m_port; }
    int    GetCtrlPort() { return m_ctrlport; }
    int    GetNWorkerThreads()  { return m_nWorkerThreads; }
    time_t GetAllConnectedInterval() { return m_allConnInterval; }
    time_t GetHeartbeatInterval() { return m_heartbeatInterval; }
    const char*  GetServerName() { return m_serverName.c_str(); }


 private:
    RelayConfigs( const char* cfile );
    void parse( const char* fname );

    time_t m_allConnInterval;
    time_t m_heartbeatInterval;
    int m_ctrlport;
    int m_port;
    int m_nWorkerThreads;
    std::string m_serverName;

    static RelayConfigs* instance;
};


#endif

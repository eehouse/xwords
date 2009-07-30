/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "xwrelay_priv.h"

using namespace std;

class RelayConfigs {

 public:
    static void InitConfigs( const char* confFile );
    static RelayConfigs* GetConfigs();

    ~RelayConfigs() {}

    bool GetValueFor( const char* key, int* intValue );
    bool GetValueFor( const char* key, time_t* timeValue );
    bool GetValueFor( const char* key, vector<int>& ints );
    bool GetValueFor( const char* key, char* buf, int len );

    void SetValueFor( const char* key, const char* value );

 private:
    RelayConfigs( const char* cfile );
    ino_t parse( const char* fname, ino_t prev );

    time_t m_allConnInterval;

    map<const char*,const char*,bool (*)( const char*, const char* )>m_values;
    pthread_mutex_t m_values_mutex;

    static RelayConfigs* instance;
};


#endif

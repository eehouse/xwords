/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4.loc;

import java.util.ArrayList;
import android.content.Context;

import org.eehouse.android.xw4.DbgUtils;

/** This class provides filtering on the list of translations, first by
 * built-in filters like "recent menu", and then by a search string.  Either
 * can be changed independently of the other.
 *
 */

public class LocSearcher {

    private String m_contextName;

    private interface FilterFunc {
        public boolean passes( Context context, Pair pair );
    }

    // keep in sync with loc_filters in common_rsrc.xml
    protected static enum SHOW_BYS {
        LOC_FILTERS_ALL
        ,LOC_FILTERS_SCREEN
        ,LOC_FILTERS_MENU
        ,LOC_FILTERS_MODIFIED
    };
    private SHOW_BYS m_showBy;

    public static class Pair {
        public Pair( String key, String eng, String xlation ) {
            m_key = key;
            m_english = eng;
            m_xlation = xlation;
        }

        public boolean matches( String term )
        {
            return m_english.contains( term ) ||
                (null != m_xlation && m_xlation.contains( term ) );
        }

        public void setXlation( String xlation )
        {
            m_xlation = xlation;
        }

        public String getKey() { return m_key; }

        private String m_key;
        private String m_english;
        private String m_xlation;
    }

    private Context m_context;
    private Pair[] m_pairs;
    private Pair[] m_filteredPairs;
    private Pair[] m_matchingPairs;
    private String m_lastTerm;

    public LocSearcher( Context context, Pair pairs[], String contextName )
    {
        m_pairs = m_filteredPairs = m_matchingPairs = pairs;
        m_context = context;
        m_contextName = contextName;
    }

    protected void start( int position )
    {
        if ( 0 <= position && position < SHOW_BYS.values().length ) {
            SHOW_BYS showBy = SHOW_BYS.values()[position];
            if ( m_showBy != showBy ) {
                m_showBy = showBy;

                if ( SHOW_BYS.LOC_FILTERS_ALL == showBy ) {
                    m_filteredPairs = m_pairs;
                } else {
                    FilterFunc proc = null;
                    switch ( showBy ) {
                    case LOC_FILTERS_SCREEN:
                        proc = new ScreenFilter( m_contextName );
                        break;
                    case LOC_FILTERS_MENU:
                        proc = s_menuProc;
                        break;
                    case LOC_FILTERS_MODIFIED:
                        proc = s_modifiedProc;
                        break;
                    }

                    ArrayList<Pair> matches = new ArrayList<Pair>();
                    for ( Pair pair : m_pairs ) {
                        if ( proc.passes( m_context, pair ) ) {
                            matches.add( pair );
                        }
                    }
                    m_filteredPairs = m_matchingPairs =
                        matches.toArray( new Pair[matches.size()] );
                }
                String term = m_lastTerm;
                m_lastTerm = null;
                start( term );
            }
        }
    }

    protected void start( String term )
    {
        if ( null == term || 0 == term.length() ) {
            m_matchingPairs = m_filteredPairs;
        } else {
            Pair[] usePairs = null != m_lastTerm && term.contains(m_lastTerm)
                ? m_matchingPairs : m_filteredPairs;
            DbgUtils.logf( "start: searching %d pairs", usePairs.length );
            ArrayList<Pair> matches = new ArrayList<Pair>();
            for ( Pair pair : usePairs ) {
                if ( pair.matches( term ) ) {
                    matches.add( pair );
                }
            }
            m_matchingPairs = matches.toArray( new Pair[matches.size()] );
        }
        m_lastTerm = term;
    }

    protected Pair getNthMatch( int nn )
    {
        return m_matchingPairs[nn];
    }

    protected int matchSize()
    {
        return m_matchingPairs.length;
    }

    private static FilterFunc s_modifiedProc = new FilterFunc() {
            public boolean passes( Context context, Pair pair ) {
                return null !=
                    LocUtils.getLocalXlation( context, pair.getKey(), true );
            }
        };

    private static FilterFunc s_menuProc = new FilterFunc() {
            public boolean passes( Context context, Pair pair ) {
                return LocUtils.inLatestMenu( pair.getKey() );
            }
        };

    private static class ScreenFilter implements FilterFunc {
        private String m_contextName;
        public ScreenFilter( String contextName )
        {
            m_contextName = contextName;
        }

        public boolean passes( Context context, Pair pair )
        {
            return LocUtils.inLatestScreen( pair.getKey(), m_contextName );
        }
    }
}

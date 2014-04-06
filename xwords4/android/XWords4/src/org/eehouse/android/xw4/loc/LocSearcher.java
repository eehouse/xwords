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

import org.eehouse.android.xw4.DbgUtils;

public class LocSearcher {

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

    private Pair[] m_pairs;
    private Pair[] m_matchingPairs;
    private String m_lastTerm;

    public LocSearcher( Pair pairs[] )
    {
        m_pairs = m_matchingPairs = pairs;
    }

    protected void start( String term )
    {
        if ( 0 == term.length() ) {
            m_matchingPairs = m_pairs;
        } else {
            Pair[] usePairs = null != m_lastTerm && term.contains(m_lastTerm) 
                ? m_matchingPairs : m_pairs;
            DbgUtils.logf( "start: searching %d pairs", usePairs.length );
            ArrayList<Pair> matchers = new ArrayList<Pair>();
            for ( Pair pair : usePairs ) {
                if ( pair.matches( term ) ) {
                    matchers.add( pair );
                }
            }
            m_matchingPairs = matchers.toArray( new Pair[matchers.size()] );
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

}

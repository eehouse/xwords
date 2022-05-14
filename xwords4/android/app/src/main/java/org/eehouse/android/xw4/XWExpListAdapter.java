/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009-2014 by Eric House (xwords@eehouse.org).  All
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

import android.view.View;
import android.view.ViewGroup;


import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

abstract class XWExpListAdapter extends XWListAdapter {

    interface GroupTest {
        boolean isTheGroup( Object item );
    }

    interface ChildTest {
        boolean isTheChild( Object item );
    }

    private Object[] m_listObjs;
    private Class m_groupClass;
    private int m_nGroups;
    private Map<Class, Integer> m_types;

    public XWExpListAdapter( Class[] childClasses )
    {
        m_groupClass = childClasses[0];
        m_types = new HashMap<>();
        for ( int ii = 0; ii < childClasses.length; ++ii ) {
            m_types.put( childClasses[ii], ii );
        }
    }

    abstract Object[] makeListData();
    abstract View getView( Object dataObj, View convertView );

    @Override
    public int getCount()
    {
        if ( null == m_listObjs ) {
            m_listObjs = makeListData();
            m_nGroups = 0;
            for ( int ii = 0; ii < m_listObjs.length; ++ii ) {
                if ( m_listObjs[ii].getClass() == m_groupClass ) {
                    ++m_nGroups;
                }
            }
        }
        return m_listObjs.length;
    }

    @Override
    public int getItemViewType( int position )
    {
        return m_types.get( m_listObjs[position].getClass() );
    }

    public int getViewTypeCount()
    {
        return m_types.size();
    }

    @Override
    public View getView( int position, View convertView, ViewGroup parent )
    {
        View result = getView( m_listObjs[position], convertView );
        // DbgUtils.logf( "getView(position=%d) => %H (%s)", position, result,
        //                result.getClass().getName() );
        return result;
    }


    protected int getGroupCount()
    {
        return m_nGroups;
    }

    protected int findGroupItem( GroupTest test )
    {
        int result = -1;
        for ( int ii = 0; ii < m_listObjs.length; ++ii ) {
            Object obj = m_listObjs[ii];
            if ( obj.getClass() == m_groupClass && test.isTheGroup( obj ) ) {
                result = ii;
                break;
            }
        }
        return result;
    }

    protected int indexForPosition( final int posn )
    {
        int result = -1;
        int curGroup = 0;
        for ( int ii = 0; ii < m_listObjs.length; ++ii ) {
            Object obj = m_listObjs[ii];
            if ( obj.getClass() == m_groupClass ) {
                if ( curGroup == posn ) {
                    result = ii;
                    break;
                }
                ++curGroup;
            }
        }
        return result;
    }

    protected void removeChildrenOf( int groupIndex )
    {
        Assert.assertTrueNR( 0 <= groupIndex );
        Assert.assertTrue( m_groupClass == m_listObjs[groupIndex].getClass() );
        int end = findGroupEnd( groupIndex );
        int nChildren = end - groupIndex - 1; // 1: don't remove parent
        Object[] newArray = new Object[m_listObjs.length - nChildren];
        System.arraycopy( m_listObjs, 0, newArray, 0, groupIndex + 1 ); // 1: include parent
        int nAbove = m_listObjs.length - (groupIndex + nChildren + 1);
        if ( end < m_listObjs.length ) {
            System.arraycopy( m_listObjs, end, newArray, groupIndex + 1,
                              m_listObjs.length - end );
        }
        m_listObjs = newArray;
        notifyDataSetChanged();
    }

    protected void addChildrenOf( int groupIndex, List<Object> children )
    {
        Assert.assertTrueNR( 0 <= groupIndex );
        int nToAdd = children.size();
        Object[] newArray = new Object[m_listObjs.length + nToAdd];
        System.arraycopy( m_listObjs, 0, newArray, 0, groupIndex + 1 ); // up to and including parent

        Iterator<Object> iter = children.iterator();
        for ( int ii = 0; iter.hasNext(); ++ii ) {
            newArray[groupIndex + 1 + ii] = iter.next();
        }
        System.arraycopy( m_listObjs, groupIndex + 1,
                          newArray, groupIndex + 1 + nToAdd,
                          m_listObjs.length - groupIndex - 1 );
        m_listObjs = newArray;
        notifyDataSetChanged();
    }

    protected void removeChildren( ChildTest test )
    {
        // Run over the array testing non-parents. For each that passes, mark
        // it as null.  Then reallocate.
        int nLost = 0;
        for ( int ii = 0; ii < m_listObjs.length; ++ii ) {
            Object obj = m_listObjs[ii];
            if ( obj.getClass() != m_groupClass && test.isTheChild( obj ) ) {
                ++nLost;
            } else if ( 0 < nLost ) {
                m_listObjs[ii - nLost] = obj;
            }
        }

        if ( 0 < nLost ) {
            m_listObjs = Arrays.copyOfRange( m_listObjs,
                                             0, m_listObjs.length - nLost );
            notifyDataSetChanged();
        }
    }

    protected Object findParent( ChildTest test )
    {
        Object result = null;
        Object curParent = null;
        for ( int ii = 0; ii < m_listObjs.length; ++ii ) {
            Object obj = m_listObjs[ii];
            if ( obj.getClass() == m_groupClass ) {
                curParent = obj;
            } else if ( test.isTheChild( obj ) ) {
                result = curParent;
                break;
            }
        }
        // DbgUtils.logf( "findParent() => %H (class %s)", result,
        //                null == result ? "null" : result.getClass().getName() );
        return result;
    }

    protected void swapGroups( int groupPosn1, int groupPosn2 )
    {
        // switch if needed so we know the direction
        if ( groupPosn1 > groupPosn2 ) {
            int tmp = groupPosn2;
            groupPosn2 = groupPosn1;
            groupPosn1 = tmp;
        }

        int groupIndx1 = indexForPosition( groupPosn1 );
        int groupIndx2 = indexForPosition( groupPosn2 );

        // copy out the lower group subarray
        int groupEnd1 = findGroupEnd( groupIndx1 );
        Object[] tmp1 = Arrays.copyOfRange( m_listObjs, groupIndx1, groupEnd1 );

        int groupEnd2 = findGroupEnd( groupIndx2 );
        int nToCopy = groupEnd2 - groupEnd1;
        System.arraycopy( m_listObjs, groupEnd1, m_listObjs, groupIndx1, nToCopy );

        // copy the saved subarray back in
        System.arraycopy( tmp1, 0, m_listObjs, groupIndx1 + nToCopy, tmp1.length );

        notifyDataSetChanged();
    }

    private int findGroupEnd( int indx )
    {
        ++indx;
        while ( indx < m_listObjs.length && ! (m_listObjs[indx].getClass() == m_groupClass) ) {
            ++indx;
        }
        return indx;
    }
}

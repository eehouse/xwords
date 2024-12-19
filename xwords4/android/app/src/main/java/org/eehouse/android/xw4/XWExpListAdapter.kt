/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All
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
package org.eehouse.android.xw4

import android.view.View
import android.view.ViewGroup

import java.util.Arrays

internal abstract class XWExpListAdapter(childClasses: Array<Class<*>>) :
    XWListAdapter()
{
    private val TAG = XWExpListAdapter::class.java.getSimpleName()

    internal interface GroupTest {
        fun isTheGroup(item: Any): Boolean
    }

    internal interface ChildTest {
        fun isTheChild(item: Any?): Boolean
    }

    private var mListObjs: Array<Any>? = null
    private val mGroupClass = childClasses[0]
    var groupCount: Int = 0
        private set
    private val mTypes: MutableMap<Class<*>, Int> = HashMap()

    init {
        for (ii in childClasses.indices) {
            mTypes[childClasses[ii]] = ii
        }
    }

    abstract fun makeListData(): Array<Any>
    abstract fun getView(dataObj: Any, convertView: View?): View

    override fun getCount(): Int {
        if (null == mListObjs) {
            mListObjs = makeListData()
            groupCount = mListObjs!!.filter{it.javaClass == mGroupClass}.size
        }
        return mListObjs!!.size
    }

    override fun getItemViewType(position: Int): Int {
        return mTypes[mListObjs!![position]!!.javaClass]!!
    }

    override fun getViewTypeCount(): Int {
        return mTypes.size
    }

    override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
        val result = getView(mListObjs!![position], convertView)
        // DbgUtils.logf( "getView(position=%d) => %H (%s)", position, result,
        //                result.getClass().getName() );
        return result
    }

    protected fun findGroupItem(test: GroupTest): Int {
        var result = -1
        for (ii in mListObjs!!.indices) {
            val obj = mListObjs!![ii]
            if (obj!!.javaClass == mGroupClass && test.isTheGroup(obj)) {
                result = ii
                break
            }
        }
        return result
    }

    protected fun indexForPosition(posn: Int): Int {
        var result = -1
        var curGroup = 0
        for (ii in mListObjs!!.indices) {
            val obj = mListObjs!![ii]
            if (obj!!.javaClass == mGroupClass) {
                if (curGroup == posn) {
                    result = ii
                    break
                }
                ++curGroup
            }
        }
        return result
    }

    protected fun removeChildrenOf(groupIndex: Int) {
        if (0 <= groupIndex) {
            Assert.assertTrue(mGroupClass == mListObjs!![groupIndex]!!.javaClass)
            val end = findGroupEnd(groupIndex)
            val nChildren = end - groupIndex - 1 // 1: don't remove parent
            val newArray = arrayOfNulls<Any>(mListObjs!!.size - nChildren)
            val listObjs = mListObjs!!
            System.arraycopy(listObjs, 0, newArray, 0, groupIndex + 1) // 1: include parent
            val nAbove = listObjs.size - (groupIndex + nChildren + 1)
            if (end < listObjs.size) {
                System.arraycopy(
                    listObjs, end, newArray, groupIndex + 1,
                    listObjs.size - end
                )
            }
            mListObjs = newArray as Array<Any>
            notifyDataSetChanged()
        }
    }

    protected fun addChildrenOf(groupIndex: Int, children: List<Any?>) {
        Assert.assertTrueNR(0 <= groupIndex)
        val nToAdd = children.size
        val newArray = arrayOfNulls<Any>(mListObjs!!.size + nToAdd)
        System.arraycopy(mListObjs!!, 0, newArray, 0, groupIndex + 1) // up to and including parent

        val iter = children.iterator()
        var ii = 0
        while (iter.hasNext()) {
            newArray[groupIndex + 1 + ii] = iter.next()
            ++ii
        }
        System.arraycopy(
            mListObjs!!, groupIndex + 1,
            newArray, groupIndex + 1 + nToAdd,
            mListObjs!!.size - groupIndex - 1
        )
        mListObjs = newArray as Array<Any>
        notifyDataSetChanged()
    }

    protected fun removeChildren(test: ChildTest) {
        // Run over the array testing non-parents. For each that passes, mark
        // it as null.  Then reallocate.
        var nLost = 0
        val listObjs = mListObjs!!
        for (ii in listObjs.indices) {
            val obj = listObjs[ii]
            if (obj!!.javaClass != mGroupClass && test.isTheChild(obj)) {
                ++nLost
            } else if (0 < nLost) {
                listObjs[ii - nLost] = obj
            }
        }

        if (0 < nLost) {
            mListObjs = Arrays.copyOfRange(
                listObjs, 0, listObjs.size - nLost
            )
            notifyDataSetChanged()
        }
    }

    protected fun findParent(test: ChildTest): Any? {
        var result: Any? = null
        var curParent: Any? = null
        for (ii in mListObjs!!.indices) {
            val obj = mListObjs!![ii]
            if (obj!!.javaClass == mGroupClass) {
                curParent = obj
            } else if (test.isTheChild(obj)) {
                result = curParent
                break
            }
        }
        // DbgUtils.logf( "findParent() => %H (class %s)", result,
        //                null == result ? "null" : result.getClass().getName() );
        return result
    }

    protected fun swapGroups(groupPosn1: Int, groupPosn2: Int) {
        // switch if needed so we know the direction
        var groupPosn1 = groupPosn1
        var groupPosn2 = groupPosn2
        if (groupPosn1 > groupPosn2) {
            val tmp = groupPosn2
            groupPosn2 = groupPosn1
            groupPosn1 = tmp
        }

        val groupIndx1 = indexForPosition(groupPosn1)
        val groupIndx2 = indexForPosition(groupPosn2)

        // copy out the lower group subarray
        val groupEnd1 = findGroupEnd(groupIndx1)
        val listObjs = mListObjs!!
        val tmp1 = Arrays.copyOfRange(listObjs, groupIndx1, groupEnd1)

        val groupEnd2 = findGroupEnd(groupIndx2)
        val nToCopy = groupEnd2 - groupEnd1
        System.arraycopy(listObjs, groupEnd1, listObjs, groupIndx1, nToCopy)

        // copy the saved subarray back in
        System.arraycopy(tmp1, 0, listObjs, groupIndx1 + nToCopy, tmp1.size)

        notifyDataSetChanged()
    }

    private fun findGroupEnd(indx: Int): Int {
        var indx = indx + 1
        val listObjs = mListObjs!!
        while (indx < listObjs.size
                   && listObjs[indx].javaClass != mGroupClass) {
            ++indx
        }
        return indx
    }
}

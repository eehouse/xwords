/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

import android.app.Activity
import android.content.Context
import android.os.Bundle

import java.util.HashMap

import org.eehouse.android.xw4.Utils.ISOCode

class StudyListDelegate(delegator: Delegator, sis: Bundle?) :
	IsoWordsBase(delegator, sis, "CHECKED_KEY"),
	DBUtils.StudyListListener {

	companion object {
		private val TAG = StudyListDelegate::class.java.getSimpleName()

		@JvmStatic
		public fun launch( delegator: Delegator )
		{
			launch( delegator, null )
		}

		@JvmStatic
		public fun launch( delegator: Delegator, isoCode: ISOCode? )
		{
			val context = delegator.getActivity() as Context
			if ( null == isoCode ) {
				Assert.assertTrueNR( 0 < DBUtils.studyListLangs( context ).size )
			} else {
				Assert.assertTrueNR( DBUtils.studyListWords( context, isoCode!! ).isNotEmpty() )
			}

			delegator.addFragment( StudyListFrag.newInstance( delegator ),
								   mkBundle( isoCode ) )
		}
	}

    override fun onResume()
    {
        super.onResume()
        DBUtils.addStudyListChangedListener( this )
    }

	override fun onPause()
    {
        DBUtils.removeStudyListChangedListener( this )
        super.onPause()
    }

    //////////////////////////////////////////////////
    // DBUtils.StudyListListener
    //////////////////////////////////////////////////
    override fun onWordAdded( word: String, isoCode: ISOCode )
    {
        if ( isoCode.equals( m_langCodes!![m_langPosition] ) ) {
            loadList()
        }
    }

    //////////////////////////////////////////////////
    // Abstract methods from superclass
    //////////////////////////////////////////////////
	override fun getData( context: Context ): HashMap<ISOCode, ArrayList<String>> {
		val result = HashMap<ISOCode, ArrayList<String>>()
		for ( code in DBUtils.studyListLangs( m_activity ) ) {
			val words = DBUtils.studyListWords( m_activity, code )
			result.put(code, words)
		}
		return result
	}

	override fun clearWords( isoCode: ISOCode, words: Array<String> ) {
		DBUtils.studyListClear( m_activity, isoCode, words )
	}

	override fun getTitleID(): Int = R.string.studylist_title_fmt
}

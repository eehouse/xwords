/*
 * Copyright 2014 - 2016 by Eric House (xwords@eehouse.org).  All rights
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
package org.eehouse.android.xw4

import android.content.Context
import android.os.Bundle
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.jni.Device

class LegalPhoniesDelegate(delegator: Delegator) :
	IsoWordsBase(delegator, "CHECKED_KEY_LP")
{
    companion object {
		private val TAG = LegalPhoniesDelegate::class.java.getSimpleName()
		public fun launch( delegator: Delegator )
		{
			launch( delegator, null )
		}

		public fun launch( delegator: Delegator, isoCode: Utils.ISOCode? )
			= delegator.addFragment( LegalPhoniesFrag.newInstance( delegator ),
									 mkBundle(isoCode) )

		public suspend fun haveLegalPhonies(context: Context): Boolean
			= !getDataPrv(context).isEmpty()

		private suspend fun getDataPrv( context: Context ):
            HashMap<ISOCode, ArrayList<String>> {
			val result = HashMap<ISOCode, ArrayList<String>>()

			Device.getLegalPhonyCodes().map { code ->
				val strings = Device.getLegalPhoniesFor( code )
				val al = ArrayList<String>()
				al.addAll(strings)
				result.put(code, al)
			}
			return result
		}
    }

	override suspend fun getData( context: Context ): HashMap<ISOCode, ArrayList<String>>
		= getDataPrv(context)

	override fun clearWords( isoCode: ISOCode, words: Array<String> )
	{
		for ( word in words ) {
			Device.clearLegalPhony(isoCode, word)
		}
	}

	override fun getTitleID(): Int = R.string.legalphonies_title_fmt
}

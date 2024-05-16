/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2023 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
import android.text.TextUtils
import android.util.AttributeSet
import android.view.View
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup

import java.util.HashMap
import java.util.Map

import org.eehouse.android.xw4.jni.XwJNI
import org.eehouse.android.xw4.jni.XwJNI.RematchOrder
import org.eehouse.android.xw4.loc.LocUtils

class RematchConfigView(val mContext: Context, attrs: AttributeSet)
	: LinearLayout( mContext, attrs )
	, RadioGroup.OnCheckedChangeListener
{
    companion object {
        private val TAG = RematchConfigView::class.java.simpleName
        private val KEY_LAST_RO = TAG + "/key_last_ro"
    }

    var mDlgDlgt: DlgDelegate.HasDlgDelegate? = null
    var mGroup: RadioGroup? = null
	var mWrapper: GameUtils.GameWrapper? = null
    val mRos: HashMap<Int, RematchOrder> = HashMap<Int, RematchOrder>()
    var mInflated: Boolean? = null
    var mNameStr: String? = null
    var mNewOrder: Array<Int>? = null
    var mSep: String? = null
    var mUserEditing: Boolean = false
	var mNAShown: Boolean? = null
    var mEWC: EditWClear? = null
    var mCurRO: RematchOrder? = null

	public fun configure( rowid: Long, dlgDlgt: DlgDelegate.HasDlgDelegate  )
    {
        mDlgDlgt = dlgDlgt
        mWrapper = GameUtils.GameWrapper.make( mContext, rowid )
        val nPlayers = mWrapper!!.gi().nPlayers
        mNewOrder = Array<Int>(nPlayers, {it})
        trySetup()
    }

	fun getName(): String
    {
        return mEWC?.getText().toString()
    }

    override fun onFinishInflate(): Unit
    {
        mInflated = true
        trySetup()
    }

    override fun onDetachedFromWindow()
    {
        mWrapper?.close()
        mWrapper = null
        super.onDetachedFromWindow()
    }

    // RadioGroup.OnCheckedChangeListener
	override fun onCheckedChanged( group: RadioGroup, checkedId: Int )
    {
        val curName = getName()
        if ( TextUtils.isEmpty( curName ) ) {
            mUserEditing = false
        } else if ( !mUserEditing && null != mNameStr ) {
            mUserEditing = ! mNameStr.equals( curName )
        }

        mCurRO = mRos.get( checkedId )
        mNewOrder = XwJNI.server_figureOrderKT( mWrapper!!.gamePtr(), mCurRO!!)

        if ( mUserEditing ) {
            if ( mNAShown != true ) {
                mNAShown = true
                mDlgDlgt!!.makeNotAgainBuilder( R.string.key_na_rematch_edit,
												R.string.na_rematch_edit )
                    .show()
            }
        } else {
            setName()
        }
    }

    public fun getNewOrder(): Array<Int>?
    {
        if ( null != mCurRO ) {
            DBUtils.setIntFor( mContext, KEY_LAST_RO, mCurRO!!.ordinal )
        }
        return mNewOrder
    }

    private fun trySetup()
    {
        if ( mInflated == true && null != mWrapper ) {
            mSep = LocUtils.getString( mContext, R.string.vs_join )
            mGroup = findViewById( R.id.group )
            mGroup!!.setOnCheckedChangeListener( this )
            mEWC = findViewById( R.id.name )

            val results = XwJNI.server_canOfferRematch( mWrapper!!.gamePtr() )
            if ( results[0] && results[1] ) {
                val ordinal = DBUtils.getIntFor( mContext, KEY_LAST_RO,
                                                 RematchOrder.RO_SAME.ordinal )
                val lastSel = RematchOrder.entries[ordinal]

                for ( ro in RematchOrder.entries ) {
                    val strId = ro.strID
                    if ( 0 != strId ) {
                        val button = RadioButton( mContext )
                        button.setText( LocUtils.getString( mContext, strId ) )
                        mGroup!!.addView( button )
                        mRos.put( button.getId(), ro )
                        if ( lastSel == ro ) {
                            button.setChecked( true )
                        }
                    }
                }
            } else {
				// not sure why I have to cast this....
                (findViewById( R.id.ro_stuff) as View).setVisibility( View.GONE )
                setName()
            }
        }
    }

    private fun setName()
    {
        mNameStr = TextUtils.join( mSep!!.toString(), mWrapper!!.gi().playerNames(mNewOrder) )
        mEWC?.setText( mNameStr )
    }
}

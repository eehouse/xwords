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
import androidx.core.view.doOnAttach

import java.util.HashMap
import java.util.Map

import org.eehouse.android.xw4.jni.CurGameInfo
import org.eehouse.android.xw4.jni.GameRef
import org.eehouse.android.xw4.jni.GameRef.RematchOrder
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
    var mGR: GameRef? = null
    var mGI: CurGameInfo? = null
    val mRos: HashMap<Int, RematchOrder> = HashMap<Int, RematchOrder>()

    var mNameStr: String? = null
    var mNewOrder: Array<Int>? = null
    var mSep: String? = null
    var mUserEditing: Boolean = false
	var mNAShown: Boolean? = null
    var mEWC: EditWClear? = null
    var mCurRO: RematchOrder? = null

    init {
        this.doOnAttach {
            trySetup()
        }
    }

	public fun configure(gr: GameRef, dlgDlgt: DlgDelegate.HasDlgDelegate  )
    {
        mDlgDlgt = dlgDlgt
        mGR = gr
        trySetup()
    }

	fun getName(): String
    {
        return mEWC?.text.toString()
    }

    fun getGR(): GameRef { return mGR!! }

    fun getRematchOrder(): RematchOrder {
        DBUtils.setIntFor( mContext, KEY_LAST_RO, mCurRO!!.ordinal )
        return mCurRO!!
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
        launch {
            mNewOrder = mGR!!.figureOrder(mCurRO!!)

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
    }

    public fun getNewOrder(): Array<Int>?
    {
        mCurRO?.let {
            DBUtils.setIntFor( mContext, KEY_LAST_RO, it.ordinal )
        }
        return mNewOrder
    }

    private fun trySetup()
    {
        if (null == mGI) {
            mGR?.let { gr ->
                launch {
                    mGI = gr.getGI()
                    mNewOrder = Array<Int>(mGI!!.nPlayers, {it})
                    
                    mSep = LocUtils.getString( mContext, R.string.vs_join )
                    val group = findViewById<RadioGroup>( R.id.group )
                    group.setOnCheckedChangeListener( this@RematchConfigView )
                    mEWC = findViewById( R.id.name )

                    val results = gr.canOfferRematch()
                    if ( results[0] && results[1] ) {
                        val ordinal = DBUtils.getIntFor( mContext, KEY_LAST_RO,
                                                         RematchOrder.RO_SAME.ordinal )
                        val lastSel = RematchOrder.entries[ordinal]

                        for ( ro in RematchOrder.entries ) {
                            val strId = ro.strID
                            if ( 0 != strId ) {
                                val button = RadioButton( mContext )
                                button.setText( LocUtils.getString( mContext, strId ) )
                                group.addView( button )
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
        }
    }

    private fun setName()
    {
        mNameStr = TextUtils.join( mSep!!, mGI!!.playerNames(mNewOrder!!) )
        mEWC?.setText( mNameStr )
    }
}

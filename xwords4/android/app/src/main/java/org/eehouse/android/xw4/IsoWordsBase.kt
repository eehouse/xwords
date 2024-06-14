/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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
import android.text.TextUtils
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Spinner
import org.eehouse.android.xw4.jni.GameSummary

import java.util.HashMap

abstract class IsoWordsBase(delegator: Delegator, private val CHECKED_KEY: String )
	: ListDelegateBase(delegator, R.layout.studylist, R.menu.studylist),
	  SelectableItem, View.OnLongClickListener, View.OnClickListener,
      AdapterView.OnItemSelectedListener
{
	val m_activity = delegator.getActivity()!!

	var m_pickView: LabeledSpinner? = null
	var m_spinner: Spinner? = null
	var m_langCodes: Array<Utils.ISOCode>? = null
	var m_words: Array<String>? = arrayOf()
	val m_checkeds = HashSet<String>()
	var m_langPosition: Int = 0
	var m_adapter: SLWordsAdapter? = null
    private var m_origTitle: String? = null

	// Subclasses must override these
	abstract fun getData(context: Context): HashMap<Utils.ISOCode, ArrayList<String>>
	abstract fun clearWords(isoCode: Utils.ISOCode, words: Array<String> )
	abstract fun getTitleID(): Int

    override fun init( sis: Bundle? )
    {
        m_pickView = findViewById( R.id.pick_lang ) as LabeledSpinner
        m_spinner = m_pickView?.getSpinner()

        getBundledData( sis )

        initOrFinish( getArguments() )
    }

    override fun onPrepareOptionsMenu( menu: Menu): Boolean
    {
        val nSel = m_checkeds.size
        Utils.setItemVisible( menu, R.id.slmenu_copy_sel, 0 < nSel )
        Utils.setItemVisible( menu, R.id.slmenu_clear_sel, 0 < nSel )
        Utils.setItemVisible( menu, R.id.slmenu_select_all,
                              m_words!!.size > nSel )
        Utils.setItemVisible( menu, R.id.slmenu_deselect_all, 0 < nSel )

        val enable = 1 == nSel
        if ( enable ) {
            val title = getString( R.string.button_lookup_fmt,
                                   getSelWords()!![0] )
            menu.findItem( R.id.slmenu_lookup_sel ).setTitle( title )
        }
        Utils.setItemVisible( menu, R.id.slmenu_lookup_sel, enable )

        return true
    }

	override protected fun handleBackPressed(): Boolean
    {
        val handled = 0 < m_checkeds.size
        if ( handled ) {
            clearSels()
        }
        return handled
    }

    override fun onOptionsItemSelected( item: MenuItem): Boolean
    {
		var handled = true
        when ( item.getItemId() ) {
			R.id.slmenu_copy_sel -> {
				makeNotAgainBuilder( R.string.key_na_studycopy,
									 DlgDelegate.Action.SL_COPY_ACTION,
									 R.string.not_again_studycopy )
					.show()
			}
			R.id.slmenu_clear_sel -> {
				val msg = getQuantityString( R.plurals.confirm_studylist_clear_fmt,
											 m_checkeds.size, m_checkeds.size )
				makeConfirmThenBuilder( DlgDelegate.Action.SL_CLEAR_ACTION, msg ).show()
			}

			R.id.slmenu_select_all -> {
				m_checkeds.addAll( m_words!! )
				makeAdapter()
				setTitleBar()
			}
			R.id.slmenu_deselect_all -> clearSels()
			
			R.id.slmenu_lookup_sel -> {
				val oneWord = arrayOf( getSelWords()!![0] )
				launchLookup( oneWord, m_langCodes!![m_langPosition], true )
			}
			else -> handled = false
        }
        return handled
    }

	override fun onSaveInstanceState( outState: Bundle )
    {
        outState.putSerializable( CHECKED_KEY, m_checkeds as HashSet )
    }

    //////////////////////////////////////////////////
    // DlgDelegate.DlgClickNotify interface
    //////////////////////////////////////////////////
    override fun onPosButton(action: DlgDelegate.Action, vararg params: Any ): Boolean
    {
        Assert.assertVarargsNotNullNR(params)
        var handled = true
        when ( action ) {
			DlgDelegate.Action.SL_CLEAR_ACTION -> {
				var selWords: Array<String>? = getSelWords()
				if ( null != selWords ) {
					clearWords( m_langCodes!![m_langPosition], selWords!! )
					clearSels()
				}
				initOrFinish()
			}
			DlgDelegate.Action.SL_COPY_ACTION -> {
				var selWords = getSelWords()
				Utils.stringToClip( m_activity, TextUtils.join( "\n", selWords!! ) )

                val msg = getQuantityString( R.plurals.paste_done_fmt,
											 selWords.size, selWords.size )
				showToast( msg )
			}
			else -> {
				Log.d( TAG, "not handling: %s", action )
				handled = false
			}
		}
        return handled
    }
	
	//////////////////////////////////////////////////
    // SelectableItem interface
    //////////////////////////////////////////////////
    override fun itemClicked( clicked: SelectableItem.LongClickHandler,
                              summary: GameSummary
    )
    {
        m_checkeds.add( (clicked as XWListItem).getText() )
    }

    override public fun itemToggled( toggled: SelectableItem.LongClickHandler,
									 selected: Boolean )
    {
        val word = (toggled as XWListItem).getText()
        if ( selected ) {
            m_checkeds.add( word )
        } else {
            m_checkeds.remove( word )
        }
        setTitleBar()
    }

	//////////////////////////////////////////////////
    // AdapterView.OnItemSelectedListener interface
    //////////////////////////////////////////////////
    override fun onItemSelected( parent: AdapterView<*>?, view: View,
                                 position: Int, id: Long )
    {
        m_langPosition = position
        m_checkeds.clear()
        loadList()             // because language has changed
    }

	override fun onNothingSelected(parent: AdapterView<*>?) {
        TODO("Not yet implemented")
    }

	//////////////////////////////////////////////////
    // View.OnClickListener interface
    //////////////////////////////////////////////////
	override fun onClick( view: View )
    {
        val item = view as XWListItem
        val words: Array<String> = arrayOf(m_words!![item.getPosition()])
        launchLookup( words, m_langCodes!![m_langPosition], true )
    }

    override fun getSelected( obj: SelectableItem.LongClickHandler  ): Boolean
    {
        return m_checkeds.contains( (obj as XWListItem).getText() )
    }

	//////////////////////////////////////////////////
    // View.OnLongClickListener interface
    //////////////////////////////////////////////////
    override fun onLongClick( view: View ): Boolean 
    {
        val success = view is SelectableItem.LongClickHandler
        if ( success ) {
            (view as SelectableItem.LongClickHandler).longClicked()
        }
        return success
    }

	private fun initOrFinish( args: Bundle? = null )
    {
		val data = getData(m_activity)
        m_langCodes = data.keys.toTypedArray()
		when ( m_langCodes!!.size ) {
			0 -> finish()
			1 -> {
				m_pickView!!.setVisibility( View.GONE )
				m_langPosition = 0
				loadList()
			}
			else -> {
				var startLang: String? = null
				var startIndex = -1
				if ( null != args ) {
					startLang = args.getString( START_LANG )
				}

				val siz = m_langCodes!!.size
				val myNames = ArrayList<String>()
				for ( ii in 0 ..< siz ) {
					val isoCode = m_langCodes!![ii]
					myNames.add( DictLangCache.getLangNameForISOCode( m_activity,
																	  isoCode )!!)
					if ( isoCode.equals( startLang ) ) {
						startIndex = ii
					}
				}

				val adapter = ArrayAdapter<String>( m_activity,
													android.R.layout.simple_spinner_item,
													myNames )
				adapter.setDropDownViewResource( android.R.layout.
													 simple_spinner_dropdown_item )
				m_spinner!!.setAdapter( adapter )
				m_spinner!!.setOnItemSelectedListener( this )
				if ( -1 != startIndex ) {
					m_spinner!!.setSelection( startIndex )
				}
			}
        }
    }

    private fun getBundledData( sis: Bundle? )
    {
        if ( null != sis ) {
			val checkeds = sis.getSerializable( CHECKED_KEY ) as HashSet<String>
			if ( null != checkeds ) {
				m_checkeds.clear()
				m_checkeds.addAll(checkeds)
			}
        }
    }

    private fun clearSels()
    {
        m_checkeds.clear()
        makeAdapter()
        setTitleBar()
    }

	private fun setTitleBar()
    {
        val nSels = m_checkeds.size
        setTitle( if ( 0 == nSels ) {
					  m_origTitle
				  } else {
					  getString( R.string.sel_items_fmt, nSels )
				  } )
        invalidateOptionsMenuIf()
    }

	private fun getSelWords(): Array<String>?
    {
        val nSels = m_checkeds.size
        var result: Array<String>? = null
        if ( nSels == m_words!!.size ) {
            result = m_words
        } else {
            val tmp = m_checkeds
            result = tmp!!.toTypedArray()
        }
        return result
    }

	protected fun loadList()
    {
        val isoCode = m_langCodes!![m_langPosition]
		val data = getData( m_activity )
		val al = data.get( isoCode )
        m_words =  al?.toTypedArray()

        makeAdapter()

        val langName = DictLangCache.getLangNameForISOCode( m_activity, isoCode )
        m_origTitle = getString( getTitleID(), langName )
        setTitleBar()
    }

	private fun makeAdapter()
    {
        m_adapter = SLWordsAdapter()
        setListAdapter( m_adapter )
    }

    inner class SLWordsAdapter() : XWListAdapter(m_words!!.size) {

        override fun getView( position: Int, convertView: View?,
							  parent: ViewGroup? ): View {
            val item = XWListItem.inflate( m_activity, this@IsoWordsBase )
            item.setPosition( position )
            val word = m_words!![position]
            item.setText( word )
            item.setSelected( m_checkeds.contains( word ) )
            item.setOnLongClickListener( this@IsoWordsBase )
            item.setOnClickListener( this@IsoWordsBase )
            return item
        }
    }

	companion object {
		private val TAG = IsoWordsBase::class.java.getSimpleName()
        const val START_LANG = "START_LANG"

		@JvmStatic
        protected fun mkBundle( isoCode: Utils.ISOCode? ): Bundle {
			val bundle = Bundle()
			if ( null != isoCode ) {
				bundle.putString( START_LANG, isoCode.toString() )
			}
			return bundle
		}
	}
}

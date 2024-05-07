/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

package org.eehouse.android.xw4

import android.content.Intent
import android.os.Bundle
import androidx.fragment.app.Fragment
import android.view.LayoutInflater
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.ListAdapter
import android.widget.ListView

import java.util.HashSet
import java.util.Set

abstract open class XWFragment: Fragment(), Delegator {

	companion object {
		private val TAG = XWFragment::class.java.getSimpleName()

		private const val PARENT_NAME = "PARENT_NAME"
		private const val COMMIT_ID = "COMMIT_ID"

		private val sActiveFrags = HashSet<XWFragment>()

		@JvmStatic
		fun findOwnsView( view: View ): XWFragment?
		{
			var result: XWFragment? = null
			DbgUtils.assertOnUIThread()
			for ( frag in sActiveFrags ) {
				if ( frag.getView() == view ) {
					Assert.assertNull( result )
					result = frag
					// break;  <-- put this back eventually
				}
			}

			return result
		}
	}

    var m_dlgt: DelegateBase? = null
    var m_parentName: String? = null
    var m_hasOptionsMenu = false
    var m_commitID: Int = 0

    fun setParentName( parent: Delegator? ): XWFragment 
    {
        m_parentName =
			if ( null == parent ) "<none>"
			else parent::class.java.getSimpleName()
        return this
    }

	fun getParentName(): String
    {
        Assert.assertNotNull( m_parentName )
        return m_parentName!!
    }

    fun setCommitID( id: Int ) { m_commitID = id }
    fun getCommitID(): Int { return m_commitID }

    override fun onSaveInstanceState( outState: Bundle )
    {
        Log.d( TAG, "%H/%s.onSaveInstanceState() called", this,
			   this::class.java.getSimpleName() )
        Assert.assertNotNull( m_parentName )
        outState?.putStringAnd( PARENT_NAME, m_parentName )
			?.putIntAnd( COMMIT_ID, m_commitID )
        m_dlgt?.onSaveInstanceState( outState )
        super.onSaveInstanceState( outState )
    }

    fun onCreate( dlgt: DelegateBase, sis: Bundle?, hasOptionsMenu: Boolean )
    {
        Log.d( TAG, "%H/%s.onCreate() called", this, this::class.java.getSimpleName() )
        m_hasOptionsMenu = hasOptionsMenu
        this.onCreate( dlgt, sis )
    }

    fun onCreate( dlgt: DelegateBase, sis: Bundle? )
    {
        Log.d( TAG, "%H/%s.onCreate() called", this, this::class.java.getSimpleName() )
        super.onCreate( sis )
        if ( null != sis ) {
            m_parentName = sis.getString( PARENT_NAME )
            Assert.assertNotNull( m_parentName )
            m_commitID = sis.getInt( COMMIT_ID )
        }
        Assert.assertNull( m_dlgt )
        m_dlgt = dlgt
    }

    // This is supposed to be the first call we can use to start hooking stuff
    // up.
    // @Override
    // public void onAttach( Activity activity )
    // {
    //     Log.df( "%s.onAttach() called",
    //                     this.getClass().getSimpleName() );
    //     super.onAttach( activity );
    // }
    override fun onCreateView( inflater: LayoutInflater,
							   container: ViewGroup?,
							   savedInstanceState: Bundle? ): View?
    {
        Log.d( TAG, "%H/%s.onCreateView() called", this, this::class.java.getSimpleName() )
        sActiveFrags.add(this)
        return m_dlgt?.inflateView( inflater, container )
    }

    // override fun onActivityCreated(savedInstanceState: Bundle?) {
    //     super.onActivityCreated(savedInstanceState)
    // }

    override fun onActivityCreated(savedInstanceState: Bundle? )
    {
        Log.d( TAG, "%H/%s.onActivityCreated() called", this, this::class.java.getSimpleName() )
        m_dlgt?.init( savedInstanceState )
        super.onActivityCreated( savedInstanceState )
        if ( m_hasOptionsMenu ) {
            setHasOptionsMenu( true )
        }
    }

    override fun onPause()
    {
        Log.d( TAG, "%H/%s.onPause() called", this, this::class.java.getSimpleName() )
        m_dlgt?.onPause()
        super.onPause()
    }

    override fun onResume()
    {
        Log.d( TAG, "%H/%s.onResume() called", this, this::class.java.getSimpleName() )
        super.onResume()
        m_dlgt?.onResume()
    }

    override fun onStart()
    {
        Log.d( TAG, "%H/%s.onStart() called", this, this::class.java.getSimpleName() )
        super.onStart()
        m_dlgt?.onStart()
    }

    override fun onStop()
    {
        Log.d( TAG, "%H/%s.onStop() called", this, this::class.java.getSimpleName() )
        m_dlgt?.onStop()
        super.onStop()
    }

    override fun onDestroy()
    {
        Log.d( TAG, "%H/%s.onDestroy() called", this, this::class.java.getSimpleName() )
        m_dlgt?.onDestroy()
        sActiveFrags.remove( this )
        super.onDestroy()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        Log.d( TAG, "%H/%s.onActivityResult() called", this, this::class.java.getSimpleName() )
        m_dlgt?.onActivityResult( RequestCode.entries[requestCode],
                                  resultCode, data )
    }

    override fun onPrepareOptionsMenu( menu: Menu )
    {
        m_dlgt?.onPrepareOptionsMenu( menu )
    }

    override fun onCreateOptionsMenu( menu: Menu, inflater: MenuInflater )
    {
        m_dlgt?.onCreateOptionsMenu( menu, inflater )
    }

    override fun onOptionsItemSelected( item: MenuItem ): Boolean
		= m_dlgt!!.onOptionsItemSelected( item )

    override fun finish()
    {
        Assert.failDbg()
    }

    fun setTitle() { m_dlgt?.setTitle() }

    override fun addFragment( fragment: XWFragment, extras: Bundle? )
    {
        val main = getActivity() as MainActivity
        if ( null != main ) {   // I've seen this come back null
            main.addFragment( fragment, extras )
        }
    }

    override fun addFragmentForResult( fragment: XWFragment, extras: Bundle,
                                       code: RequestCode )
    {
        val main = getActivity() as MainActivity
        main.addFragmentForResult( fragment, extras, code, this )
    }

	fun getDelegate() : DelegateBase?
    {
        return m_dlgt
    }

    override fun getListView(): ListView
    {
        val view = m_dlgt?.findViewById( android.R.id.list ) as ListView
        return view
    }

    override fun setListAdapter( adapter: ListAdapter )
    {
        getListView().setAdapter( adapter )
    }

    override fun getListAdapter(): ListAdapter
    {

        return getListView().getAdapter()
    }
}

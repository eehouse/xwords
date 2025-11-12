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

import android.content.Intent
import android.content.res.Configuration
import android.os.Bundle
import android.text.TextUtils
import android.view.ContextMenu
import android.view.ContextMenu.ContextMenuInfo
import android.view.MenuItem
import android.view.View
import android.widget.LinearLayout
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import org.eehouse.android.xw4.DbgUtils.assertOnUIThread
import java.lang.ref.WeakReference
import kotlin.math.min

class MainActivity : XWActivity(), FragmentManager.OnBackStackChangedListener {
    private var m_root: LinearLayout? = null
    private var m_safeToCommit = false
    private val m_runWhenSafe = ArrayList<Runnable>()
    private var m_newIntent: Intent? = null // work in progress...

    // for tracking launchForResult callback recipients
    private val m_pendingCodes: Map<RequestCode, WeakReference<DelegateBase>> = HashMap()
    override fun onCreate(savedInstanceState: Bundle?) {
        if (BuildConfig.NON_RELEASE && !isTaskRoot) {
            Log.e(TAG, "isTaskRoot() => false!!! What to do?")
        }

        if ( RecoveryActivity.active() ) {
            super.onCreate( savedInstanceState )
            RecoveryActivity.start(this)
            finish()
        } else {
            val dlgt = DualpaneDelegate(this)
            super.onCreate(savedInstanceState, dlgt)
            m_root = findViewById<View>(R.id.main_container) as LinearLayout
            supportFragmentManager.addOnBackStackChangedListener(this)

            // Nothing to do if we're restarting
            if (savedInstanceState == null) {
                // In case this activity was started with special instructions from an Intent,
                // pass the Intent's extras to the fragment as arguments
                addFragmentImpl(
                    GamesListFrag.newInstance(),
                    intent.extras, null
                )
            }
            setSafeToRun()

            KAService.startIf(this)
        }
    } // onCreate

    override fun onSaveInstanceState(outState: Bundle) {
        m_safeToCommit = false
        super.onSaveInstanceState(outState)
    }

    override fun onPostResume() {
        setSafeToRun()
        super.onPostResume()
        setVisiblePanes()
        logPaneFragments()
        // getDelegate().onPostResume()
    }

    // called when we're brought to the front (probably as a result of
    // notification)
    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        getDelegate().handleNewIntent(intent)
    }

    /* Sometimes I'm getting crashes because views don't have fragments
     * associated yet. I suspect that's because adding them's been postponed
     * via the m_runWhenSafe mechanism. So: postpone handling intents too.
     *
     * This postponing thing won't scale, and makes me suspect there's
     * something I'm doing wrong w.r.t. fragments. Should be revisited. In
     * this particular case there might be a better way to get to the Delegate
     * on which I need to call handleNewIntent().
     */
    fun dispatchNewIntent(intent: Intent): Boolean {
        val handled: Boolean
        handled = if (m_safeToCommit) {
            dispatchNewIntentImpl(intent)
        } else {
            assertOnUIThread()
            m_runWhenSafe.add(Runnable { dispatchNewIntentImpl(intent) })
            if (BuildConfig.DEBUG) {
                Log.d(
                    TAG, "Putting off handling intent; %d waiting",
                    m_runWhenSafe.size
                )
            }
            true
        }
        return handled
    }

    private fun popIntoView(newTopFrag: XWFragment) {
        val fm = supportFragmentManager
        while (true) {
            val top = m_root!!.childCount - 1
            if (top < 0) {
                break
            }
            val child = m_root!!.getChildAt(top)
            val frag = findFragment(child)
            if (frag === newTopFrag) {
                break
            }
            val name = frag!!.javaClass.getSimpleName()
            Log.d(TAG, "popIntoView(): popping %d: %s", top, name)
            fm.popBackStackImmediate()
            Log.d(TAG, "popIntoView(): DONE popping %s", name)
        }
    }

    /**
     * Run down the list of fragments until one can handle the intent. If
     * necessary, pop fragments above it until it comes into view. Then send
     * it the event.
     */
    private fun dispatchNewIntentImpl(intent: Intent): Boolean {
        var handled = false
        var ii = m_root!!.childCount - 1
        while (!handled && ii >= 0) {
            val child = m_root!!.getChildAt(ii)
            val frag = findFragment(child)
            if (null != frag) {
                handled = frag.getDelegate()!!.canHandleNewIntent(intent)
                if (handled) {
                    popIntoView(frag)
                    frag.getDelegate()!!.handleNewIntent(intent)
                }
            } else {
                Log.d(
                    TAG, "no fragment for child %s indx %d",
                    child.javaClass.getSimpleName(), ii
                )
            }
            --ii
        }
        if (BuildConfig.DEBUG && !handled) {
            // DbgUtils.showf( this, "dropping intent %s", intent.toString() );
            Log.d(TAG, "dropping intent %s", intent.toString())
            // DbgUtils.printStack();
            // setIntent( intent ); -- look at handling this in onPostResume()?
            m_newIntent = intent
        }
        return handled
    }

    /**
     * The right-most pane only gets a chance to handle on-back-pressed.
     */
    fun dispatchBackPressed(): Boolean {
        val frag = topFragment
        return (null != frag
                && frag.getDelegate()!!.handleBackPressed())
    }

    fun dispatchOnActivityResult(
        requestCode: RequestCode,
        resultCode: Int, data: Intent?
    ) {
        val frag = topFragment
        if (null != frag) {
            frag.onActivityResult(requestCode.ordinal, resultCode, data)
        } else {
            Log.d(
                TAG, "dispatchOnActivityResult(): can't dispatch %s",
                requestCode.toString()
            )
        }
    }

    fun dispatchOnCreateContextMenu(
        menu: ContextMenu, view: View,
        ignoreMe: ContextMenuInfo?
    ) {
        val frags = visibleFragments
        for (frag in frags) {
            frag!!.getDelegate()!!.onCreateContextMenu(menu, view, ignoreMe)
        }
    }

    fun dispatchOnContextItemSelected(item: MenuItem): Boolean {
        var handled = false
        val frags = visibleFragments
        for (frag in frags) {
            handled = frag!!.getDelegate()!!.onContextItemSelected(item)
            if (handled) {
                break
            }
        }
        return handled
    }

    //////////////////////////////////////////////////////////////////////
    // Delegator interface
    //////////////////////////////////////////////////////////////////////
    override fun addFragment(fragment: XWFragment, extras: Bundle?) {
        addFragmentImpl(fragment, extras, fragment.getParentName())
    }

    private inner class PendingResultCache(
        target: Fragment, var m_request: Int,
        var m_result: Int, var m_data: Intent?
    ) {
        private val mFrag: WeakReference<Fragment>
            = WeakReference(target)

        fun getTarget(): Fragment? {
            return mFrag.get()
        }
    }

    private var m_pendingResult: PendingResultCache? = null
    fun addFragmentForResult(
        fragment: XWFragment, extras: Bundle?,
        requestCode: RequestCode, registrant: XWFragment?
    ) {
        assertOnUIThread()
        fragment.setTargetFragment(registrant, requestCode.ordinal)
        addFragmentImpl(fragment, extras, fragment.getParentName())
    }

    fun setFragmentResult(
        fragment: XWFragment?,  // this is occasionally null. Not sure why
        resultCode: Int, data: Intent?
    ) {
        val target = fragment?.getTargetFragment()
        target?.let {
            val requestCode = fragment.targetRequestCode
            Assert.assertTrueNR(null == m_pendingResult)
            m_pendingResult = PendingResultCache(
                it, requestCode,
                resultCode, data
            )
        }
    }

    fun finishFragment(fragment: XWFragment) {
        // Log.d( TAG, "finishFragment()" );
        val ID = fragment.getCommitID()
        supportFragmentManager
            .popBackStack(ID, FragmentManager.POP_BACK_STACK_INCLUSIVE)
    }

    //////////////////////////////////////////////////////////////////////
    // FragmentManager.OnBackStackChangedListener
    //////////////////////////////////////////////////////////////////////
    override fun onBackStackChanged() {
        // make sure the right-most are visible
        val fragCount = supportFragmentManager.backStackEntryCount
        Log.i(TAG, "onBackStackChanged(); count now %d", fragCount)
        if (0 == fragCount) {
            finish()
        } else {
            if (fragCount == m_root!!.childCount - 1) {
                val child = m_root!!.getChildAt(fragCount)
                if (LOG_IDS) {
                    Log.i(
                        TAG, "onBackStackChanged(): removing view with id %x",
                        child.id
                    )
                }
                m_root!!.removeView(child)
            }
            setVisiblePanes()

            // If there's a pending on-result call, make it.
            if (null != m_pendingResult) {
                val target = m_pendingResult!!.getTarget()
                if (null != target) {
                    Log.i(TAG, "onBackStackChanged(): calling onActivityResult()")
                    target.onActivityResult(
                        m_pendingResult!!.m_request,
                        m_pendingResult!!.m_result,
                        m_pendingResult!!.m_data
                    )
                }
                m_pendingResult = null
            }
        }
        logPaneFragments()
    }

    private val topFragment: XWFragment?
        ////////////////////////////////////////////////////////////////////////
        private get() {
            var frag: XWFragment? = null
            val child = m_root!!.getChildAt(m_root!!.childCount - 1)
            if (null != child) {
                frag = findFragment(child)
            }
            return frag
        }

    private fun logPaneFragments() {
        if (BuildConfig.DEBUG) {
            val panePairs: MutableList<String?> = ArrayList()
            if (null != m_root) {
                val childCount = m_root!!.childCount
                for (ii in 0 until childCount) {
                    val child = m_root!!.getChildAt(ii)
                    val name = findFragment(child)!!.javaClass.getSimpleName()
                    val pair = String.format("%d:%s", ii, name)
                    panePairs.add(pair)
                }
            }
            val fm = supportFragmentManager
            val fragPairs: MutableList<String?> = ArrayList()
            val fragCount = fm.backStackEntryCount
            for (ii in 0 until fragCount) {
                val entry = fm.getBackStackEntryAt(ii)
                val name = entry.name
                val pair = String.format("%d:%s", ii, name)
                fragPairs.add(pair)
            }
            Log.d(
                TAG, "panes: [%s]; frags: [%s]", TextUtils.join(",", panePairs),
                TextUtils.join(",", fragPairs)
            )
        }
    }

    val visibleFragments: Array<XWFragment?>
        get() = getFragments(true)

    fun getFragments(visibleOnly: Boolean): Array<XWFragment?> {
        val childCount = m_root!!.childCount
        val count = if (visibleOnly) min(maxPanes().toDouble(), childCount.toDouble())
            .toInt() else childCount
        val result = arrayOfNulls<XWFragment>(count)
        for (ii in 0 until count) {
            val child = m_root!!.getChildAt(childCount - 1 - ii)
            result[ii] = findFragment(child)
        }
        return result
    }

    private fun maxPanes(): Int {
        val result: Int
        val orientation = resources.configuration.orientation
        result = if (XWPrefs.getIsTablet(this)
            && Configuration.ORIENTATION_LANDSCAPE == orientation
        ) {
            MAX_PANES_LANDSCAPE
        } else {
            1
        }
        // Log.d( TAG, "maxPanes() => %d", result );
        return result
    }

    private fun setVisiblePanes() {
        // hide all but the right-most m_maxPanes children
        val nPanes = m_root!!.childCount
        val maxPanes = maxPanes()
        for (ii in 0 until nPanes) {
            val child = m_root!!.getChildAt(ii)
            val visible = ii >= nPanes - maxPanes
            child.visibility = if (visible) View.VISIBLE else View.GONE
            setMenuVisibility(child, visible)
            if (visible) {
                trySetTitle(child)
            }
        }
        logPaneFragments()
    }

    private fun trySetTitle(view: View) {
        val frag = findFragment(view)
        if (null != frag) {
            frag.setTitle()
        } else {
            Log.d(TAG, "trySetTitle(): no fragment found")
        }
    }

    private fun setMenuVisibility(cont: View, visible: Boolean) {
        val frag: Fragment? = findFragment(cont)
        frag?.setMenuVisibility(visible)
    }

    private fun findFragment(view: View): XWFragment? {
        return XWFragment.findOwnsView(view)
    }

    private fun addFragmentImpl(
        fragment: XWFragment, bundle: Bundle?,
        parentName: String?
    ) {
        fragment.setArguments(bundle)
        addFragmentImpl(fragment, parentName)
    }

    private fun addFragmentImpl(
        fragment: XWFragment,
        parentName: String?
    ) {
        if (m_safeToCommit) {
            safeAddFragment(fragment, parentName)
        } else {
            assertOnUIThread()
            m_runWhenSafe.add(Runnable { safeAddFragment(fragment, parentName) })
        }
    }

    private fun popUnneeded(fm: FragmentManager, newName: String, parentName: String?) {
        val fragCount = fm.backStackEntryCount
        var lastKept = fragCount
        for (ii in 0 until fragCount) {
            val entry = fm.getBackStackEntryAt(ii)
            val entryName = entry.name
            if (entryName == newName) {
                lastKept = ii - 1 // keep only my parent; kill my same-class sibling!
                break
            } else if (entryName == parentName) {
                lastKept = ii
                break
            }
        }
        for (ii in fragCount - 1 downTo lastKept + 1) {
            fm.popBackStack()
        }
    }

    private fun safeAddFragment(fragment: XWFragment, parentName: String?) {
        Assert.assertTrue(m_safeToCommit)
        val newName = fragment.javaClass.getSimpleName()
        val fm = supportFragmentManager
        popUnneeded(fm, newName, parentName)
        val ID = fm.beginTransaction()
            .add(R.id.main_container, fragment, newName)
            .addToBackStack(newName)
            .commit()
        fragment.setCommitID(ID)
        // Don't do this. It causes an exception if e.g. from fragment.start()
        // I wind up launching another fragment and calling into this code
        // again. If I need executePendingTransactions() I'm doing something
        // else wrong.
        // fm.executePendingTransactions();
    }

    private fun setSafeToRun() {
        assertOnUIThread()
        m_safeToCommit = true
        for (proc in m_runWhenSafe) {
            proc.run()
        }
        m_runWhenSafe.clear()
    }

    companion object {
        private val TAG = MainActivity::class.java.getSimpleName()
        private const val MAX_PANES_LANDSCAPE = 2
        private const val LOG_IDS = true
    }
}

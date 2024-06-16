/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
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

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.OnHierarchyChangeListener
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView

import java.io.Serializable

import org.eehouse.android.xw4.DBUtils.SentInvitesInfo
import org.eehouse.android.xw4.Utils.OnNothingSelDoesNothing

private val TAG: String = InviteDelegate::class.java.simpleName

abstract class InviteDelegate(delegator: Delegator) :
    DelegateBase(delegator, R.layout.inviter, R.menu.empty), View.OnClickListener,
    OnHierarchyChangeListener
{
    interface InviterItem {
        fun equals(item: InviterItem?): Boolean

        // the string that identifies this item in results
        fun getDev(): String
    }

    protected class TwoStringPair(private val mDev: String, var str2: String?) : InviterItem,
        Serializable {
        override fun equals(item: InviterItem?): Boolean {
            var result = false
            if (null != item) {
                val pair = item as TwoStringPair
                result = (mDev == pair.getDev()) && ((null == str2 && null == pair.str2)
                        || (str2 == pair.str2))
                Log.d(TAG, "%s.equals(%s) => %b", mDev, pair.getDev(), result)
            }
            return result
        }

        override fun getDev(): String { return mDev }

        override fun toString(): String {
            return String.format("{dev: \"%s\", str2: \"%s\"}", mDev, str2)
        }
    }

    protected var m_nMissing: Int
    protected var m_lastDev: String?
    protected var m_inviteButton: Button? = null
    private val m_activity = delegator.getActivity()
    private var m_lv: LinearLayout? = null
    private var m_ev: TextView? = null
    protected var m_counts: MutableMap<InviterItem?, Int>
    private var m_checked: HashSet<String>
    private var m_setChecked = false
    private val m_remotesAreRobots: Boolean

    init {
        val intent = intent
        m_nMissing = intent.getIntExtra(INTENT_KEY_NMISSING, -1)
        m_lastDev = intent.getStringExtra(INTENT_KEY_LASTDEV)
        m_remotesAreRobots = intent.getBooleanExtra(RAR, false)
        m_counts = HashMap()
        m_checked = HashSet()
    }

    override fun init(sis: Bundle?) {
        // DO NOT CALL super!!!
        getBundledData(sis)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        addBundledData(outState)
        super.onSaveInstanceState(outState)
    }

    private fun getBundledData(bundle: Bundle?)
    {
        if (null != bundle) {
            m_checked.clear()
            val checked = bundle.getSerializable(KEY_CHECKED) as HashSet<String>?
            if (null != checked) {
                m_checked.addAll(checked)
            }
        }
    }

    private fun addBundledData(bundle: Bundle) {
        bundle.putSerializable(KEY_CHECKED, m_checked)
    }

    protected fun init(descTxt: String, emptyMsgId: Int) {
        m_inviteButton = findViewById(R.id.button_invite) as Button
        m_inviteButton!!.setOnClickListener(this)

        val descView = findViewById(R.id.invite_desc) as TextView
        descView.text = descTxt

        val extraID = getExtra()
        if (0 != extraID) {
            val extraView = findViewById(R.id.invite_extra) as TextView
            extraView.text = getString(extraID)
            extraView.visibility = View.VISIBLE
        }

        m_lv = findViewById(R.id.invitees) as LinearLayout
        m_ev = findViewById(R.id.empty) as TextView
        if (null != m_lv && null != m_ev && 0 != emptyMsgId) {
            m_ev!!.text = getString(emptyMsgId)
            m_lv!!.setOnHierarchyChangeListener(this)
            showEmptyIfEmpty()
        }

        tryEnable()
    }

    // Children implement ...
    abstract fun onChildAdded(child: View, item: InviterItem)

    // Implement this if you want to insert descriptive text
    open fun getExtra(): Int { return 0 }

    // Subclasses are meant to call this
    protected fun addButtonBar(buttonBarId: Int, buttonBarItemIds: IntArray)
    {
        val container = findViewById(R.id.button_bar) as FrameLayout
        val bar = inflate(buttonBarId) as ViewGroup
        container.addView(bar)

        val listener = View.OnClickListener { view -> onBarButtonClicked(view.id) }

        for (id in buttonBarItemIds) {
            bar.findViewById<View>(id).setOnClickListener(listener)
        }

        tryEnable()
    }

    protected fun updateList(items: List<InviterItem>) {
        updateList(R.layout.two_strs_item, items)
    }

    protected fun updateList(itemId: Int, items: List<InviterItem>) {
        updateChecked(items)

        m_lv!!.removeAllViews()
        val itemsArr = items.toTypedArray<InviterItem>()
        for (item in itemsArr) {
            m_lv!!.addView(makeViewFor(itemId, item))
        }
    }

    abstract fun onBarButtonClicked(id: Int)

    ////////////////////////////////////////
    // View.OnClickListener
    ////////////////////////////////////////
    override fun onClick(view: View) {
        if (m_inviteButton === view) {
            val len = m_checked!!.size

            val items = selItems
            val devs = arrayOfNulls<String>(items.size)
            for (ii in items.indices) {
                devs[ii] = items[ii]!!.getDev()
            }

            val counts = IntArray(len)
            for (ii in 0 until len) {
                counts[ii] = m_counts[items[ii]]!!
            }

            val intent = Intent()
            intent.putExtra(DEVS, devs)
            intent.putExtra(COUNTS, counts)
            intent.putExtra(RAR, m_remotesAreRobots)
            setResult(Activity.RESULT_OK, intent)
            finish()
        }
    }

    private val selItems: Array<InviterItem?>
        get() {
            val list: MutableList<InviterItem?> = ArrayList()
            val next = 0
            for (ii in 0 until m_lv!!.childCount) {
                val child = m_lv!!.getChildAt(ii) as InviterItemFrame
                val item = child.item
                if (m_checked!!.contains(item!!.getDev())) {
                    list.add(item)
                    Assert.assertTrue(child.getChecked() || !BuildConfig.DEBUG)
                }
            }
            return list.toTypedArray<InviterItem?>()
        }

    ////////////////////////////////////////
    // ViewGroup.OnHierarchyChangeListener
    ////////////////////////////////////////
    override fun onChildViewAdded(parent: View, child: View) {
        showEmptyIfEmpty()
    }

    override fun onChildViewRemoved(parent: View, child: View) {
        showEmptyIfEmpty()
    }

    private fun showEmptyIfEmpty() {
        val count = m_lv!!.childCount
        m_ev!!.visibility = if (0 == count) View.VISIBLE else View.GONE
    }

    protected open fun tryEnable() {
        val count = m_checked!!.size
        m_inviteButton!!.isEnabled = count > 0 && count <= m_nMissing
    }

    fun getChecked(): Set<String> { return m_checked!! }

    protected fun clearChecked() {
        m_checked!!.clear()
    }

    // Figure which previously-checked items belong in the new set.
    private fun updateChecked(newItems: List<InviterItem>) {
        val old: MutableSet<String> = HashSet()
        old.addAll(m_checked!!)
        m_checked!!.clear()

        val iter: Iterator<String> = old.iterator()
        while (iter.hasNext()) {
            val oldDev = iter.next()
            for (item in newItems) {
                if (item.getDev() == oldDev) {
                    m_checked!!.add(oldDev)
                    break
                }
            }
        }
    }

    // callbacks made by InviteItemsAdapter
    protected fun onItemChecked(item: InviterItem, checked: Boolean) {
        val dev = item.getDev()
        if (checked) {
            m_checked!!.add(dev)
        } else {
            m_checked!!.remove(dev)
        }
    }

    private fun makeViewFor(itemID: Int, item: InviterItem): View {
        val layout = inflate(R.layout.inviter_item_frame) as InviterItemFrame
        layout.item = item

        // Give subclass a chance to install and populate its view
        val child = inflate(itemID)
        (layout.findViewById<View>(R.id.frame) as FrameLayout).addView(child)
        onChildAdded(child, item)

        m_counts[item] = 1
        if (XWPrefs.getCanInviteMulti(m_activity!!) && 1 < m_nMissing) {
            val spinner = layout.findViewById<View>(R.id.nperdev_spinner) as Spinner
            val adapter =
                ArrayAdapter<String>(
                    m_activity, android.R.layout
                        .simple_spinner_item
                )
            for (ii in 1..m_nMissing) {
                val str = getQuantityString(R.plurals.nplayers_fmt, ii, ii)
                adapter.add(str)
            }
            spinner.adapter = adapter
            spinner.visibility = View.VISIBLE
            spinner.onItemSelectedListener = object : OnNothingSelDoesNothing() {
                override fun onItemSelected(
                    parent: AdapterView<*>?,
                    view: View, pos: Int,
                    id: Long
                ) {
                    m_counts[item] = 1 + pos
                    tryEnable()
                }
            }
        }

        layout.setOnCheckedChangeListener { buttonView, isChecked ->
            if (!isChecked) {
                m_setChecked = false
            }
            onItemChecked(item, isChecked)
            tryEnable()
        }

        val dev = item.getDev()
        var setIt = false
        if (m_setChecked || m_checked!!.contains(dev)) {
            setIt = true
        } else if (null != m_lastDev && m_lastDev == dev) {
            m_lastDev = null
            setIt = true
        }
        layout.setChecked(setIt)

        return layout
    }

    companion object {
        const val DEVS: String = "DEVS"
        const val COUNTS: String = "COUNTS"
        const val RAR: String = "RAR"
        private const val INTENT_KEY_NMISSING = "NMISSING"
        @JvmStatic
        // buggy kotlin, e.g. https://youtrack.jetbrains.com/issue/KT-39868/Allow-access-to-protected-consts-and-fields-from-a-super-companion-object
        @Suppress("JVM_STATIC_ON_CONST_OR_JVM_FIELD")
        const val INTENT_KEY_LASTDEV: String = "LDEV"
        private const val KEY_CHECKED = "CHECKED"

        fun makeIntent(
            activity: Activity, target: Class<*>?,
            nMissing: Int, info: SentInvitesInfo?
        ): Intent {
            val intent = Intent(activity, target)
                .putExtra(INTENT_KEY_NMISSING, nMissing)
            if (null != info) {
                intent.putExtra(RAR, info.remotesRobots)
            }
            return intent
        }
    }
}

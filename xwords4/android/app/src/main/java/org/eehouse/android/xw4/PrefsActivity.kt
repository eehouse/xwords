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

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat

import org.eehouse.android.xw4.DlgDelegate.Action
import org.eehouse.android.xw4.DlgDelegate.HasDlgDelegate
import org.eehouse.android.xw4.gen.PrefsWrappers.prefs
import org.eehouse.android.xw4.jni.CommonPrefs

class PrefsActivity : XWActivity(), Delegator, HasDlgDelegate,
    PreferenceFragmentCompat.OnPreferenceStartFragmentCallback,
    PreferenceFragmentCompat.OnPreferenceDisplayDialogCallback {
    private var mDlgt: PrefsDelegate? = null

    internal interface DialogProc {
        fun makeDialogFrag(): XWDialogFragment
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        mDlgt = PrefsDelegate(this, this, savedInstanceState)
        super.onCreate(savedInstanceState, mDlgt!!)
        val layoutID = mDlgt!!.layoutID
        Assert.assertTrue(0 < layoutID)
        mDlgt!!.setContentView(layoutID)
        var rootFrag: PreferenceFragmentCompat
        try {
            val rootName = intent.extras!!.getString(CLASS_NAME)
            Assert.assertTrueNR(null != rootName)
            val clazz = Class.forName(rootName!!)
            rootFrag = clazz.newInstance() as PreferenceFragmentCompat
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
            rootFrag = prefs()
            Assert.failDbg()
        }
        mDlgt!!.setRootFragment(rootFrag)
        supportFragmentManager
            .beginTransaction()
            .replace(R.id.main_container, rootFrag)
            .commit()
    }

    override fun makeOkOnlyBuilder(msg: String): DlgDelegate.Builder {
        return mDlgt!!.makeOkOnlyBuilder(msg)
    }

    override fun makeNotAgainBuilder(
        key: Int, action: Action, msgID: Int,
        vararg params: Any?
    ): DlgDelegate.Builder {
        return mDlgt!!.makeNotAgainBuilder(key, action, msgID, *params)
    }

    override fun onPreferenceDisplayDialog(
        caller: PreferenceFragmentCompat, pref: Preference
    ): Boolean {
        val success = pref is DialogProc
        if (success) {
            show((pref as DialogProc).makeDialogFrag())
        } else {
            Log.e(TAG, "class %s not a DialogProc",
                  pref.javaClass.getSimpleName())
        }
        return success
    }

    override fun onPreferenceStartFragment(
        caller: PreferenceFragmentCompat,
        pref: Preference
    ): Boolean {
        val args = pref.getExtras()
        val fragment = supportFragmentManager.getFragmentFactory()
            .instantiate(classLoader, pref.fragment!!)
        fragment.setArguments(args)
        fragment.setTargetFragment(caller, 0)
        supportFragmentManager.beginTransaction()
            .replace(R.id.main_container, fragment)
            .addToBackStack(null)
            .commit()
        setTitle(pref.title)
        return true
    }

    fun makeConfirmThenBuilder(action: Action, msg: String): DlgDelegate.Builder {
        return mDlgt!!.makeConfirmThenBuilder(action, msg)
    }

    fun showSMSEnableDialog(action: Action) {
        mDlgt!!.showSMSEnableDialog(action)
    }

    // Every subscreen in the prefs.xml heierarchy has to have a class
    // associated with it just to provide its xml-file ID. Stupid design; not
    // mine! To make this a bit less gross, the classes are generated in
    // gen/PrefsWrappers.java for files matching the pattern
    // main/res/xml/prefs*.xml.
    //
    // See the notes in res/xml/prefs.xml
    abstract class BasePrefsFrag : PreferenceFragmentCompat() {
        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
            setPreferencesFromResource(getResID(), rootKey)
        }

        override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
            val context = view.context
            val hideSet = getHideSet(context)
            for (key in hideSet) {
                val pref = findPreference<Preference>(
                    key
                )
                if (null != pref) {
                    Log.d(
                        TAG, "in %s, found pref %s", javaClass.getSimpleName(),
                        pref.title
                    )
                    pref.isVisible = false
                }
            }
            super.onViewCreated(view, savedInstanceState)
        }

        abstract fun getResID(): Int
    }

    companion object {
        private val TAG = PrefsActivity::class.java.getSimpleName()
        private const val CLASS_NAME = "CLASS_NAME"
        private var sHideSet: MutableSet<String>? = null
        @Synchronized
        private fun getHideSet(context: Context): Set<String> {
            if (null == sHideSet) {
                val hidees: MutableSet<Int> = HashSet()
                if (!Utils.isGSMPhone(context) || Perms23.haveNativePerms()) {
                    hidees.add(R.string.key_enable_nbs)
                }
                hidees.add(R.string.key_hide_title)
                if (!BuildConfig.WIDIR_ENABLED) {
                    hidees.add(R.string.key_enable_p2p)
                }
                if (BuildConfig.DEBUG) {
                    hidees.add(R.string.key_logging_on)
                    hidees.add(R.string.key_enable_debug)
                } else {
                    hidees.add(R.string.key_unhide_dupmode)
                }
                if (CommonPrefs.getDupModeHidden(context)) {
                    hidees.add(R.string.key_init_dupmodeon)
                }
                sHideSet = HashSet()
                hidees.map{key -> sHideSet!!.add(context.getString(key))}
            }
            return sHideSet!!
        }

        fun bundleRoot(root: Class<*>, intent: Intent) {
            Assert.assertTrueNR(null == intent.extras)
            val bundle = Bundle()
                .putCharSequenceAnd(CLASS_NAME, root.getName())
            intent.putExtras(bundle)
        }
    }
}

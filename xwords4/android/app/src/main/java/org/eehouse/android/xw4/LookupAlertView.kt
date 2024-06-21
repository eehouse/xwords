/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
 * Copyright 2009 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.View
import android.widget.AdapterView
import android.widget.AdapterView.OnItemClickListener
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.TextView
import org.eehouse.android.xw4.Utils.ISOCode
import org.eehouse.android.xw4.loc.LocUtils

class LookupAlertView
// These two are probably always the same object
    (private val mContext: Context, aset: AttributeSet?) :
    LinearLayout(mContext, aset), View.OnClickListener,
    DialogInterface.OnKeyListener, OnItemClickListener
{
    interface OnDoneListener {
        fun onDone()
    }

    private var m_onDone: OnDoneListener? = null
    private var m_listView: ListView? = null
    private var m_words: Array<String>? = null
    private var m_studyOn = false
    private var m_wordIndex = 0
    private var m_urlIndex = 0
    private var m_state = 0
    private var m_wordsAdapter: ArrayAdapter<String>? = null
    private var m_doneButton: Button? = null
    private var m_studyButton: Button? = null
    private var m_summary: TextView? = null

    fun init(lstn: OnDoneListener, bundle: Bundle) {
        m_onDone = lstn
        m_words = bundle.getStringArray(WORDS)
        val isoCode = ISOCode.newIf(bundle.getString(LANG))
        setLang(mContext, isoCode)
        m_studyOn = (XWPrefs.getStudyEnabled(mContext)
                && bundle.getBoolean(STUDY_ON, true))

        m_state = bundle.getInt(STATE, STATE_WORDS)
        m_wordIndex = bundle.getInt(WORDINDEX, 0)
        m_urlIndex = bundle.getInt(URLINDEX, 0)

        m_wordsAdapter = ArrayAdapter(mContext, LIST_LAYOUT, m_words!!)
        m_listView = findViewById<View>(android.R.id.list) as ListView
        m_listView!!.onItemClickListener = this

        m_doneButton = findViewById<View>(R.id.button_done) as Button
        m_doneButton!!.setOnClickListener(this)
        m_studyButton = findViewById<View>(R.id.button_study) as Button
        if (m_studyOn) {
            m_studyButton!!.setOnClickListener(this)
        } else {
            m_studyButton!!.visibility = GONE
        }

        m_summary = findViewById<View>(R.id.summary) as TextView

        switchState()
        if (1 == m_words!!.size) {
            // imitate onItemClick() on the 0th elem
            Assert.assertTrueNR(STATE_WORDS == m_state)
            Assert.assertTrueNR(m_wordIndex == 0)
            switchState(true)
        }
    }

    // NOT @Override!!!
    fun saveInstanceState(bundle: Bundle) {
        addParams(bundle, m_words, s_lang, m_studyOn)
        bundle.putInt(STATE, m_state)
        bundle.putInt(WORDINDEX, m_wordIndex)
        bundle.putInt(URLINDEX, m_urlIndex)
    }

    //////////////////////////////////////////////////////////////////////
    // View.OnClickListener
    //////////////////////////////////////////////////////////////////////
    override fun onClick(view: View) {
        if (view === m_doneButton) {
            switchState(false)
        } else if (view === m_studyButton) {
            val word = m_words!![m_wordIndex]
            if (DBUtils.addToStudyList(mContext, word, s_lang!!)) {
                val msg = LocUtils.getString(
                    mContext, R.string.add_done_fmt,
                    word, s_langName
                )
                Utils.showToast(mContext, msg)
            }
        }
    }

    //////////////////////////////////////////////////////////////////////
    // AdapterView.OnItemClickListener
    //////////////////////////////////////////////////////////////////////
    override fun onItemClick(
        parentView: AdapterView<*>?, view: View,
        position: Int, id: Long
    ) {
        if (STATE_WORDS == m_state) {
            m_wordIndex = position
        } else if (STATE_URLS == m_state) {
            m_urlIndex = position
        } else {
            Assert.failDbg()
        }
        switchState(true)
    }

    private fun adjustState(forward: Boolean) {
        val incr = if (forward) 1 else -1
        m_state += incr
        while (true) {
            val curState = m_state
            if (STATE_WORDS == m_state && 1 >= m_words!!.size) {
                m_state += incr
            }
            if (STATE_URLS == m_state &&
                (1 >= s_lookupUrls!!.size && !m_studyOn)
            ) {
                m_state += incr
            }
            if (m_state == curState) {
                break
            }
        }
    }

    private fun switchState(forward: Boolean) {
        adjustState(forward)
        switchState()
    }

    private fun switchState() {
        when (m_state) {
            STATE_DONE -> m_onDone!!.onDone()
            STATE_WORDS -> {
                m_listView!!.adapter = m_wordsAdapter
                setSummary(if (m_studyOn) R.string.title_lookup_study else R.string.title_lookup)
                m_doneButton!!.setText(R.string.button_done)
                m_studyButton!!.visibility = GONE
            }

            STATE_URLS -> {
                m_listView!!.adapter = s_urlsAdapter
                setSummary(m_words!![m_wordIndex])
                var txt = LocUtils.getString(
                    mContext, R.string.button_done_fmt,
                    m_words!![m_wordIndex]
                )
                m_doneButton!!.text = txt
                txt = LocUtils.getString(
                    mContext, R.string.add_to_study_fmt,
                    m_words!![m_wordIndex]
                )
                if (m_studyOn) {
                    m_studyButton!!.visibility = VISIBLE
                    m_studyButton!!.text = txt
                }
            }

            STATE_LOOKUP -> {
                lookupWord(
                    mContext, m_words!![m_wordIndex],
                    s_lookupUrls!![m_urlIndex]
                )
                switchState(false)
            }

            else -> Assert.failDbg()
        }
    } // switchState

    private fun lookupWord(context: Context, word: String, fmt: String) {
        val dict_url = String.format(fmt, s_lang, word)
        val uri = Uri.parse(dict_url)
        val intent = Intent(Intent.ACTION_VIEW, uri)
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)

        try {
            context.startActivity(intent)
        } catch (anfe: ActivityNotFoundException) {
            Log.ex(TAG, anfe)
        }
    } // lookupWord

    private fun setLang(context: Context, isoCode: ISOCode?) {
        if (!isoCode!!.equals(s_lang)) {
            val urls = context.resources.getStringArray(R.array.lookup_urls)
            val tmpUrls = ArrayList<String>()
            val tmpNames = ArrayList<String>()
            val langCode = String.format(":%s:", isoCode)
            var ii = 0
            while (ii < urls.size) {
                val codes = urls[ii + 1]
                if (0 == codes.length || codes.contains(langCode)) {
                    val url = urls[ii + 2]
                    if (!tmpUrls.contains(url)) {
                        tmpNames.add(urls[ii])
                        tmpUrls.add(url)
                    }
                }
                ii += 3
            }
            s_lookupNames = tmpNames.toTypedArray<String>()
            s_lookupUrls = tmpUrls.toTypedArray<String>()
            s_urlsAdapter = ArrayAdapter(
                context, LIST_LAYOUT,
                s_lookupNames!!
            )
            s_lang = isoCode
            s_langName = DictLangCache.getLangNameForISOCode(context, isoCode)
        }
    }

    private fun setSummary(id: Int) {
        m_summary!!.text = LocUtils.getString(mContext, id)
    }

    private fun setSummary(word: String) {
        val title =
            LocUtils.getString(mContext, R.string.pick_url_title_fmt, word)
        m_summary!!.text = title
    }

    //////////////////////////////////////////////////////////////////////
    // Dialog.OnKeyListener interface
    //////////////////////////////////////////////////////////////////////
    override fun onKey(arg0: DialogInterface, keyCode: Int, event: KeyEvent): Boolean {
        val handled = (keyCode == KeyEvent.KEYCODE_BACK
                && KeyEvent.ACTION_UP == event.action)
        if (handled) {
            switchState(false)
        }
        return handled
    }

    companion object {
        private val TAG: String = LookupAlertView::class.java.simpleName

        private const val WORDS = "WORDS"
        private const val LANG = "LANG"
        private const val STUDY_ON = "STUDY_ON"
        private const val STATE = "STATE"
        private const val WORDINDEX = "WORDINDEX"
        private const val URLINDEX = "URLINDEX"

        private const val STATE_DONE = 0
        private const val STATE_WORDS = 1
        private const val STATE_URLS = 2
        private const val STATE_LOOKUP = 3

        private var s_lookupNames: Array<String>? = null
        private var s_lookupUrls: Array<String>? = null
        private var s_urlsAdapter: ArrayAdapter<String>? = null
        private const val LIST_LAYOUT = android.R.layout.simple_list_item_1

        private var s_lang: ISOCode? = null
        private var s_langName: String? = null

        private fun addParams(
            bundle: Bundle, words: Array<String>?,
            isoCode: ISOCode?, studyOn: Boolean
        ) {
            bundle.putStringArray(WORDS, words)
            bundle.putString(LANG, isoCode.toString())
            bundle.putBoolean(STUDY_ON, studyOn)
        }

        fun makeParams(
            words: Array<String>?, isoCode: ISOCode?,
            noStudyOption: Boolean
        ): Bundle {
            val bundle = Bundle()
            addParams(bundle, words, isoCode, !noStudyOption)
            return bundle
        }
    }
}

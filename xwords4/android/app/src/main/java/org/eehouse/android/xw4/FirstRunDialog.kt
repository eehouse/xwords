/*
 * Copyright 2010 - 2024 by Eric House (xwords@eehouse.org).  All rights
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

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import android.webkit.WebView
import android.webkit.WebViewClient
import org.eehouse.android.xw4.loc.LocUtils

/* Put up a dialog greeting user after every upgrade.  Based on
 * similar feature in OpenSudoku, to whose author "Thanks".
 */
class FirstRunDialog : XWDialogFragment() {
    override fun onCreateDialog(sis: Bundle?): Dialog {
        val context: Context = requireActivity()
        // boolean showSurvey = !Utils.onFirstVersion( context );

        // This won't support e.g mailto refs.  Probably want to
        // launch the browser with an intent eventually.
        val view = WebView(context)
        view.setWebViewClient(object : WebViewClient() {
            private val m_loaded = false
            override fun shouldOverrideUrlLoading(
                view: WebView,
                url: String
            ): Boolean {
                var result = false
                if (url.startsWith("mailto:")) {
                    Utils.emailAuthor(context)
                    result = true
                }
                return result
            } // @Override
            // public void onPageFinished(WebView view, String url)
            // {
            //     if ( !m_loaded ) {
            //         m_loaded = true;
            //         if ( showSurvey ) {
            //             view.loadUrl( "javascript:showSurvey();" );
            //         }
            //     }
            // }
        })
        // view.getSettings().setJavaScriptEnabled( true ); // for surveymonkey
        view.loadUrl("file:///android_asset/changes.html")
        return LocUtils.makeAlertBuilder(context)
            .setIcon(android.R.drawable.ic_menu_info_details)
            .setTitle(R.string.changes_title)
            .setView(view)
            .setPositiveButton(android.R.string.ok, null)
            .create()
    }

    override fun getFragTag(): String {
        return TAG
    }

    companion object {
        private val TAG = FirstRunDialog::class.java.getSimpleName()
        fun newInstance(): FirstRunDialog {
            return FirstRunDialog()
        }
    }
}

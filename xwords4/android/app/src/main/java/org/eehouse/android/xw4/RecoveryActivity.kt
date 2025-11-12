/*
 * Copyright 2009 - 2025 by Eric House (xwords@eehouse.org).  All
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
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteDatabase.OPEN_READWRITE
import android.os.Bundle
import android.os.Environment
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import java.io.File


class RecoveryActivity: Activity(), View.OnClickListener {
    private val FILE_NAME_CODE = 10001
    private var mDB: SQLiteDatabase? = null
    private var mNextCmd: Int = -1

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.recovery)

        val path = getDatabasePath(BuildConfig.DB_NAME).toString()
        mDB = SQLiteDatabase.openDatabase(path, null, OPEN_READWRITE)
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        arrayOf(R.id.loaddb, R.id.addquery, R.id.export_db, R.id.import_db)
            .map { findViewById<Button>(it).setOnClickListener(this) }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        when (requestCode) {
            FILE_NAME_CODE -> {
                data?.data?.let { uri ->
                    val whats = List<ZipUtils.SaveWhat>(1) { ZipUtils.SaveWhat.GAMES }
                    Log.d(TAG, "passing $whats to save()")
                    ZipUtils.save(this, uri, whats)
                }
            }
        }
    }

    override fun onClick(view: View?) {
        when ( view!!.id ) {
            R.id.loaddb ->
                try {
                    val query = findViewById<EditText>(R.id.query_edit).text.toString()
                    val cursor = mDB?.rawQuery(query, null)
                    val lines = ArrayList<String>()
                    cursor?.let { cursor ->
                        while (cursor.moveToNext()) {
                            val str = (0..<cursor.columnCount).map{cursor.getString(it)}.joinToString( separator="|" )
                            lines.add(str)
                        }
                        cursor.close()
                    }
                    findViewById<TextView>(R.id.results).setText(lines.joinToString(separator="\n"))
                } catch (ex: Exception) {
                    Log.d(TAG, "exception: ${ex.toString()}sele")
                }
            R.id.addquery -> {
                mNextCmd = (mNextCmd + 1) % sCmds.size
                val query = sCmds[mNextCmd]
                findViewById<TextView>(R.id.query_edit).setText(query)
            }
            R.id.export_db -> {
                val intent = Intent(Intent.ACTION_CREATE_DOCUMENT)
                intent.addCategory(Intent.CATEGORY_OPENABLE)
                intent.setType(ZipUtils.getMimeType(true))
                intent.putExtra(
                    Intent.EXTRA_TITLE, ZipUtils.getFileName(this)
                )
                startActivityForResult(intent, FILE_NAME_CODE)
                // ZipUtils.save(this, uri, List<SaveWhat>(SaveWhat.GAMES))
            }
            R.id.import_db -> {}
            else -> Log.d(TAG, "no hander")
        }
    }

    companion object {
        private val TAG = RecoveryActivity::class.java.getSimpleName()
        private val fileName = BuildConfig.APPLICATION_ID.split(".")
            .last() + "Recover.txt"
        private val sCmds =
            arrayOf( "select key from pairs where key like 'games/%/%'",
                     "select key from pairs where key like 'groups/%/%'",
                     "delete from pairs where key like 'games/%' or key like 'groups/%' or key = 'gmgr/state'",
            )

        fun active(): Boolean {
            val downloadsDirectory = Environment
                .getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            val path = File(downloadsDirectory, fileName)
            Log.d(TAG, "$fileName exists: ${path.exists()}")
            return path.exists()
        }

        fun start(parent: Activity) {
            val myIntent = Intent(parent, RecoveryActivity::class.java)
            parent.startActivity(myIntent)
        }
    }
}

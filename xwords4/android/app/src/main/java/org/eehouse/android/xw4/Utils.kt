/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */ /*
 * Copyright 2009-2010 by Eric House (xwords@eehouse.org).  All
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
import android.app.AlertDialog
import android.app.Dialog
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.AssetManager
import android.content.res.Configuration
import android.media.RingtoneManager
import android.net.Uri
import android.os.Build
import android.os.Looper
import android.provider.ContactsContract.PhoneLookup
import android.telephony.PhoneNumberUtils
import android.telephony.TelephonyManager
import android.text.TextUtils
import android.util.Base64
import android.view.Menu
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.AdapterView.OnItemSelectedListener
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.core.app.NotificationCompat
import androidx.core.content.FileProvider
import org.eehouse.android.xw4.Assert.assertNotNull
import org.eehouse.android.xw4.Assert.assertTrue
import org.eehouse.android.xw4.Assert.assertTrueNR
import org.eehouse.android.xw4.Assert.assertVarargsNotNullNR
import org.eehouse.android.xw4.Assert.failDbg
import org.eehouse.android.xw4.Channels.ID
import org.eehouse.android.xw4.DictUtils.DictAndLoc
import org.eehouse.android.xw4.Perms23.Perm
import org.eehouse.android.xw4.jni.CommonPrefs
import org.eehouse.android.xw4.jni.XwJNI.Companion.dvc_getMQTTDevID
import org.eehouse.android.xw4.loc.LocUtils
import java.io.BufferedReader
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.InputStream
import java.io.InputStreamReader
import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import java.io.Serializable
import java.security.MessageDigest
import java.security.NoSuchAlgorithmException
import java.util.Formatter
import java.util.Locale
import java.util.Random
import kotlin.math.min

object Utils {
    private val TAG: String = Utils::class.java.simpleName
    const val TURN_COLOR: Int = 0x7F00FF00

    private const val DB_PATH = "XW_GAMES"
    private const val HIDDEN_PREFS = "xwprefs_hidden"
    private const val FIRST_VERSION_KEY = "FIRST_VERSION_KEY"
    private const val SHOWN_VERSION_KEY = "SHOWN_VERSION_KEY"

    private val sDefaultChannel = ID.GAME_EVENT

    private var s_isFirstBootThisVersion: Boolean? = null
    private var s_firstVersion: Boolean? = null
    private var s_isFirstBootEver: Boolean? = null
    private var s_appVersion: Int? = null
    private val s_phonesHash = HashMap<String, String?>()
    private var s_hasSmallScreen: Boolean? = null
    private val s_random = Random()

    @JvmStatic
    fun nextRandomInt(): Int {
        return s_random.nextInt()
    }

    fun onFirstVersion(context: Context): Boolean {
        setFirstBootStatics(context)
        return s_firstVersion!!
    }

    fun firstBootEver(context: Context): Boolean {
        setFirstBootStatics(context)
        return s_isFirstBootEver!!
    }

    @JvmStatic
    fun firstBootThisVersion(context: Context): Boolean {
        setFirstBootStatics(context)
        return s_isFirstBootThisVersion!!
    }

    fun isGSMPhone(context: Context): Boolean {
        var result = false
        if (Perms23.havePermissions(context, Perm.READ_PHONE_STATE, Perm.READ_PHONE_NUMBERS)) {
            val info = SMSPhoneInfo.get(context)
            result = null != info && info.isPhone && info.isGSM
        }
        Log.d(TAG, "isGSMPhone() => %b", result)
        return result
    }

    // Does the device have ability to send SMS -- e.g. is it a phone and not
    // a Kindle Fire.  Not related to XWApp.SMSSUPPORTED.  Note that as a
    // temporary workaround for KitKat having broken use of non-data messages,
    // we only support SMS on kitkat if data messages have been turned on (and
    // that's not allowed except on GSM phones.)
    @JvmStatic
    fun deviceSupportsNBS(context: Context): Boolean {
        var result = false
        if (Perms23.haveNBSPerms(context)) {
            val tm = context.getSystemService(Context.TELEPHONY_SERVICE) as TelephonyManager
            if (null != tm) {
                val type = tm.phoneType
                result = TelephonyManager.PHONE_TYPE_GSM == type
            }
        }
        Log.d(TAG, "deviceSupportsNBS() => %b", result)
        return result
    }

    fun notImpl(context: Context) {
        val text = "Feature coming soon"
        showToast(context, text)
    }

    @JvmStatic
    fun showToast(
        context: Context,
        msg: String?
    ) {
        // Make this safe to call from non-looper threads
        val activity = DelegateBase.getHasLooper()
        activity?.runOnUiThread {
            try {
                Toast.makeText(context, msg, Toast.LENGTH_SHORT).show()
            } catch (re: RuntimeException) {
                Log.ex(TAG, re)
            }
        }
    }

    @JvmStatic
    fun showToast(context: Context, id: Int, vararg args: Any?) {
        assertVarargsNotNullNR(*args)
        var msg = LocUtils.getString(context, id)
        msg = Formatter().format(msg, *args).toString()
        showToast(context, msg)
    }

    @JvmOverloads
    @JvmStatic
    fun emailAuthor(context: Context, msg: String? = null) {
        emailAuthorImpl(
            context, msg, R.string.email_author_subject,
            R.string.email_author_chooser, null
        )
    }

    @JvmStatic
    fun emailLogFile(context: Context, attachment: File?) {
        val msg = LocUtils.getString(context, R.string.email_logs_msg)
        emailAuthorImpl(
            context, msg, R.string.email_logs_subject,
            R.string.email_logs_chooser, attachment
        )
    }

    private fun emailAuthorImpl(
        context: Context, msg: String?, subject: Int,
        chooser: Int, attachment: File?
    ) {
        val intent = Intent(Intent.ACTION_SEND)
        intent.setType("message/rfc822") // force email
        intent.putExtra(
            Intent.EXTRA_SUBJECT,
            LocUtils.getString(context, subject)
        )
        val addrs = arrayOf(
            LocUtils.getString(
                context,
                R.string.email_author_email
            )
        )
        intent.putExtra(Intent.EXTRA_EMAIL, addrs)

        if (null != attachment) {
            val uri = FileProvider.getUriForFile(
                context,
                context.packageName + ".provider",
                attachment
            )
            intent.putExtra(Intent.EXTRA_STREAM, uri)
        }

        val devID = dvc_getMQTTDevID()
        var body = LocUtils.getString(
            context, R.string.email_body_rev_fmt,
            BuildConfig.GIT_REV, Build.MODEL,
            Build.VERSION.RELEASE, devID
        )
        if (null != msg) {
            body += """
                
                
                $msg
                """.trimIndent()
        }
        intent.putExtra(Intent.EXTRA_TEXT, body)
        val chooserMsg = LocUtils.getString(context, chooser)
        context.startActivity(Intent.createChooser(intent, chooserMsg))
    }

    @JvmStatic
    fun gitInfoToClip(context: Context) {
        var sb: StringBuilder?
        try {
            val `is` = context.assets.open(
                BuildConfig.BUILD_INFO_NAME,
                AssetManager.ACCESS_BUFFER
            )
            val reader = BufferedReader(InputStreamReader(`is`))
            sb = StringBuilder()
            while (true) {
                val line = reader.readLine() ?: break
                sb.append(line).append("\n")
            }
            reader.close()
        } catch (ex: Exception) {
            sb = null
        }

        if (null != sb) {
            stringToClip(context, sb.toString())
        }
    }

    @JvmStatic
    fun stringToClip(context: Context, str: String?) {
        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val label = LocUtils.getString(context, R.string.clip_label)
        val clip = ClipData.newPlainText(label, str)
        clipboard.setPrimaryClip(clip)
    }

    @JvmStatic
    fun postNotification(
        context: Context, intent: Intent?,
        titleID: Int, bodyID: Int, id: Int
    ) {
        postNotification(
            context, intent, titleID,
            LocUtils.getString(context, bodyID), id
        )
    }

    fun postNotification(
        context: Context, intent: Intent?,
        title: String?, body: String?, rowid: Long
    ) {
        val id = sDefaultChannel.idFor(rowid)
        postNotification(context, intent, title, body, id)
    }

    @JvmStatic
    fun postNotification(
        context: Context, intent: Intent?,
        titleId: Int, body: String?, rowid: Long
    ) {
        postNotification(
            context, intent, titleId, body, rowid,
            sDefaultChannel
        )
    }

    @JvmStatic
    fun postNotification(
        context: Context, intent: Intent?,
        titleID: Int, body: String?, id: Int
    ) {
        postNotification(
            context, intent, titleID, body, id,
            sDefaultChannel
        )
    }

    fun postNotification(
        context: Context, intent: Intent?,
        titleID: Int, body: String?, rowid: Long,
        channel: ID
    ) {
        val id = channel.idFor(rowid)
        postNotification(context, intent, titleID, body, id, channel)
    }

    private fun postNotification(
        context: Context, intent: Intent?,
        titleID: Int, body: String?, id: Int,
        channel: ID
    ) {
        val title = LocUtils.getString(context, titleID)
        // Log.d( TAG, "posting with title %s", title );
        postNotification(
            context, intent, title, body, id, channel, false,
            null, 0
        )
    }

    @JvmStatic
    fun postNotification(
        context: Context, intent: Intent?,
        title: String?, body: String?,
        id: Int
    ) {
        postNotification(
            context, intent, title, body, id,
            sDefaultChannel, false, null, 0
        )
    }

    @JvmStatic
    fun postOngoingNotification(
        context: Context, intent: Intent?,
        title: String?, body: String?,
        rowid: Long, channel: ID,
        actionIntent: Intent?,
        actionString: Int
    ) {
        val id = channel.idFor(rowid)
        postNotification(
            context, intent, title, body, id, channel, true,
            actionIntent, actionString
        )
    }

    private fun postNotification(
        context: Context, intent: Intent?,
        title: String?, body: String?,
        id: Int, channel: ID, ongoing: Boolean,
        actionIntent: Intent?, actionString: Int
    ) {
        /* nextRandomInt: per this link
           http://stackoverflow.com/questions/10561419/scheduling-more-than-one-pendingintent-to-same-activity-using-alarmmanager
           one way to avoid getting the same PendingIntent for similar
           Intents is to send a different second param each time,
           though the docs say that param's ignored.
        */
        val pi = if (null == intent
        ) null else getPendingIntent(context, intent)

        val channelID = Channels.getChannelID(context, channel)
        val builder =
            NotificationCompat.Builder(context, channelID)
                .setContentIntent(pi)
                .setSmallIcon(R.drawable.notify)
                .setOngoing(ongoing)
                .setAutoCancel(true)
                .setContentTitle(title)
                .setContentText(body)


        if (null != actionIntent) {
            val actionPI = getPendingIntent(context, actionIntent)
            builder.addAction(
                0, LocUtils.getString(context, actionString),
                actionPI
            )
        }

        val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.notify(id, builder.build())
    }

    private fun getPendingIntent(context: Context, intent: Intent): PendingIntent {
        val pi = PendingIntent
            .getActivity(
                context, nextRandomInt(), intent,
                PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE
            )
        return pi
    }

    @JvmStatic
    fun cancelNotification(
        context: Context, channel: ID,
        rowid: Long
    ) {
        val id = channel.idFor(rowid)
        cancelNotification(context, id)
    }

    fun cancelNotification(context: Context, rowid: Long) {
        cancelNotification(context, sDefaultChannel, rowid)
    }

    fun cancelNotification(context: Context, id: Int) {
        val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.cancel(id)
    }

    fun playNotificationSound(context: Context) {
        if (CommonPrefs.getSoundNotify(context)) {
            val uri = RingtoneManager
                .getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
            val ringtone = RingtoneManager.getRingtone(context, uri)
            ringtone?.play()
        }
    }

    // adapted from
    // http://stackoverflow.com/questions/2174048/how-to-look-up-a-contacts-name-from-their-phone-number-on-android
    @JvmStatic
    fun phoneToContact(
        context: Context, phone: String,
        phoneStandsIn: Boolean
    ): String? {
        // I'm assuming that since context is passed this needn't
        // worry about synchronization -- will always be called from
        // UI thread.
        var name: String? = null
        synchronized(s_phonesHash) {
            if (s_phonesHash.containsKey(phone)) {
                name = s_phonesHash[phone]
            } else if (Perms23.havePermissions(context, Perm.READ_CONTACTS)) {
                try {
                    val contentResolver = context
                        .contentResolver
                    val cursor = contentResolver
                        .query(
                            Uri.withAppendedPath(
                                PhoneLookup.CONTENT_FILTER_URI,
                                Uri.encode(phone)
                            ),
                            arrayOf(PhoneLookup.DISPLAY_NAME),
                            null, null, null
                        )
                    if (cursor!!.moveToNext()) {
                        val indx = cursor.getColumnIndex(PhoneLookup.DISPLAY_NAME)
                        name = cursor.getString(indx)
                    }
                    cursor.close()
                    s_phonesHash.put(phone, name)
                } catch (ex: Exception) {
                    // could just be lack of permsisions
                    name = null
                }
            } else {
                val phones = XWPrefs.getSMSPhones(context)
                val iter = phones.keys()
                while (iter.hasNext()) {
                    val key = iter.next()
                    if (PhoneNumberUtils.compare(key, phone)) {
                        name = phones.optString(key, phone)
                        s_phonesHash[phone] = name
                        break
                    }
                }
            }
        }
        if (null == name && phoneStandsIn) {
            name = phone
        }
        return name
    }

    fun capitalize(str: String?): String? {
        var str = str
        if (null != str && 0 < str.length) {
            str = str.substring(0, 1).uppercase(Locale.getDefault()) + str.substring(1)
        }
        return str
    }

    @JvmStatic
    fun getMD5SumFor(context: Context, dal: DictAndLoc): String? {
        var result: String? = null

        if (DictUtils.DictLoc.BUILT_IN == dal.loc) {
            result = dal.loc.toString()
        } else {
            val file = dal.getPath(context)
            try {
                val `is`: InputStream = FileInputStream(file)
                val md = MessageDigest.getInstance("MD5")
                val buf = ByteArray(1024 * 8)
                while (true) {
                    val nRead = `is`.read(buf)
                    if (0 >= nRead) {
                        break
                    }
                    md.update(buf, 0, nRead)
                }
                result = digestToString(md.digest())
            } catch (ex: Exception) {
                Log.ex(TAG, ex)
            }
        }
        // Log.d( TAG, "getMD5SumFor(%s) => %s", dal.name, result );
        return result
    }

    @JvmStatic
    fun getMD5SumFor(bytes: ByteArray?): String? {
        var result: String? = null
        if (bytes != null) {
            var digest: ByteArray? = null
            try {
                val md = MessageDigest.getInstance("MD5")
                val buf = ByteArray(128)
                var nLeft = bytes.size
                var offset = 0
                while (0 < nLeft) {
                    val len = min(buf.size.toDouble(), nLeft.toDouble()).toInt()
                    System.arraycopy(bytes, offset, buf, 0, len)
                    md.update(buf, 0, len)
                    nLeft -= len
                    offset += len
                }
                digest = md.digest()
            } catch (nsae: NoSuchAlgorithmException) {
                Log.ex(TAG, nsae)
            }
            result = digestToString(digest)
        }
        return result
    }

    fun setChecked(parent: View, id: Int, value: Boolean) {
        val cbx = parent.findViewById<View>(id) as CheckBox
        cbx.isChecked = value
    }

    fun setText(parent: View, id: Int, value: String?) {
        val editText = parent.findViewById<View>(id) as EditText
        editText?.setText(value, TextView.BufferType.EDITABLE)
    }

    fun setInt(parent: View, id: Int, value: Int) {
        val str = value.toString()
        setText(parent, id, str)
    }

    fun setEnabled(view: View, enabled: Boolean) {
        view.isEnabled = enabled
        if (view is ViewGroup) {
            val asGroup = view
            for (ii in 0 until asGroup.childCount) {
                setEnabled(asGroup.getChildAt(ii), enabled)
            }
        }
    }

    fun setEnabled(parent: View, id: Int, enabled: Boolean) {
        val view = parent.findViewById<View>(id)
        setEnabled(view, enabled)
    }

    fun getChecked(dialog: Dialog, id: Int): Boolean {
        val cbx = dialog.findViewById<View>(id) as CheckBox
        return cbx.isChecked
    }

    fun getText(dialog: Dialog, id: Int): String {
        val editText = dialog.findViewById<View>(id) as EditText
        return editText.text.toString()
    }

    fun getInt(dialog: Dialog, id: Int): Int {
        val str = getText(dialog, id)
        return try {
            str.toInt()
        } catch (nfe: NumberFormatException) {
            0
        }
    }

    @JvmStatic
    fun setItemVisible(menu: Menu, id: Int, enabled: Boolean) {
        val item = menu.findItem(id)
        item?.setVisible(enabled)
    }

    fun setItemEnabled(menu: Menu, id: Int, enabled: Boolean) {
        val item = menu.findItem(id)
        item.setEnabled(enabled)
    }

    fun hasSmallScreen(context: Context): Boolean {
        if (null == s_hasSmallScreen) {
            val screenLayout = context.resources.getConfiguration().screenLayout
            val hasSmallScreen = (
                    (screenLayout and Configuration.SCREENLAYOUT_SIZE_MASK)
                            == Configuration.SCREENLAYOUT_SIZE_SMALL)
            s_hasSmallScreen = hasSmallScreen
        }
        return s_hasSmallScreen!!
    }

    fun digestToString(digest: ByteArray?): String? {
        var result: String? = null
        if (null != digest) {
            val hexArray = charArrayOf(
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                'a', 'b', 'c', 'd', 'e', 'f'
            )
            val chars = CharArray(digest.size * 2)
            for (ii in digest.indices) {
                val byt = digest[ii].toInt() and 0xFF
                chars[ii * 2] = hexArray[byt shr 4]
                chars[ii * 2 + 1] = hexArray[byt and 0x0F]
            }
            result = String(chars)
        }
        return result
    }

    @JvmStatic
    fun getCurSeconds(): Long
    {
        // Called from andutils.c in the jni world
        // Note: an int is big enough for *seconds* (not milliseconds) since 1970
        // until 2038
        val millis = System.currentTimeMillis()
        val result = (millis / 1000).toLong()
        return result
    }

    fun makeDictUriFromName(
        context: Context,
        langName: String?, dictName: String?
    ): Uri {
        val isoCode = DictLangCache.getLangIsoCode(context, langName!!)
        return makeDictUriFromCode(context, isoCode, dictName)
    }

    @JvmStatic
    fun makeDictUriFromCode(context: Context, isoCode: ISOCode?, name: String?): Uri {
        val dictUrl = XWPrefs.getDefaultDictURL(context)
        val builder = Uri.parse(dictUrl).buildUpon()
        if (null != isoCode) {
            builder.appendPath(isoCode.toString())
        }
        if (null != name) {
            assertNotNull(isoCode)
            builder.appendPath(DictUtils.addDictExtn(name))
        }
        val result = builder.build()
        return result
    }

    fun getAppVersion(context: Context): Int {
        if (null == s_appVersion) {
            try {
                val version = context.packageManager
                    .getPackageInfo(BuildConfig.APPLICATION_ID, 0)
                    .versionCode
                s_appVersion = version
            } catch (e: Exception) {
                Log.ex(TAG, e)
            }
        }
        return if (null == s_appVersion) 0 else s_appVersion!!
    }

    @JvmStatic
    fun makeInstallIntent(context: Context, file: File?): Intent {
        val uri = FileProvider
            .getUriForFile(
                context!!,
                BuildConfig.APPLICATION_ID + ".provider",
                file!!
            )
        val intent = Intent(Intent.ACTION_VIEW)
        intent.setDataAndType(uri, XWConstants.APK_TYPE)
        intent.addFlags(
            Intent.FLAG_ACTIVITY_NEW_TASK
                    or Intent.FLAG_GRANT_READ_URI_PERMISSION
        )
        return intent
    }

    // Return whether there's an app installed that can install
    @JvmStatic
    fun canInstall(context: Context, path: File?): Boolean {
        var result = false
        val pm = context.packageManager
        val intent = makeInstallIntent(context, path)
        val doers =
            pm.queryIntentActivities(
                intent,
                PackageManager.MATCH_DEFAULT_ONLY
            )
        result = 0 < doers.size
        return result
    }

    @JvmStatic
    fun getContentView(activity: Activity): View {
        return activity.findViewById(android.R.id.content)
    }

    @JvmStatic
    fun isGooglePlayApp(context: Context): Boolean {
        val pm = context.packageManager
        val packageName = BuildConfig.APPLICATION_ID
        val installer = pm.getInstallerPackageName(packageName)
        val result =
            "com.google.android.feedback" == installer || "com.android.vending" == installer
        return result
    }

    @JvmStatic
    fun isOnUIThread(): Boolean
    {
        return Looper.getMainLooper() == Looper.myLooper()
    }

    @JvmStatic
    fun getChildInstanceOf(parent: ViewGroup, clazz: Class<*>): View? {
        var result: View? = null
        var ii = 0
        while (null == result && ii < parent.childCount) {
            val child = parent.getChildAt(ii)
            if (clazz.isInstance(child)) {
                result = child
                break
            } else if (child is ViewGroup) {
                result = getChildInstanceOf(child, clazz)
            }
            ++ii
        }
        return result
    }

    @JvmStatic
    fun enableAlertButton(dlg: AlertDialog, which: Int, enable: Boolean) {
        val button = dlg.getButton(which)
        if (null != button) {
            button.isEnabled = enable
        }
    }

    // But see hexArray above
    private const val HEX_CHARS = "0123456789ABCDEF"
    private val HEX_CHARS_ARRAY = HEX_CHARS.toCharArray()

    fun ba2HexStr(input: ByteArray): String {
        val sb = StringBuffer()

        for (byt in input) {
            sb.append(HEX_CHARS_ARRAY[byt.toInt() shr 4 and 0x0F])
            sb.append(HEX_CHARS_ARRAY[byt.toInt() and 0x0F])
        }

        val result = sb.toString()
        return result
    }

    fun hexStr2ba(data: String): ByteArray {
        var data = data
        data = data.uppercase(Locale.getDefault())
        assertTrue(0 == data.length % 2)
        val result = ByteArray(data.length / 2)

        var ii = 0
        while (ii < data.length) {
            val one = HEX_CHARS.indexOf(data[ii])
            assertTrue(one >= 0)
            val two = HEX_CHARS.indexOf(data[ii + 1])
            assertTrue(two >= 0)
            result[ii / 2] = ((one shl 4) or two).toByte()
            ii += 2
        }

        return result
    }

    @JvmStatic
    fun base64Encode(`in`: ByteArray?): String {
        return Base64.encodeToString(`in`, Base64.NO_WRAP)
    }

    @JvmStatic
    fun base64Decode(`in`: String?): ByteArray {
        return Base64.decode(`in`, Base64.NO_WRAP)
    }

    @JvmStatic
    fun bytesToSerializable(bytes: ByteArray?): Serializable? {
        var result: Serializable? = null
        try {
            val ois =
                ObjectInputStream(ByteArrayInputStream(bytes))
            result = ois.readObject() as Serializable
        } catch (ex: Exception) {
            Log.d(TAG, "%s", ex.message)
        }
        return result
    }

    fun string64ToSerializable(str64: String?): Any? {
        val bytes = base64Decode(str64)
        val result = bytesToSerializable(bytes)
        return result
    }

    @JvmStatic
    fun serializableToBytes(obj: Serializable?): ByteArray? {
        var result: ByteArray? = null
        val bas = ByteArrayOutputStream()
        try {
            val out = ObjectOutputStream(bas)
            out.writeObject(obj)
            out.flush()
            result = bas.toByteArray()
        } catch (ex: Exception) {
            Log.ex(TAG, ex)
            failDbg()
        }
        return result
    }

    fun serializableToString64(obj: Serializable?): String {
        val asBytes = serializableToBytes(obj)
        val result = base64Encode(asBytes)
        return result
    }

    @JvmStatic
    fun testSerialization(obj: Serializable) {
        if (false && BuildConfig.DEBUG) {
            val as64 = serializableToString64(obj)
            val other = string64ToSerializable(as64)
            assertTrue(other == obj)
            Log.d(TAG, "testSerialization(%s) worked!!!", obj)
        }
    }

    fun getFirstVersion(context: Context): Int {
        val prefs =
            context.getSharedPreferences(
                HIDDEN_PREFS,
                Context.MODE_PRIVATE
            )
        val firstVersion = prefs.getInt(FIRST_VERSION_KEY, Int.MAX_VALUE)
        assertTrueNR(firstVersion < Int.MAX_VALUE)
        return firstVersion
    }

    private fun setFirstBootStatics(context: Context) {
        if (null == s_isFirstBootThisVersion) {
            val thisVersion = getAppVersion(context)
            var prevVersion = 0
            val prefs =
                context.getSharedPreferences(
                    HIDDEN_PREFS,
                    Context.MODE_PRIVATE
                )


            if (0 < thisVersion) {
                prevVersion = prefs.getInt(SHOWN_VERSION_KEY, -1)
            }
            val newVersion = prevVersion != thisVersion

            s_isFirstBootThisVersion = newVersion
            s_isFirstBootEver = -1 == prevVersion

            val firstVersion = prefs.getInt(FIRST_VERSION_KEY, Int.MAX_VALUE)
            s_firstVersion = firstVersion >= thisVersion
            if (newVersion || Int.MAX_VALUE == firstVersion) {
                val editor = prefs.edit()
                if (newVersion) {
                    editor.putInt(SHOWN_VERSION_KEY, thisVersion)
                }
                if (Int.MAX_VALUE == firstVersion) {
                    editor.putInt(FIRST_VERSION_KEY, thisVersion)
                }
                editor.commit()
            }
        }
    }

    @JvmField
    val ISO_EN: ISOCode = ISOCode.newIf("en")!!

    internal abstract class OnNothingSelDoesNothing

        : OnItemSelectedListener {
        override fun onNothingSelected(parentView: AdapterView<*>?) {}
    }

    // Let's get some type safety between language name and iso code.
    class ISOCode(code: String) : Serializable {
        val mISOCode: String

        init {
            // Log.d( TAG, "ISOCode(%s)", code );
            assertTrueNR(8 > code.length)
            mISOCode = code
        }

        override fun toString(): String {
            return mISOCode
        }

        override fun equals(other: Any?): Boolean {
            return (null != other && other is ISOCode) && other.mISOCode == mISOCode
        }

        override fun hashCode(): Int {
            return mISOCode.hashCode()
        }

        companion object {
            private val sMap: MutableMap<String, ISOCode> = HashMap()
            @JvmStatic
            fun newIf(code: String?): ISOCode? {
                var result: ISOCode? = null
                if (!TextUtils.isEmpty(code)) {
                    synchronized(sMap) {
                        result = sMap[code]
                        if (null == result) {
                            result = ISOCode(code!!)
                            sMap[code] = result!!
                        }
                    }
                }
                return result
            }

            @JvmStatic
            fun safeEquals(ic1: ISOCode?, ic2: ISOCode?): Boolean {
                val result = if (ic1 === ic2) {
                    true
                } else if (null == ic1 || null == ic2) {
                    false
                } else {
                    TextUtils.equals(ic1.mISOCode, ic2.mISOCode)
                }
                return result
            }
        }
    }                           // class ISOCode
}

/* -*- compile-command: "find-and-ant.sh debug install"; -*- */
/*
 * Copyright 2009-2012 by Eric House (xwords@eehouse.org).  All rights
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

package org.eehouse.android.xw4;

import android.app.Activity;
// import android.content.Context;
// import android.content.Intent;
// import android.net.Uri;
// import android.os.AsyncTask;
import android.os.Bundle;
// import android.os.Handler;
// import android.view.Window;
// import android.widget.ProgressBar;
// import android.widget.TextView;

// import java.io.File;
// import java.io.FileOutputStream;
// import java.io.InputStream;

// import java.net.URI;
// import java.net.URLConnection;
// import java.security.MessageDigest;
// import java.util.HashMap;

// import org.eehouse.android.xw4.loc.LocUtils;

// import junit.framework.Assert;

public class DictImportActivity extends Activity {

    private DictImportDelegate m_dlgt;

    @Override
    protected void onCreate( Bundle savedInstanceState ) 
    {
        super.onCreate( savedInstanceState );
        m_dlgt = new DictImportDelegate( this, savedInstanceState );
    }
}

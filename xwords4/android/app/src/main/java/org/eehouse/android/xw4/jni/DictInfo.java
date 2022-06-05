/* -*- compile-command: "find-and-gradle.sh inXw4dDeb"; -*- */
/*
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

package org.eehouse.android.xw4.jni;

import org.eehouse.android.xw4.BuildConfig;

public class DictInfo {
    // set in java code
    public String name;
    public String fullSum;      // md5sum of the whole file

    // set in jni code
    public int wordCount;
    public String isoCode;
    public String langName;
    public String md5Sum;       // internal (skipping header?)

    @Override
    public String toString()
    {
        if ( BuildConfig.NON_RELEASE ) {
            return new StringBuilder("{")
                .append("name: ").append(name)
                .append(", isoCode: ").append(isoCode)
                .append(", langName: ").append(langName)
                .append(", md5Sum: ").append(md5Sum)
                .append(", fullSum: ").append(fullSum)
                .append("}").toString();
        } else {
            return super.toString();
        }
    }
};

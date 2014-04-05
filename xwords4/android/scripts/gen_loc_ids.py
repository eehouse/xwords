#!/usr/bin/python

# Generate a java file with a string->ID mapping for every R.string.id
# in R.java where the string is used in a menu in the form This allows
# "loc:<string>"runtime lookup based on the string, which is the only
# thing it's possible to store in the menu's title (since there's no
# AttributeSet provided with menu inflation.

import glob, sys, re, os
from lxml import etree

pairs = {}

# Enforce some conventions: No %d/%s in strings, and anything that
# does have formatting has a name ending in _fmt.
HAS_FMT = re.compile('.*%\d\$[ds].*', re.DOTALL)
OLD_PCT = re.compile('.*%[ds].*', re.DOTALL)
ENDS_WITH_FMT = re.compile('.*_fmt$')
for path in glob.iglob( "res/values*/strings.xml" ):
    for action, elem in etree.iterparse(path):
        if "end" == action and elem.text:
            if OLD_PCT.match( elem.text ):
                print "%d and %s no longer allowed: in", path, "text:", elem.text
                sys.exit(1)

# Get all string IDs that are used in menus -- the ones we care about
TITLE = re.compile('.*android:title="loc:(.*)".*')
for path in glob.iglob( "res/menu/*.xml" ):
    for line in open( path, "r" ):
        line.strip()
        mtch = TITLE.match(line)
        if mtch:
            pairs[mtch.group(1)] = True

LOC_START = re.compile('loc:(.*)')
for path in glob.iglob( "res/values/common_rsrc.xml" ):
    for action, elem in etree.iterparse(path):
        if "end" == action and elem.text:
            mtch = LOC_START.match(elem.text)
            if mtch: 
                pairs[mtch.group(1)] = True


# Get all string IDs, but only keep those we've seen in menus
# LINE = re.compile('.*public static final int (.*)=(0x.*);.*')
# for line in open("gen/org/eehouse/android/xw4/R.java", "r"):
#     line.strip()
#     mtch = LINE.match(line)
#     if mtch:
#         key = mtch.group(1)
#         if key in pairs:
#             pairs[key] = mtch.group(2)


# beginning of the class file
print """
/***********************************************************************
* Generated file; do not edit!!! 
***********************************************************************/

package org.eehouse.android.xw4.loc;

import java.util.HashMap;

import org.eehouse.android.xw4.R;

public class LocIDs {
    public static final int NOT_FOUND = -1;
    protected static HashMap<String,Integer> s_map;
    static {
        s_map = new HashMap<String,Integer>();
"""

for key in pairs.keys():
    print "        s_map.put(\"%s\", R.string.%s);" % (key, key)

# Now the end of the class
print """
    }

    protected static int getID( String key )
    {
        int result = NOT_FOUND;
        if ( s_map.containsKey( key ) ) {
            result = s_map.get( key );
        }
        return result;
    }
}
/* end generated file */
"""

#!/usr/bin/python

import glob, sys, re, os, getopt

from lxml import etree

# sys.exit(0)

g_xmlTypes = [
    { 'elemName': 'item',
      'attrType' : '{http://schemas.android.com/apk/res/android}title'
  },
    { 'elemName': 'Button',
      'attrType' : '{http://schemas.android.com/apk/res/android}text'
  },
    { 'elemName': 'TextView',
      'attrType' : '{http://schemas.android.com/apk/res/android}text'
  },
    { 'elemName': 'CheckBox',
      'attrType' : '{http://schemas.android.com/apk/res/android}text'
  },
    { 'elemName': 'EditText',
      'attrType' : '{http://schemas.android.com/apk/res/android}hint'
  },
      
]

g_pairs = {}

STR_REF = re.compile('@string/(.*)$')

def xform(src, dest):
    doc = etree.parse(src)
    root = doc.getroot();
    for child in root.iter():
        for elem in g_xmlTypes:
            if child.tag == elem['elemName']:
                value = child.get(elem['attrType'])
                match = value and STR_REF.match(value)
                if match: 
                    key = match.group(1)
                    if key in g_pairs:
                        child.set(elem['attrType'], "loc:" + key)

    # create directory if needed, then write file
    dir = os.path.dirname( dest )
    if not os.path.exists(dir): os.makedirs(dir)
    doc.write( dest, pretty_print=True )

def printStrings( pairs, outfile ):
    fil = open( outfile, "w" )

    # beginning of the class file
    lines = """
/***********************************************************************
* Generated file; do not edit!!! 
***********************************************************************/

package org.eehouse.android.xw4.loc;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.eehouse.android.xw4.R;

public class LocIDsData {
    public static final int NOT_FOUND = -1;

    protected static final Map<String, Integer> S_MAP = 
        Collections.unmodifiableMap(new HashMap<String, Integer>() {{ 
"""
    fil.write( lines )

    for key in pairs.keys():
        fil.write( "        put(\"%s\", R.string.%s);\n" % (key, key) )

    # Now the end of the class
    lines = """
    }});
}
/* end generated file */
"""
    fil.write( lines )

def main():
    outfile = ''
    pairs, rest = getopt.getopt(sys.argv[1:], "o:")
    for option, value in pairs:
        if option == '-o': outfile = value

    # Gather all localizable strings
    for path in glob.iglob( "res/values/strings.xml" ):
        for action, elem in etree.iterparse(path):
            if "end" == action and 'string' == elem.tag:
                g_pairs[elem.get('name')] = True

    for subdir, dirs, files in os.walk('res_src'):
        for file in files:
            src = subdir + '/' + file
            dest = src.replace( 'res_src', 'res', 1 )
            xform( src, dest )

    if outfile: printStrings( g_pairs, outfile )


main()

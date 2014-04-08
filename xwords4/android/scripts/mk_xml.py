#!/usr/bin/python

import glob, sys, re, os
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
      
]

g_pairs = {}

STR_REF = re.compile('@string/(.*)$')

def xform(src, dest):
    print "looking at file", src
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
                        print child.tag, key, "loc:" + key
                        child.set(elem['attrType'], "loc:" + key)

    # create directory if needed, then write file
    dir = os.path.dirname( dest )
    if not os.path.exists(dir): os.makedirs(dir)
    doc.write( dest, pretty_print=True )

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

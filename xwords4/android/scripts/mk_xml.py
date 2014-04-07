#!/usr/bin/python

import glob, sys, re, os
from lxml import etree

pairs = {}

STR_REF = re.compile('@string/(.*)$')

def xform(src, dest):
    doc = etree.parse(src)
    for item in doc.findall('item'):
        title = item.get('{http://schemas.android.com/apk/res/android}title')
        if title:
            match = STR_REF.match(title)
            if match: 
                key = match.group(1)
                if key in pairs:
                    print key, "loc:" + key
                    item.set('{http://schemas.android.com/apk/res/android}title', "loc:" + key)
    doc.write( dest, pretty_print=True )

# Gather all localizable strings
for path in glob.iglob( "res/values/strings.xml" ):
    for action, elem in etree.iterparse(path):
        if "end" == action and 'string' == elem.tag:
            pairs[elem.get('name')] = True

for subdir, dirs, files in os.walk('res_src'):
    for file in files:
        src = subdir + '/' + file
        dest = src.replace( 'res_src', 'res', 1 )
        xform( src, dest )

#!/usr/bin/python

import re, sys
from lxml import etree


# Take an English strings.xml file and another, "join" them on the
# name of each string, and then produce an array that's a mapping of
# English to the other. Get ride of extra whitespace etc in the
# English strings so they're identical to how an Android app displays
# them.

english = 'res/values/strings.xml'
other_f = 'res_src/values-%s/strings.xml'

def readIDs(base):
    ids = {}
    start = re.compile('\s*public static final class string {\s*')
    end = re.compile('\s*}\s*')
    entry = re.compile('\s*public static final int (\S+)=(0x.*);\s*')
    inLine = False
    path = base + '/archive/R.java'
    for line in open(path, 'r'):
        line = line.strip()
        # print line
        if inLine:
            if end.match(line):
                break
            else:
                match = entry.match(line)
                if match:
                    name = match.group(1)
                    value = int(match.group(2), 16)
                    ids[name] = value
        elif start.match(line):
            inLine = True
    return ids

def asMap( path, ids ):
    map = {}
    parser = etree.XMLParser(remove_blank_text=True)
    doc = etree.parse( path, parser )
    for elem in doc.getroot().iter():
        if 'string' == elem.tag:
            text = elem.text
            if text:
                # print 'text before:', text
                text = " ".join(re.split('\s+', text)) \
                          .replace("\\'", "'") \
                          .replace( '\\"', '"' )
                # print 'text after:', text
                name = elem.get('name')
                id = ids[name]
                map[id] = text
    return map

def getXlationFor( base, loc ):
    ids = readIDs(base)
    eng = asMap( base + '/' + english, ids )
    other = asMap( base + '/' + other_f % (loc), ids )
    result = []
    for key in eng.keys():
        if key in other:
            result.append( { 'id' : key, 'loc' : other[key] } )
    return result

def main():
    data = getXlationFor( '.', 'ba_CK' )
    print data
    data = getXlationFor( '.', 'ca_PS' )
    print data

##############################################################################
if __name__ == '__main__':
    main()

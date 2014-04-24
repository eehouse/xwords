#!/usr/bin/python

import re
from lxml import etree


# Take an English strings.xml file and another, "join" them on the
# name of each string, and then produce an array that's a mapping of
# English to the other. Get ride of extra whitespace etc in the
# English strings so they're identical to how an Android app displays
# them.

english = 'res/values/strings.xml'
other_f = 'res_src/values-%s/strings.xml'

def asMap( path ):
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
                map[elem.get('name')] = text
    return map

def getXlationFor( base, loc ):
    eng = asMap( base + '/' + english )
    other = asMap( base + '/' + other_f % (loc) )
    result = []
    for key in eng.keys():
        if key in other:
            result.append( { 'en' : eng[key], 'loc' : other[key] } )
    return result

def main():
    data = getXlationFor( 'ba_CK' )
    data = getXlationFor( 'ca_PS' )
    print data

##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/python

import mk_xml, os, sys, codecs

from lxml import etree

def longestCommon( name, pairs ):
    match = None
    for ii in range(1, len(name)):
        str = name[:ii]
        for key in pairs.keys():
            if str == key[:ii]:
                print str, "matches", key, "so far"
                match = key
                break
    return match

def checkAgainst( path, pairs ):
    print "looking at", path
    doc = etree.parse( path )
    root = doc.getroot();
    for child in root.iter():
        if child.tag == "string":
            name = child.get("name")
            if not name in pairs:
                candidate = longestCommon( name, pairs )
                print name, "not found in the English strings"
                print "closest I can find is", candidate
                print "here are the two strings, English then the other"
                print pairs[candidate]
                print child.text
                response = raw_input( "replace %s with %s? (y, n, s or q)" % (name, candidate) )
                if response == 'y':
                    child.set('name', candidate)
                elif response == 's':
                    break
                elif response == 'q':
                    sys.exit(0)
                # try = tryNames( name, pairs )
                # response = raw_input( "unknown name: %s; respond:" % (name) )
                # print "you wrote:", response

    # Now walk the doc, comparing names with the set in pairs and
    # enforcing rules about names, offering to change whereever
    # possible
    out = open( path, "w" )
    out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8", xml_declaration=True ) )


def main():
    pairs = mk_xml.getStrings()

    for subdir, dirs, files in os.walk('res_src'):
        for file in [file for file in files if file == "strings.xml"]:
            path = "%s/%s" % (subdir, file)
            checkAgainst( path, pairs )
            sys.exit(0)


##############################################################################
if __name__ == '__main__':
    main()

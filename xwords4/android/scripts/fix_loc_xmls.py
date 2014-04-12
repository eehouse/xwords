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
                match = str
                break
                
    sys.exit(0)

def checkAgainst( path, pairs ):
    print "looking at", path
    doc = etree.parse( path )
    root = doc.getroot();
    # for child in root.iter():
    #     if child.tag == "string":
    #         name = child.get("name")
    #         if not name in pairs:
    #             longestCommon( name, pairs )
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


##############################################################################
if __name__ == '__main__':
    main()

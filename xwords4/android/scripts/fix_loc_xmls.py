#!/usr/bin/python

import mk_xml, os, sys, getopt

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
    done = False
    for child in root.iter():
        if done: break
        if child.tag == "string":
            name = child.get("name")
            if not name in pairs:
                candidate = longestCommon( name, pairs )
                if not candidate: continue
                print name, "not found in the English strings"
                print "closest I can find is", candidate
                print "here are the two strings, English then the other"
                print 'English:', pairs[candidate]
                print 'Other:  ', child.text
                print 'Replace %s with %s?'  % (name, candidate)
                while True:
                    response = raw_input( "Yes, No, Remove, Save or Quit?" ).lower()
                    if response == 'n': 
                        pass
                    elif response == 'y': 
                        child.set( 'name', candidate )
                    elif response == 'r':
                        root.remove( child )
                    elif response == 's':
                        done = True
                    elif response == 'q':
                        sys.exit(0)
                    else:
                        continue
                    break
                # try = tryNames( name, pairs )
                # response = raw_input( "unknown name: %s; respond:" % (name) )
                # print "you wrote:", response

    # Now walk the doc, comparing names with the set in pairs and
    # enforcing rules about names, offering to change whereever
    # possible
    out = open( path, "w" )
    out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8", xml_declaration=True ) )


def main():
    stringsFiles = []
    pairs, rest = getopt.getopt(sys.argv[1:], "f:")
    for option, value in pairs:
        if option == '-f': stringsFiles.append(value)

    pairs = mk_xml.getStrings()

    if 0 == len(stringsFiles):
        for subdir, dirs, files in os.walk('res_src'):
            for file in [file for file in files if file == "strings.xml"]:
                stringsFiles.append( "%s/%s" % (subdir, file) )
    for path in stringsFiles:
        checkAgainst( path, pairs )


##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/python

import getopt, sys
from lxml import etree

def usage(msg):
    if msg: print "ERROR: " + msg
    print "usage:", sys.argv[0]
    print "  -i                        # modify files in place"
    print "  -s <string_name>          # name to convert; (can be repeated)"
    print "  -p <path/to/strings.xml>  # file to search in (can be repeated)"

def modFile( path, stringNames, inPlace ):
    doc = etree.parse(path)
    root = doc.getroot();
    for child in root.iter():
        if child.tag == 'string':
            name = child.get('name')
            if name in stringNames:
                child.tag = 'plurals'
                item = etree.Element("item")
                item.text = child.text
                item.set('quantity', 'other')
                child.append( item )
                child.text = None
    out = sys.stdout
    if inPlace: out = open(path, 'w')
    doc.write( out, pretty_print=True )

def main():
    stringFiles = []
    stringNames = []
    inPlace = False

    pairs, rest = getopt.getopt(sys.argv[1:], "ip:s:")
    for option, value in pairs:
        if option == '-i': inPlace = True
        elif option == '-p': stringFiles.append(value)
        elif option == '-s': stringNames.append(value)
        else: usage('unknown option: ' + option)

    for path in stringFiles: modFile( path, stringNames, inPlace )

##############################################################################
if __name__ == '__main__':
    main()

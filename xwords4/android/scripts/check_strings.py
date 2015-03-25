#!/usr/bin/python

import re, sys, os, getopt
from lxml import etree

g_formats = {}

def usage(msg=''):
    print
    if not '' == msg: print 'Error:', msg
    print "usage:", sys.argv[0]
    print "   Compares all files res/values-??/strings.xml with res/values/strings.xml"
    print "   and makes sure they're legal: same format strings, etc."
    sys.exit(1)


def associate( formats, name, fmt ):
    if name in formats:
        forName = formats[name]
    else:
        forName = set()
        formats[name] = forName
    if fmt in forName: 
        print "Warning: %s duplicated in %s" % (fmt, name)
    forName.add(fmt)
    # print 'added', fmt, 'to', name

def checkFormats( formats ):
    for name in formats:
        curSet = formats[name]
        testSet = set()
        for digit in range(1, 9):
            foundDigit = False
            for spec in ['s', 'd']:
                fmt = '%' + str(digit) + '$' + spec
                if fmt in curSet: 
                    foundDigit = True
                    testSet.add(fmt)
                    break
            if not foundDigit:
                break
        if curSet == testSet:
            print name, "is ok"
        else:
            print 'ERROR: sets different for', name, curSet, testSet
            sys.exit(1)

def main():
    if 1 < len(sys.argv): usage()

    wd = os.path.dirname(sys.argv[0])
    path = wd + '/../XWords4/res/values/strings.xml'

    # Load English
    engFormats = {}
    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")
    doc = etree.parse(path, parser)
    pat = re.compile( '(%\d\$[sd])', re.DOTALL | re.MULTILINE )
    for typ in ['string', 'item']:
        for elem in doc.findall(typ):
            splits = re.split( pat, elem.text )
            nParts = len(splits)
            if 1 < nParts:
                for ii in range(nParts):
                    part = splits[ii]
                    if re.match( pat, part ):
                        associate( engFormats, elem.get('name'), part )
    checkFormats( engFormats )

    for subdir, dirs, files in os.walk(path):
        for file in [file for file in files if file == "strings.xml" \
                     and not subdir.endswith('/values')]:
            print file, subdir



##############################################################################
if __name__ == '__main__':
    main()

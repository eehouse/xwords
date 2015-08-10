#!/usr/bin/python

import re, sys, os, getopt
from lxml import etree

g_formats = {}
g_verbose = 0

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
            if 0 < g_verbose: print name, "is ok"
        else:
            print 'WARNING: sets different for', name, curSet, testSet

def checkLangFormats( engData, langData, lang ):
    for key in langData:
        if not key in engData:
            print 'WARNING: key', key, 'in', lang, 'but not in English'
        elif not engData[key] == langData[key]:
            print 'ERROR: set mismatch', key,  'from', lang, engData[key], 'vs', langData[key]
            sys.exit(1)

def getForElem( data, pat, elem, name ):
    splits = re.split( pat, elem.text )
    nParts = len(splits)
    if 1 < nParts:
        for ii in range(nParts):
            part = splits[ii]
            if re.match( pat, part ):
                associate( data, name, part )

def getFormats( doc, pat, lang ):
    result = {}
    for elem in doc.findall('string'):
        getForElem( result, pat, elem, elem.get('name') )
    for elem in doc.findall('plurals'):
        name = elem.get('name')
        for elem in elem.findall('item'):
            quantity = elem.get('quantity')
            if not elem.text or 0 == len(elem.text):
                print 'plurals', name, 'has empty quantity', quantity, \
                    'in file', lang
                sys.exit(1)
            else:
                getForElem( result, pat, elem, name + '/' + quantity )
    return result

def main():
    if 1 < len(sys.argv): usage()
    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")

    wd = os.path.dirname(sys.argv[0])

    # Load English
    path = wd + '/../XWords4/res/values/strings.xml'
    doc = etree.parse(path, parser)
    pat = re.compile( '(%\d\$[sd])', re.DOTALL | re.MULTILINE )
    engFormats = getFormats( doc, pat, 'en' )
    checkFormats( engFormats )

    path = wd + '/../XWords4/res_src'
    for subdir, dirs, files in os.walk(path):
        for file in [file for file in files if file == "strings.xml" \
                     and not subdir.endswith('/values')]:
            doc = etree.parse( subdir + '/' + file, parser )
            forLang = getFormats( doc, pat, subdir )
            checkLangFormats( engFormats, forLang, subdir )
            sys.exit(0)

##############################################################################
if __name__ == '__main__':
    main()

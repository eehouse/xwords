#!/usr/bin/python3

import re, sys, os, getopt
from lxml import etree

g_formats = {}
g_verbose = 0

def usage(msg=''):
    print()
    if msg: print( 'Error:', msg)
    print( "usage:", sys.argv[0], '[-c <??>]* [-l] [-h]' )
    print( "   Compares all files res/values-??/strings.xml with res/values/strings.xml" )
    print( "   and makes sure they're legal: same format strings, etc." )
    print( "   -c options, if present, limit the check to what's specified" )
    print( "   -l option lists available codes and exits" )
    print( "   -h option prints this message" )
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
            if 0 < g_verbose: print( name, "is ok")
        else:
            print( 'WARNING: sets different for', name, curSet, testSet)

# Make sure that if there's a positional param of one type (%s or %d,
# typically) in one set that the same positional in the other is of
# the same type.
g_specPat = re.compile('%(\d)\$[ds]')
def asDict(set):
    result = {}
    for elem in set:
        match = re.match( g_specPat, elem )
        if match:
            index = int(match.group(1))
            result[index] = elem
    return result

def setIndicesAgree(set1, set2):
    result = True
    dict1 = asDict(set1)
    dict2 = asDict(set2)
    for index in dict1:
        if index in dict2 and not dict1[index] == dict2[index]:
            result = False
            break
    return result

def checkLangFormats( engData, langData, lang ):
    for key in langData:
        if not key in engData:
            print( 'WARNING: key', key, 'in', lang, 'but not in English')
        elif not setIndicesAgree(engData[key], langData[key] ):
            print( 'ERROR: illegal set mismatch', key,  'from', lang, engData[key], 'vs', langData[key])
            sys.exit(1)
        elif not engData[key] == langData[key]:
            print( 'WARNING: set mismatch', key,  'from', lang, engData[key], 'vs', langData[key])

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
                print( 'ERROR: plurals', name, 'has empty quantity', quantity, \
                    'in file', lang)
                sys.exit(1)
            else:
                add = name + '/' + quantity
                getForElem( result, pat, elem, add )
    return result

g_dirpat = re.compile( '.*values-(..)$' )
def getCodes(wd):
    result = []
    path = wd + '/../XWords4/res_src'
    for subdir, dirs, files in os.walk(path):
        for file in [file for file in files if file == "strings.xml"]:
            match = re.match(g_dirpat, subdir)
            if match:
                result.append( match.group(1) )
    return result

def main():
    wd = os.path.dirname(sys.argv[0])
    langCodes = []
    allCodes = getCodes(wd)

    pairs, rest = getopt.getopt(sys.argv[1:], "c:hl")
    for option, value in pairs:
        if option == '-c':
            if value in allCodes:
                langCodes.append(value)
            else:
                usage( "unexpected code: " + value + " not one of "
                       + ', '.join(allCodes) )
        elif option == '-h': usage()
        elif option == '-l':
            print( 'Available codes:', ', '.join(allCodes))
            sys.exit(0)
        else:
            usage()

    # use the entire set if not specified
    if not langCodes: langCodes = allCodes

    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")

    # Load English
    path = wd + '/../app/src/main/res/values/strings.xml'
    doc = etree.parse(path, parser)
    pat = re.compile( '(%\d\$[sd])', re.DOTALL | re.MULTILINE )
    engFormats = getFormats( doc, pat, 'en' )
    checkFormats( engFormats )

    for code in langCodes:
        file = wd + '/../XWords4/res_src/values-%s/strings.xml' % code
        doc = etree.parse( file, parser )
        forLang = getFormats( doc, pat, code )
        checkLangFormats( engFormats, forLang, code )

##############################################################################
if __name__ == '__main__':
    main()

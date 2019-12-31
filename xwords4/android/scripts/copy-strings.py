#!/usr/bin/python

# Go through all the res_src strings.xml files, and copy them over
# into the world where they'll get used in a build. This is meant to
# allow them to be built-in as an alternative to the
# locutils/downloadable system.

import re, sys, os, getopt
from lxml import etree

s_prefix = 'XLATE ME: '
# languages in which it's ok to make a standalone quantity="one" into
# quantity="other"
g_oneToOthers = ['values-ja']
g_formatsPat = re.compile( '(%\d\$[sd])', re.DOTALL | re.MULTILINE )

sComment = """
    DO NOT EDIT THIS FILE!!!!
    It was generated (from %s, by %s).
    Any changes you make to it will be lost.
"""

def exitWithError(msg):
    print 'ERROR:', msg
    sys.exit(1)

def usage():
    print "usage:", sys.argv[0], '[-k <list-o-dirs>]'
    sys.exit(1)

def sameOrSameWithPrefix( str1, str2 ):
    result = str1 == str2
    if not result:
        if str1.startswith(s_prefix):
            result = str1[len(s_prefix):] == str2
    return result

def sameAsEnglishPlural(engNames, strElem):
    strs = engNames[strElem.get('name')]['strings']
    str = strElem.text
    result = 1 == len(strs) and 'other' in strs \
             and sameOrSameWithPrefix( str, strs['other'] )
    return result

# If has a one and no (or empty) other, convert the one to other
def tryConvertOne( plurals ):
    quantities = {}
    for item in plurals.getchildren():
        quantities[item.get("quantity")] = item

    use = False
    if "one" in quantities:
        if "other" in quantities:
            text = quantities['other'].text
            if not text or 0 == len(text):
                use = True
        else:
            use = True

    if use:
        print "converting", plurals.get('name')
        plurals.remove(quantities['other'])
        quantities['one'].set('quantity', 'other')

def pluralsIsBogus(engNames, plurals, verbose):
    haveOther = False           # will crash without one
    bogus = False
    for item in plurals.getchildren():
        text = item.text
        if not text or 0 == len(text):
            bogus = True
            if verbose:
                quantity = item.get("quantity")
                print 'dropping plurals {name} because of empty/missing \"{quantity}\"' \
                    .format(name=plurals.get("name"), quantity=quantity )
            break
        if item.get("quantity") == "other":
            haveOther = True

    if verbose and not bogus and not haveOther:
        print "dropping plurals {name} because no \"other\" quantity" \
            .format(name=plurals.get("name"))
            
    return bogus or not haveOther

def pluralsIsSame(engNames, plurals):
    different = False           # all children duplicates of English
    engItem = engNames[plurals.get('name')]
    strings = engItem['strings']
    for item in plurals.getchildren():
        text = item.text
        if not text or 0 == len(text):
            exitWithError( "bogus empty plurals item in " + plurals.get('name'))
            engItem = engItem

        quantity = item.get('quantity')
        if quantity in strings:
            if sameOrSameWithPrefix( strings[quantity], text ):
                different = True
    return different

# path will be something like res_src/values-pt/strings.xml. We want
# the next-to-last entry.
def valuesDir( path ):
    splits = path.split('/')
    return splits[-2]

def checkPlurals( engNames, elem, src, verbose ):
    name = elem.get('name')
    ok = True
    if not name in engNames or not 'plurals' == engNames[name]['type']:
        print 'plurals', name, 'not in engNames or not a plurals there'
        ok = False

    if ok and valuesDir(src) in g_oneToOthers:
        tryConvertOne( elem )

    if ok and pluralsIsBogus(engNames, elem, verbose):
        ok = False
    if ok and pluralsIsSame(engNames, elem):
        ok = False
    if ok:
        for item in elem.getchildren():
            if 0 == len(item.text):
                ok = False
                exitWithError( 'bad empty item ' + name )
    return ok

def loadPlural(plural):
    items = {}
    for child in plural.getchildren():
        items[child.get('quantity')] = child.text
    return items

def writeDoc(doc, src, dest):
    comment = etree.Comment(sComment % (src, os.path.basename(sys.argv[0])))
    doc.getroot().insert( 0, comment )
    dir = os.path.dirname( dest )
    try: os.makedirs( dir )
    except: pass
    out = open( dest, "w" )
    out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8", xml_declaration=True ) )

def exitWithFormatError(engSet, otherSet, name, path):
    exitWithError( 'formats set mismatch: ' + str(engSet) \
                   + ' vs ' + str(otherSet) + '; ' + name \
                   + ' in file ' + path )

def checkOrConvertString(engNames, elem, verbose):
    name = elem.get('name')
    if not elem.text:
        exitWithError( 'elem' + name + " is empty" )
    elif not name in engNames or elem.text.startswith(s_prefix):
        ok = False
    elif not 'string' == engNames[name]['type']:
        if 'plurals' == engNames[name]['type']:
            if sameAsEnglishPlural( engNames, elem ):
                ok = False
            else:
                elem.tag = 'plurals'
                item = etree.Element("item")
                item.text = elem.text
                elem.text = None
                item.set('quantity', 'other')
                elem.append( item )
                if verbose: print 'translated string', name, 'to plural'
                ok = True
        else:
            ok = False
    elif sameOrSameWithPrefix(engNames[name]['string'], elem.text ):
        if verbose: print "Same as english: name: %s; text: %s" % (name, elem.text)
        ok = False
    else:
        ok = True
    return ok

def checkAndCopy( parser, engNames, engFormats, src, dest, verbose ):
    doc = etree.parse(src, parser)

    # strings
    for elem in doc.findall('string'):
        if not checkOrConvertString(engNames, elem, verbose):
            elem.getparent().remove(elem)

    for elem in doc.findall('plurals'):
        if not checkPlurals(engNames, elem, src, verbose):
            elem.getparent().remove(elem)

    formats = getFormats( doc, src )
    for name in formats:
        if name in formats and not engFormats[name] == formats[name]:
            exitWithFormatError( engFormats[name], formats[name], name, dest )

    writeDoc(doc, src, dest)

def setForElem( elem, name ):
    result = set()
    splits = re.split( g_formatsPat, elem.text )
    nParts = len(splits)
    if 1 < nParts:
        for ii in range(nParts):
            part = splits[ii]
            if re.match( g_formatsPat, part ):
                result.add( part )
    # print 'setForElem(', name, ') =>', result
    return result

def getFormats( doc, path ):
    result = {}
    for elem in doc.findall('string'):
        name = elem.get('name')
        result[name] = setForElem( elem, name )
    for elem in doc.findall('plurals'):
        name = elem.get('name')
        for item in elem.findall('item'):
            quantity = item.get('quantity')
            if not item.text or 0 == len(item.text):
                exitWithError( 'plurals ' + name + ' has empty quantity ' + quantity \
                               + ' in file ' + lang )
            else:
                add = name + '/' + quantity
                result[add] = setForElem( item, add )
    # print 'getFormats(', path, ') => ', result
    return result

def main():
    # add these via params later
    excepts = ['values-ca_PS', 'values-ba_CK']
    verboses = ['values-ja']

    try:
        pairs, rest = getopt.getopt(sys.argv[1:], "k:")
        for option, value in pairs:
            if option == '-k': excepts += value.split(' ')
            else: usage()
    except:
        print "Unexpected error:", sys.exc_info()[0]
        usage()

    # summarize the english file
    wd = os.path.dirname(sys.argv[0])
    path = wd + '/../app/src/main/res/values/strings.xml'

    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")
    engDoc = etree.parse(path, parser)
    engFormats = getFormats( engDoc, path )

    engNames = {}
    for typ in ['string', 'plurals']:
        for elem in engDoc.findall(typ):
            name = elem.get('name')
            item = { 'type' : typ }
            if typ == 'string':
                item['string'] = elem.text
            else:
                item['strings'] = loadPlural(elem)
            engNames[name] = item
    # print engNames
    
    # iterate over src files
    for subdir, dirs, files in os.walk('res_src'):
        for file in [file for file in files if file == "strings.xml"]:
            path = "%s/%s" % (subdir, file)
            for excpt in excepts:
                if excpt in path :
                    path = None
                    break
            if path: 
                verbose = 0 == len(verboses) or 0 < len([verb for verb in verboses if verb in path])
                print "*** looking at %s ***" % (path)
                dest = path.replace( 'res_src', 'app/src/main/res', 1 )
                checkAndCopy( parser, engNames, engFormats, path, dest, verbose )

##############################################################################
if __name__ == '__main__':
    main()

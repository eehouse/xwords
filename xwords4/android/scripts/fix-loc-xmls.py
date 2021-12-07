#!/usr/bin/python3

import mk_xml, os, sys, getopt, re

from lxml import etree

g_verbose = False
STRINGS_FILE = 'app/src/main/res/values/strings.xml'

def getDocNames( doc ):
    stringNames = {}
    pluralsNames = {}

    for elem in doc.getroot():
        if elem.tag == 'string': stringNames[elem.get('name')] = True
        elif elem.tag == 'plurals': pluralsNames[elem.get('name')] = True

    return { 'stringNames' : stringNames,
             'pluralsNames' : pluralsNames,
         }

def getEnglishNames():
    doc = etree.parse(STRINGS_FILE)
    return getDocNames( doc )

def longestCommon( name, pairs ):
    match = None
    for ii in range(1, len(name)):
        str = name[:ii]
        for key in pairs.keys():
            if str == key[:ii]:
                if g_verbose: print(str, 'matches', key, "so far")
                match = key
                break
    return match

def checkAgainst( doc, pairs ):
    root = doc.getroot();
    done = False
    for child in root.iter():
        if done: break
        if child.tag == "string":
            name = child.get("name")
            if not name in pairs:
                candidate = longestCommon( name, pairs )
                if not candidate: continue
                print(name, 'not found in the English strings')
                print('closest I can find is', candidate)
                print('here are the two strings, English then the other')
                print('English:', pairs[candidate])
                print('Other:  ', child.text)
                print('Replace %s with %s?'  % (name, candidate))
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

def findWithName( doc, name, tag ):
    result = None
    for string in doc.findall(tag):
        if string.get('name') == name:
            result = string
            break
    # if g_verbose: print 'findWithName=>', result, 'for', name
    return result

def makePluralsFrom( src ):
    newNode = etree.fromstring('<plurals name="%s"></plurals>' % (src.get('name')))
    for item in src.findall('item'):
        obj = etree.fromstring('<item quantity="%s">XLATE ME: %s</item>' 
                               % (item.get('quantity'), item.text))
        newNode.append(obj)
    return newNode

def insertAfter( locRoot, englishElem, lastMatch, prevComments ):
    name = englishElem.get('name')
    text = englishElem.text
    if g_verbose: print('insertAfter(', locRoot, englishElem.get('name'),\
                        lastMatch.get('name'), prevComments, ')')
    index = locRoot.getchildren().index(lastMatch)
    if g_verbose: print('index:', index)

    for comment in prevComments:
        commentNode = etree.Comment(comment)
        index += 1
        locRoot.insert( index, commentNode )

    if 'string' == englishElem.tag:
        newNode = etree.fromstring('<string name="%s">XLATE ME: %s</string>' % (name, text))
    elif 'plurals' == englishElem.tag:
        newNode = makePluralsFrom(englishElem)
    else: sys.exit(1)
    index += 1
    locRoot.insert( index, newNode )

def longFormFor(fmt ):
    if fmt == '%s': return '%1$s'
    elif fmt == '%d': return '%1$d'
    else: assert False

def printStats( doc ):
    engNames = getEnglishNames()
    langNames = getDocNames( doc )
    print('strings: English: %d; lang: %d' % (len(engNames['stringNames']),
                                              len(langNames['stringNames'])))
    print('plurals: English: %d; lang: %d' % (len(engNames['pluralsNames']),
                                              len(langNames['pluralsNames'])))

def replacePcts( doc ):
    pat = re.compile( '(%[sd])', re.DOTALL | re.MULTILINE )
    for typ in ['item', 'string']:
        for elem in doc.findall(typ):
            if 'false' == elem.get('formatted'): continue
            splits = re.split( pat, elem.text )
            nParts = len(splits)
            if 1 < nParts:
                for ii in range(nParts):
                    part = splits[ii]
                    if re.match( pat, part ): splits[ii] = longFormFor(part)
                elem.text = ''.join( splits )

# For each name in pairs, check if it's in doc. If not, find the last
# elem before it that is in doc and insert it after.  Start over each
# time to avoid problems with iteration and order
def doAddMissing( doc ):
    locRoot = doc.getroot()
    lastMatch = None
    prevComments = []
    resources = etree.parse(STRINGS_FILE).getroot()
    for elem in resources:
        # if g_verbose: print "got elem:", elem
        tag = elem.tag
        if not isinstance( tag, str ):
            prevComments.append( elem.text )
            # if g_verbose: print "added comment:", elem.text
        elif 'string' == tag or 'plurals' == tag:
            name = elem.get('name')
            match = findWithName( locRoot, name, tag )
            if None == match:
                if g_verbose: print('NO match for', name)
                insertAfter( locRoot, elem, lastMatch, prevComments )
            else:
                lastMatch = match
                lastComments = prevComments
            prevComments = []
        else:
            print('unexpected tag:', elem.tag)
            sys.exit(1)
                    
def compare( engPairs, docPath ):
    locStrings = mk_xml.getStrings( docPath, True )
    engOnly = []
    engOnly = [key for key in engPairs.keys() if not key in locStrings]
    print('%d strings missing from %s: %s' % (len(engOnly), docPath, ", ".join(engOnly)))
    otherOnly = [key for key in locStrings.keys() if not key in engPairs]
    print('%d strings missing from English: %s' % (len(otherOnly), ", ".join(otherOnly)))

def removeNotInEnglish( doc ):
    locRoot = doc.getroot()
    engNames = getEnglishNames()
    for elem in locRoot:
        if not isinstance( elem.tag, str ):
            prevComment = elem
        elif elem.tag == 'string':
            name = elem.get('name')
            if not name in engNames['stringNames']:
                print('removing string', name)
                locRoot.remove(elem)
                if prevComment: locRoot.remove(prevComment)
            prevComment = None
        elif elem.tag == 'plurals':
            name = elem.get('name')
            if not name in engNames['pluralsNames']:
                print('removing plurals', name)
                locRoot.remove(elem)
                if prevComment: locRoot.remove(prevComment)
            prevComment = None
        else: 
            print('unknown tag', elem.tag)
            sys.exit(1)


def usage():
    print("""
    usage:", sys.argv[0]
       -a   # insert missing string elements for translation
       -c   # compare each file with the English, listing string not in both
       -i   # save any changes made (does not by default)
       -f   # work on this strings.xml file (does all if none specified)
       -l   # work on the strings.xml file for this language (e.g. ca, nl)
       -r   # remove elements not present in English
       -s   # print stats
       -%   # replace %[sd] with the correct longer form
""")
    sys.exit(1)

def langFileFor(code):
    return "res_src/values-%s/strings.xml" % code

def main():
    global g_verbose
    stringsFiles = []
    addMissing = False
    doSave = False
    doCompare = False
    doReplace = False
    doRemove = False
    doStats = False
    try:
        pairs, rest = getopt.getopt(sys.argv[1:], "acf:il:rsv%")
        for option, value in pairs:
            if option == '-a': addMissing = True
            elif option == '-c': doCompare = True
            elif option == '-i': doSave = True
            elif option == '-f': stringsFiles.append(value)
            elif option == '-l': stringsFiles.append(langFileFor(value))
            elif option == '-v': g_verbose = True
            elif option == '-r': doRemove = True
            elif option == '-s': doStats = True
            elif option == '-%': doReplace = True
            else: usage()
    except:
        usage()

    pairs = mk_xml.getStrings(STRINGS_FILE, False)

    # Build list of files to work on
    if 0 == len(stringsFiles):
        for subdir, dirs, files in os.walk('res_src'):
            for file in [file for file in files if file == "strings.xml"]:
                stringsFiles.append( "%s/%s" % (subdir, file) )

    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")
    for path in stringsFiles:
        print('looking at', path)
        doc = etree.parse(path, parser)
        # checkAgainst( doc, pairs )
        if doReplace: 
            replacePcts( doc )
        if addMissing: 
            doAddMissing( doc )
        if doCompare: 
            compare( pairs, path )
        if doRemove:
            removeNotInEnglish( doc )
        # print stats after any other changes have been made
        if doStats:
            printStats( doc )
        if doSave:
            try: etree.indent(doc, space='    ')
            except: print('unable to use indent(); check formatting')
            out = open( path, "wb" )
            out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8",
                                       xml_declaration=True ) )


##############################################################################
if __name__ == '__main__':
    main()

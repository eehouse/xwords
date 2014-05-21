#!/usr/bin/python

import mk_xml, os, sys, getopt, re

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

def findWithName( doc, name ):
    result = None
    for string in doc.findall('string'):
        if string.get('name') == name:
            result = string
            break
    print 'findWithName=>', result, 'for', name
    return result

def insertAfter( locRoot, englishElem, lastMatch, prevComments ):
    name = englishElem.get('name')
    text = englishElem.text
    print "insertAfter(", locRoot, englishElem.get('name'), lastMatch.get('name'), prevComments, ")"
    index = locRoot.getchildren().index(lastMatch)
    print 'index:', index

    for comment in prevComments:
        commentNode = etree.Comment(comment)
        index += 1
        locRoot.insert( index, commentNode )

    newNode = etree.fromstring('<string name="%s">XLATE ME: %s</string>' % (name, text))
    index += 1
    locRoot.insert( index, newNode )

def longFormFor(fmt ):
    if fmt == '%s': return '%1$s'
    elif fmt == '%d': return '%1$d'
    else: assert False

def replacePcts( doc ):
    pat = re.compile( '(%[sd])', re.DOTALL | re.MULTILINE )
    for string in doc.findall('string'):
        if string.text:
            splits = re.split( pat, string.text )
            nParts = len(splits)
            if 1 < nParts:
                for ii in range(nParts):
                    part = splits[ii]
                    if re.match( pat, part ): splits[ii] = longFormFor(part)
                string.text = ''.join( splits )

                

# For each name in pairs, check if it's in doc. If not, find the last
# elem before it that is in doc and insert it after.  Start over each
# time to avoid problems with iteration and order
def doAddMissing( doc ):
    done = False
    while not done:
        locRoot = doc.getroot()
        lastMatch = None
        prevComments = []
        for elem in etree.parse("res/values/strings.xml").getroot().iter():
            if not isinstance( elem.tag, basestring ):
                prevComments.append( elem.text )
                print "added comment:", elem.text
            elif 'string' == elem.tag:
                name = elem.get('name')
                match = findWithName( locRoot, name )
                print 'elem', name, 'has comments', prevComments
                if None == match:
                    print 'NO match for', name
                    insertAfter( locRoot, elem, lastMatch, prevComments )
                    done = True
                    # sys.exit(0)
                else:
                    print 'got match for', name
                    lastMatch = match
                    lastComments = prevComments
                prevComments = []
                    
def compare( engPairs, docPath ):
    locStrings = mk_xml.getStrings( docPath )
    engOnly = []
    engOnly = [key for key in engPairs.keys() if not key in locStrings]
    print "%d strings missing from %s: %s" % (len(engOnly), docPath, ", ".join(engOnly))
    otherOnly = [key for key in locStrings.keys() if not key in engPairs]
    print "%d strings missing from English: %s" % (len(otherOnly), ", ".join(otherOnly))

def usage():
    print "usage:", sys.argv[0]
    print "   -a   # insert missing string elements for translation"
    print "   -c   # compare each file with the English, listing string not in both"
    print "   -f   # work on this strings.xml file (does all if none specified)"
    print "   -%   # replace %[sd] with the correct longer form"
    print "   -s   # save any changes made (does not by default)"
    sys.exit(1)

def main():
    stringsFiles = []
    addMissing = False
    doSave = False
    doCompare = False
    doReplace = False
    try:
        pairs, rest = getopt.getopt(sys.argv[1:], "acf:s%")
        for option, value in pairs:
            if option == '-a': addMissing = True
            elif option == '-c': doCompare = True
            elif option == '-%': doReplace = True
            elif option == '-f': stringsFiles.append(value)
            elif option == '-s': doSave = True
            else: usage()
    except:
        usage()

    pairs = mk_xml.getStrings('res/values/strings.xml')

    # Build list of files to work on
    if 0 == len(stringsFiles):
        for subdir, dirs, files in os.walk('res_src'):
            for file in [file for file in files if file == "strings.xml"]:
                stringsFiles.append( "%s/%s" % (subdir, file) )

    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")
    for path in stringsFiles:
        doc = etree.parse(path, parser)
        # checkAgainst( doc, pairs )
        if doReplace: replacePcts( doc )
        if addMissing: doAddMissing( doc )
        if doCompare: compare( pairs, path )
        if doSave:
            out = open( path, "w" )
            out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8", xml_declaration=True ) )


##############################################################################
if __name__ == '__main__':
    main()

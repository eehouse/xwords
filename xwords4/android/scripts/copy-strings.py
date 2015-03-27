#!/usr/bin/python

# Go through all the res_src strings.xml files, and copy them over
# into the world where they'll get used in a build. This is meant to
# allow them to be built-in as an alternative to the
# locutils/downloadable system.

import re, sys, os, getopt
from lxml import etree

def checkAndCopy( engNames, src, dest ):
    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")
    doc = etree.parse(src, parser)
    for elem in doc.getroot().iter():
        if 'resources' == elem.tag:
            pass
        elif 'item' == elem.tag:
            pass
        elif 'string' == elem.tag:
            name = elem.get('name')
            if not name in engNames or not 'string' == engNames[name]:
                print 'removing', name
                elem.getparent().remove( elem )
        elif 'plurals' == elem.tag:
            name = elem.get('name')
            if not name in engNames or not 'plurals' == engNames[name]:
                print 'removing', name
                elem.getparent().remove( elem )
        elif not isinstance( elem.tag, basestring ): # comment
            elem.getparent().remove(elem)
        else:
            print 'unexpected elem:', elem.tag
            sys.exit(1)
    
    if True:
        dir = os.path.dirname( dest )
        try: os.makedirs( dir )
        except: pass
        out = open( dest, "w" )
        out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8", xml_declaration=True ) )

def main():
    # add these via params later
    excepts = ['values-ca_PS', 'values-ba_CK']

    # summarize the english file
    wd = os.path.dirname(sys.argv[0])
    path = wd + '/../XWords4/res/values/strings.xml'
    engNames = {}

    engFormats = {}
    parser = etree.XMLParser(remove_blank_text=True, encoding="utf-8")
    doc = etree.parse(path, parser)
    pat = re.compile( '(%\d\$[sd])', re.DOTALL | re.MULTILINE )
    for typ in ['string', 'plurals']:
        for elem in doc.findall(typ):
            name = elem.get('name')
            engNames[name] = typ

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
                dest = path.replace( 'res_src', 'res', 1 )
                checkAndCopy( engNames, path, dest )

##############################################################################
if __name__ == '__main__':
    main()

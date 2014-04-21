#!/usr/bin/python

import sys, getopt, re
from lxml import etree

FMT = re.compile('.*%\d\$[dsXx].*', re.DOTALL)
FMT = re.compile('(%\d\$[dsXx])', re.DOTALL)

def capitalize( str ):
    split = re.split( FMT, str )
    for ii in range(len(split)):
        part = split[ii]
        if not re.match( FMT, part ):
            split[ii] = split[ii].upper()
    result = ''.join(split)
    print str, '=>', result
    return result;

def reverse( str ):
    return str

def usage():
    print "usage:", sys.argv[0], '-l ca_PS|ba_CK [-o outfile]'
    sys.exit(1)

def main():
    print 'main'
    algo = None
    outfile = None
    try:
        pairs, rest = getopt.getopt(sys.argv[1:], "l:o:")
        for option, value in pairs:
            if option == '-l': algo = value
            elif option == '-o': outfile = value
            else: 
                print 'WTF'
                usage()
    except:
        print 'got except'
        print "Unexpected error:", sys.exc_info()[0]
        usage()

    print 'here'

    if not algo: 
        print "no algo"
        usage()

    print 'here 2'

    if algo == 'ca_PS':
        func = capitalize
    elif algo == 'ba_CK':
        func = reverse
    else:
        print "no algo"
        usage()

    parser = etree.XMLParser(remove_blank_text=True)
    doc = etree.parse("res/values/strings.xml", parser)
    for elem in doc.getroot().iter():
        if 'string' == elem.tag:
            text = elem.text
            if text:
                elem.text = func(text)

    if outfile:
        out = open( outfile, "w" )
        out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8", xml_declaration=True ) )
        
##############################################################################
if __name__ == '__main__':
    main()


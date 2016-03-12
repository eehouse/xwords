#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys, getopt, re, os
from lxml import etree

expr = '(' + \
    '|'.join( [ \
        r'%\d\$[dsXx]', \
        r'\\n', \
        r'\\t', \
        r'\\u[\da-fA-F]{4,4}', \
        r"\\'" , \
        r'\\"' , \
        r'\s' , \
        u'[;:.…]' , \
        ] ) + \
    ')'

# Can't make these work...
        # r'\(' , \
        # r'\)' , \

FMT = re.compile( expr, re.DOTALL )

def capitalize( str ):
    split = re.split( FMT, str )
    for ii in range(len(split)):
        if not re.match( FMT, split[ii] ):
            split[ii] = split[ii].upper()
    result = ''.join(split)
    return result;

def reverse( str ):
    split = re.split( FMT, str )
    for ii in range(len(split)):
        word = split[ii]
        if not re.match( FMT, word ): 
            split[ii] = revcaps(word)
    result = ''.join(split)
    return result

def revcaps(word):
    newWord = list(word[::-1])
    nLetters = len(word)
    for indx in range( nLetters ):
        letter = newWord[indx]
        if word[indx].isupper():
            letter = letter.upper()
        else:
            letter = letter.lower()
        newWord[indx] = letter
    result = ''.join(newWord)
    # print 'revcaps(%s) => %s' % (word, result)
    return result

def usage():
    print "usage:", sys.argv[0], '-l ca_PS|ba_CK [-o outfile]'
    sys.exit(1)

def main():
    algo = None
    outfile = '-'
    try:
        pairs, rest = getopt.getopt(sys.argv[1:], "l:o:")
        for option, value in pairs:
            if option == '-l': algo = value
            elif option == '-o': outfile = value
            else: 
                usage()
    except:
        print "Unexpected error:", sys.exc_info()[0]
        usage()

    if not algo: 
        print "no algo"
        usage()

    if algo == 'ca_PS':
        func = capitalize
    elif algo == 'ba_CK':
        func = reverse
    else:
        print 'no func for algo', algo
        usage()

    parser = etree.XMLParser(remove_blank_text=True)
    doc = etree.parse("res/values/strings.xml", parser)
    for elem in doc.getroot().iter():
        if 'string' == elem.tag or 'item' == elem.tag:
            text = elem.text
            if text:
                elem.text = func(text)

    if '-' == outfile: out = sys.stdout
    else: 
        dir = os.path.dirname( outfile )
        print 'making', dir
        try: os.makedirs( dir )
        except: pass

        out = open( outfile, "w" )
    out.write( etree.tostring( doc, pretty_print=True, encoding="utf-8", xml_declaration=True ) )
        
##############################################################################
if __name__ == '__main__':
    main()


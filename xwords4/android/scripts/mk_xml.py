#!/usr/bin/python3

import glob, sys, re, os, getopt, codecs

from lxml import etree

# sys.exit(0)

g_xmlTypes = [
    { 'elemName': 'item',
      'attrType' : '{http://schemas.android.com/apk/res/android}title'
  },
    { 'elemName': 'Button',
      'attrType' : '{http://schemas.android.com/apk/res/android}text'
  },
    { 'elemName': 'TextView',
      'attrType' : '{http://schemas.android.com/apk/res/android}text'
  },
    { 'elemName': 'CheckBox',
      'attrType' : '{http://schemas.android.com/apk/res/android}text'
  },
    { 'elemName': 'EditText',
      'attrType' : '{http://schemas.android.com/apk/res/android}hint'
  },
      
]

g_pairs = {}

STR_REF = re.compile('@string/(.*)$')
CLASS_NAME = re.compile('.*/([^/*]+).java')

def xform(src, dest):
    doc = etree.parse(src)
    root = doc.getroot();
    for child in root.iter():
        for elem in g_xmlTypes:
            if child.tag == elem['elemName']:
                value = child.get(elem['attrType'])
                match = value and STR_REF.match(value)
                if match: 
                    key = match.group(1)
                    if key in g_pairs:
                        child.set(elem['attrType'], "loc:" + key)

    # create directory if needed, then write file
    dir = os.path.dirname( dest )
    if not os.path.exists(dir): os.makedirs(dir)
    doc.write( dest, pretty_print=True )

# For my records: you CAN harvest a comment!!!
# def loadAndPrint(file):
#     prevComment = None
#     doc = etree.parse(file)
#     for elem in doc.getroot().iter():
#         if not isinstance( elem.tag, basestring ):
#             prevComment = elem.text
#         else:
#             print "elem:", elem,
#             if prevComment:
#                 print '//', prevComment
#                 prevComment = None
#             else:
#                 print
#     # doc.write( sys.stdout, pretty_print=True )

def checkText( text ):
    if text:
        text = " ".join(re.split('\s+', text)).replace('"', '\"')
        seen = set()
        split = re.split( '(%\d\$[sd])', text )
        for part in split:
            if 4 <= len(part) and '%' == part[0]:
                digit = int(part[1:2])
                if digit in seen: pass
                    # print "ERROR: has duplicate format digit %d
                    # (text = %s)" % (digit, text) print "This might
                    # not be what you want"
                seen.add( digit )
    return text

def printStrings( pairs, outfile, target ):
    match = CLASS_NAME.match(outfile)
    if not match:
        print( "did you give me a java file?:", outfile )
        sys.exit(0)
    name = match.group(1)
    fil = codecs.open( outfile, "w", "utf-8" )

    # beginning of the class file
    lines = """
/***********************************************************************
* Generated file (by %s); do not edit!!! 
***********************************************************************/
package org.eehouse.android.xw4.loc;

import android.content.Context;

import org.eehouse.android.xw4.Assert;
import org.eehouse.android.xw4.R;
import org.eehouse.android.xw4.Log;

public class %s {
    private static final String TAG = %s.class.getSimpleName();
    public static final int NOT_FOUND = -1;
    protected static final int[] S_IDS = {
"""
    fil.write( lines % (sys.argv[0], name, name) )

    keys = [ key for key in pairs.keys() ]
    for ii in range( len( keys ) ):
        key = keys[ii]
        fil.write( "        /* %04d */ R.string.%s,\n" % (ii, key) )

    fil.write( "    };\n\n" )

    if "debug" == target:
        fil.write( "    static final String[] strs = {\n")
        for ii in range( len( keys ) ):
            key = keys[ii]
            fil.write( "        /* %04d */ \"%s\",\n" % (ii, pairs[key]['text']) )
        fil.write( "    };\n" );

    func = "\n    protected static void checkStrings( Context context ) \n    {\n"
    if "debug" == target:
        func += """
        int nMatches = 0;
        for ( int ii = 0; ii < strs.length; ++ii ) {
            String fromCtxt = context.getString( S_IDS[ii] );
            if ( strs[ii].equals( fromCtxt ) ) {
                ++nMatches;
            } else {
                Log.i( TAG, "unequal strings: \\"%%s\\" vs \\"%%s\\"",
                       strs[ii], fromCtxt );
            }
        }
        Log.i( TAG, "checkStrings: %%d of %%d strings matched", nMatches, strs.length );
"""
    func += "    }"

    fil.write( func )

    # Now the end of the class
    lines = """
}
/* end generated file */
"""
    fil.write( lines )

def getStrings( path, dupesOK ):
    pairs = {}
    texts = {}
    prevComments = []
    foundDupe = False
    for elem in etree.parse(path).getroot().iter():
        if not isinstance( elem.tag, str ):
            prevComments.append( elem.text )
        elif 'string' == elem.tag:
            name = elem.get('name')
            text = checkText( elem.text )
            if text in texts and not dupesOK:
                print( "duplicate string: name: {}, text: '{}'".format(name, text) )
                foundDupe = True
            texts[text] = True
            rec = { 'text' : text }
            if 0 < len(prevComments):
                rec['comments'] = prevComments
                prevComments = []
            # not having a name is an error!!!
            pairs[name] = rec
    
    if foundDupe:
        print( "Exiting: please remove duplicates listed above from", path )
        sys.exit(1);

    return pairs

def main():
    outfile = ''
    outfileDbg = ''
    target=''
    pairs, rest = getopt.getopt(sys.argv[1:], "o:t:d:")
    for option, value in pairs:
        if option == '-o': outfile = value
        elif option == '-t': target = value

    # Gather all localizable strings
    pairs = getStrings("app/src/main/res/values/strings.xml", False)

    # for subdir, dirs, files in os.walk('res_src'):
    #     for file in files:
    #         src = subdir + '/' + file
    #         dest = src.replace( 'res_src', 'res', 1 )
    #         xform( src, dest )

    if outfile: printStrings( pairs, outfile, target )

##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/python

import re, sys
from lxml import etree
import mygit, xwconfig


# Take an English strings.xml file and another, "join" them on the
# name of each string, and then produce an array that's a mapping of
# English to the other. Get ride of extra whitespace etc in the
# English strings so they're identical to how an Android app displays
# them.

english = 'res/values/strings.xml'
other_f = 'res_src/values-%s/strings.xml'

def readIDs(rDotJava):
    ids = {}
    start = re.compile('\s*public static final class string {\s*')
    end = re.compile('\s*}\s*')
    entry = re.compile('\s*public static final int (\S+)=(0x.*);\s*')
    inLine = False
    for line in rDotJava.splitlines():
        if inLine:
            if end.match(line):
                break
            else:
                match = entry.match(line)
                if match:
                    name = match.group(1)
                    value = int(match.group(2), 16)
                    ids[name] = value
        elif start.match(line):
            inLine = True
    return ids

def asMap( repo, rev, path, ids ):
    map = None
    data = repo.cat( path, rev )
    if data:
        map = {}
        doc = etree.fromstring( data )
        for elem in doc.iter():
            if 'string' == elem.tag:
                text = elem.text
                if text:
                    # print 'text before:', text
                    text = " ".join(re.split('\s+', text)) \
                        .replace("\\'", "'") \
                        .replace( '\\"', '"' )
                    # print 'text after:', text
                    name = elem.get('name')
                    # All should be there, but aren't yet, and I wanna
                    # test
                    if name in ids:
                        id = ids[name]
                        map[id] = text
    return map

# Build from the most recent revisions of the english and locale
# strings.xml files that are compatible with (haven't changed since)
# stringsHash on the R.java file.  For now, just get what matches,
# assuming that all are updated with the same commit -- which they
# aren't.
#
# The stringsHash is hard-coded for an app that's shipped (and based
# on its R.java file), but both the English and (especially) the
# non-English strings.xml files can change after.  We want the newest
# of each that's still compatible with the ids compiled into the app.
# So we look for any change to R.java newer than stringsHash, and move
# backwards from one-before there to find the first (newest) version
# of the english and localized strings.xml
#
# So for R.java, we generate a list of revisions of it from HEAD back
# to the one we know.  Taking the revision immediately after the one
# we know, we generate a list from it back to the one we know.  The
# second revision in that list is the identifier of the newest
# strings.xml we an safely use.
# 
def getXlationFor( repo, rDotJava, locale, firstHash ):
    ids = readIDs(rDotJava)

    eng = asMap( repo, firstHash, english, ids )
    other = asMap( repo, firstHash, other_f % (locale), ids )
    if not other:
        locale = locale.split('_')
        if 2 == len(locale):
            other = asMap( repo, firstHash, other_f % (locale[0]), ids )
    result = []
    if other:
        for key in eng.keys():
            if key in other:
                result.append( { 'id' : key, 'loc' : other[key] } )
    return result

def main():
    repo = mygit.GitRepo( xwconfig.k_REPOPATH )
    head = repo.getHeadRev()

    # testing with the most recent (as of now) R.java change
    hash = repo.getRevsBetween( head, '33a83b0e2fcf062f4f640ccab0785b2d2b439542', \
                                'R.java' )[0]
    print 'using hash:', hash

    rDotJava = repo.cat( 'R.java', hash )
    data = getXlationFor( repo, rDotJava, 'ca_PS', hash )
    print 'data :' , data
    data = getXlationFor( repo, rDotJava, 'ba_CK', hash )
    print 'data :' , data

##############################################################################
if __name__ == '__main__':
    main()

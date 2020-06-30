#!/usr/bin/env python3

import sys

"""
print stats about in input stream that's assumed to be a dictionary.
Counts and percentages of each letter, as well as total numbers of
words.  This is not part of the dictionary build process.  I use it
for creating info.txt files for new languages and debugging the
creation of dictionaries from new wordlists.

Something like this might form the basis for choosing counts and
values for tiles without using the conventions established by
Scrabble players.  This isn't enough, though: the frequency of
letter tuples and triples -- how often letters appear together -- is
a better indicator than just letter count.
"""



def main():
    wordSizeCounts = {}
    letterCounts = {}
    wordCount = 0
    letterCount = 0
    enc = 'utf8'               # this could be a cmdline arg....
	
    for line in sys.stdin.readlines():
        line = line.strip()

        length = len(line)
        if not length in wordSizeCounts: wordSizeCounts[length] = 0
        wordSizeCounts[length] += 1
        wordCount += 1

        for letter in line:
            ii = ord(letter)
            # perl did this:  die "$0: this is a letter?: $ii" if $ii <= 32 && $ii >= 4 && $ii != 0; 
            assert ii > 32 or ii < 4 or ii == 0, 'letter {} out of range'.format(ii)
            if not letter in letterCounts: letterCounts[letter] = 0
            letterCounts[letter] += 1
            letterCount += 1

    print( 'Number of words: {}'.format(wordCount))
    print( 'Number of letters: {}'.format(letterCount))
    print('')

    print( '**** word sizes ****' )
    print( 'SIZE  COUNT   PERCENT' )
    pctTotal = 0.0
    wordTotal = 0
    for ii in sorted(wordSizeCounts):
        count = wordSizeCounts[ii]
        wordTotal += count
        pct = (100.00 * count)/wordCount
        pctTotal += pct
        print( '{:2d}   {:6d}    {:02.2f}'.format(ii, count, pct))

    print( '-------------------------------' )
    print('     {:6d}  {:.2f}'.format( wordTotal, pctTotal))
    print('')

    lineNo = 1
    pctTotal = 0.0
    print( '**** Letter counts ****' )
    print( '     ASCII  ORD  HEX     PCT (of {})'.format(letterCount))
    for letter in sorted(letterCounts):
        count = letterCounts[letter]
        pct = (100.00 * count) / letterCount
        pctTotal += pct
        print( '{:2d}: {: >6s}   {:2d}   {:x}    {:5.2f} ({:d})' \
               .format(lineNo, letter, ord(letter), ord(letter), pct, count ) )
        lineNo += 1

    print('percent total {:.2f}'.format( pctTotal))
    print('')

##############################################################################
if __name__ == '__main__':
    main()

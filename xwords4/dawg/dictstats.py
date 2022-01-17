#!/usr/bin/env python3

import argparse, sys
from collections import defaultdict

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

class Letter:
    def __init__(self, ch):
        self.ch = ch
        self.count = 0

    def increment(self): self.count += 1
    def getChr(self): return self.ch
    def getCount(self): return self.count

    def format(self, total):
        count = self.count
        pct = (100.00 * count) / total
        return '{: >6s}   {:2d}   {:x}    {:5.2f} ({:d})' \
            .format(self.ch, ord(self.ch), ord(self.ch), pct, self.count )


def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sort-by', dest = 'SORT', type = str, default = 'ASCII',
                        help = 'sort output by ASCII or COUNT')
    parser.add_argument('--enc', dest = 'ENC', type = str, default = 'utf8',
                        help = 'encoding')
    return parser

def main():
    args = mkParser().parse_args()

    letters = {}
    wordSizeCounts = defaultdict(int)
    # letterCounts = defaultdict(int)
    wordCount = 0
    letterCount = 0
	
    for line in sys.stdin.readlines():
        line = line.strip()

        length = len(line)
        # if not length in wordSizeCounts: wordSizeCounts[length] = 0
        wordSizeCounts[length] += 1
        wordCount += 1

        for letter in line:
            ii = ord(letter)
            assert ii > 32 or ii < 4 or ii == 0, 'letter {} out of range'.format(ii)
            if not letter in letters: letters[letter] = Letter(letter)
            letters[letter].increment()
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

    print( '**** Letter counts ****' )
    print( '     ASCII  ORD  HEX     PCT (of {})'.format(letterCount))

    if args.SORT == 'ASCII':
        key = lambda letter: ord(letter.getChr())
    elif args.SORT == 'COUNT':
        key = lambda letter: -letter.getCount()
    else:
        print('error: bad sort arg: {}'.format(args.SORT))
        sys.exit(1)
    lineNo = 1
    for letter in sorted(letters.values(), key=key):
        print('{:2d}: {}'.format(lineNo, letter.format(letterCount)))
        lineNo += 1

    print('')

##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/env python3

import argparse, subprocess

g_pairs = [
    (['_*ING'], [('RINGER', False)]),
    (['_{2}ING'], [('RING', False), ('DOING', True), ('SPRING', False)]),
    (['_{2}'], [('DO', True)]),
    (['D_{1,2}'], [('DOG', True), ('DO', True), ('D', False)]),
    (['A_*'], [('ABLE', True), ('BALL', False),]),
    (['B_*'], [('BALL', True), ('APPLE', False), ('CAT', False),],),
    (['ABC'], [('ABC', True), ('CBA', False)]),
    (['_*'], [('ABC', True)]),
    (['_*Z'], [('ABC', False), ('AB', False)]),
    (['_*C'], [('ABC', True)]),
    (['_*B'], [('AB', True)]),
    (['B_*'], [('AB', False), ('BA', True)]),

    (['A*'], [('A', True)]),
    (['A*A*'], [('A', True), ('AA', True), ('AAA', True), ('AAAA', True), ('AABA', False)]),
    (['A*A*B'], [('B', True), ('C', False)]),

    (['A{3}'], [('AAA', True)]),
    (['A{1}A{1}A{1}'], [('AAA', True),('AA', False),('AAAA', False)]),
    (['A*A*A*'], [('A', True),('AA', True),('AAA', True),('AAAA', True), ('ABAA', False)]),

    (['AB*'], [('A', True)]),

    (['A{2,4}'], [('A', False), ('AA', True), ('AAAA', True), ('AAAAA', False)]),
    (['_*ING'], [('RINGER', False)]),
    (['R_*'], [('RINGER', True)]),
    (['R_*', '_*ING'], [('RING', True), ('ROLLING', True), ('SPRING', False), ('INGER', False), ('RINGER', False)]),
    (['A', '_*'], [('ABC', False), ('CBA', False), ('A', True)]),
    (['ABC', '_*'], [('ABC', True)]),

    (['[ABC]{3}'], [('ABC', True), ('CBA', True), ('AAA', True), ('BBB', True)]),
    (['[+ABC]{3}'], [('ABC', True), ('CBA', True), ('AAA', False), ('AA', False), ]),
    (['[+ABC]{3}_*'], [('ABC', True), ('CBA', True), ('AAA', False), ('AA', False), ]),
    (['AA[+ABC]{3}ZZ'], [('AAABCZZ', True), ('AACBAZZ', True), ('AAAAAZZ', False), ]),

    (['[+AAB]{3}'], [('AAB', True), ('ABA', True), ('ABB', False), ('AB', False),
                     ('ABBA', False),]),
    (['[+AB_]{3}'], [('AAB', True), ('ABA', True), ('ABB', True), ('ABC', True),
                     ('AB', False), ('ABBA', False),]),
    (['[+_AB]{3}'], [('AAB', True), ('ABA', True), ('ABB', True), ('ABC', True),
                     ('AB', False), ('ABBA', False),]),
]

g_dict = '../android/app/src/main/assets/BasEnglish2to8.xwd'

def doTest( pats, pair, stderr ):
    # print('pair: {}'.format(pair))
    (word, expect) = pair
    args = ['./obj_linux_memdbg/xwords', '--test-dict', g_dict, '--test-string', word ]
    for pat in pats:
        args += ['--test-pat', pat]
    result = subprocess.run(args, stderr=stderr)
    passed = (0 == result.returncode) == expect
    return passed

def main():
    stderr = subprocess.DEVNULL
    parser = argparse.ArgumentParser()
    parser.add_argument('--with-stderr', dest = 'STDERR', action = 'store_true',
                        help = 'don\'t discard stderr')
    parser.add_argument('--do-only', dest = 'SPECIFIED', type = int, action = 'append', 
                        help = 'do this test case only')
    parser.add_argument('--show-all', dest = 'PRINT_ALL', action = 'store_true', default = False,
                        help = 'print successes in addition to failures')

    args = parser.parse_args()
    if args.STDERR: stderr = None

    ii = 0
    counts = { False: 0, True: 0 }
    for cases in g_pairs:
        (pats, rest) = cases
        for one in rest:
            if not args.SPECIFIED or ii in args.SPECIFIED:
                success = doTest(pats, one, stderr)
                note = success and 'passed' or 'FAILED'
                if args.PRINT_ALL or not success:
                    print( '{:2d} {}: {} {}'.format(ii, note, pats, one))
                counts[success] += 1
            ii += 1

    print('Successes: {}'.format(counts[True]))
    print('Failures: {}'.format(counts[False]))

##############################################################################
if __name__ == '__main__':
    main()

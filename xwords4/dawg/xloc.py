#!/usr/bin/env python3
import argparse, os, re, struct, sys

def errorOut(msg):
    print('ERROR: {}'.format(msg))
    sys.exit(1)

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('-enc', dest = 'ENCODING', type = str, help = 'use this encoding' )
    parser.add_argument('-tn', dest = 'DO_TABLE', action = 'store_true',
                        help = 'output table file' )
    parser.add_argument('-oc', dest = 'DO_OTHERCOUNTS', action = 'store_true',
                        help = 'write extra (non-15x15 board) counts' )
    parser.add_argument('-info', dest = 'INFO_KEY', type = str,
                        help = 'info.txt keyword to write null-terminated' )

    # parser.add_argument('-tn', dest = 'UNICODE', default = False,
    #                     action = 'store_true', help = 'assume unicode')
    # parser.add_argument('-t', dest = 'UNICODE', type = str, default = True,
    #                     action = 'store_false', help = 'DO NOT assume unicode')
    parser.add_argument('-v', dest = 'DO_VALS', action = 'store_true', help = 'output values file' )
    parser.add_argument('-s', dest = 'DO_SIZE', action = 'store_true', help = 'output size file')
    parser.add_argument('-out', dest = 'OUTFILE', type = str, help = 'outfile path')

    parser.add_argument('--table-file', dest = 'TABLE_FILE', type = str, help = 'write table file here')
    parser.add_argument('--size-file', dest = 'SIZE_FILE', type = str, help = 'write size file here')
    parser.add_argument('--vals-file', dest = 'VALS_FILE', type = str, help = 'write vals file here')

    return parser

sPreComment = re.compile('^(.*)#.*$')
sVarAssign = re.compile('^(\w+):(.*)$')
sBeginTiles = re.compile('^<BEGIN_TILES>$')
sEndTiles = re.compile('^<END_TILES>$')
sSingleCharMatch = re.compile("'(.(\|.)*)'")
sSpecialsMatch = re.compile('{"(.+)"(,.+)?}')

def parseTileInfo(infoFile, encoding):
    result = {'_TILES' : []}
    with open(infoFile, 'rt') as file:
        data = file.read()
        # if encoding:
        #     data = data.decode(encoding)
        data = data.split('\n')

        inTiles = False
        tiles = []
        for line in data:
            # print('line at start: {}'.format(line))
            match = sPreComment.match(line)
            if match:
                line = match.group(1)
                # print('line sans comment: {}'.format(line))
            if 0 == len(line):continue

            if inTiles:
                if sEndTiles.match(line):
                    break
                else:
                    (face, val, counts) = line.split(None, 2)
                    result['_TILES'].append({'counts': counts,
                                             'val': val,
                                             'face': face})
            elif sBeginTiles.match(line):
                inTiles = True
            else:
                match = sVarAssign.match(line)
                if match:
                    var = match.group(1)
                    if not var in result: result[var] = ''
                    result[var] += match.group(2)

    return result
            
def printLetters( letters, outfile ):
    letters = letters.split('|')
    letters = ' '.join(letters)
    outfile.write(letters.encode('utf8'))

def writeInfoFile(xlocToken, key, outfile):
    val = xlocToken[key]
    assert val
    outfile.write(val.encode('utf8'))
    outfile.write(struct.pack('B', 0 ))

def writeMapFile(xlocToken, outfile):
    print('writeMapFile(out={})'.format(outfile))
    tiles = xlocToken['_TILES']
    specialCount = 0
    for tile in tiles:
        face = tile['face']
        match = sSingleCharMatch.match(face)
        if match:
            printLetters( match.group(1), outfile )
            continue
        match = sSpecialsMatch.match(face)
        if match:
            print('specials char: {}'.format(match.group(1)))
            outfile.write(struct.pack('B', specialCount ))
            specialCount += 1
            continue

        print('bad/unmatched face: {}'.format(face))
        assert False

def writeValuesFile(xlocToken, outfile):
    header = xlocToken.get('XLOC_HEADER') or errorOut('no XLOC_HEADER found')

    print('writeValuesFile(out={}): writing header: {}'.format(outfile, header))
    outfile.write(struct.pack('!H', int(header, 16)))

    cs = xlocToken.get('COUNT_SIZES', '15').split()
    useOffset = cs.index('15')

    nCounts = 0
    for tile in xlocToken['_TILES']:
        counts = tile['counts'].split()
        assert nCounts == 0 or nCounts == len(counts)
        nCounts = len(counts)
        assert nCounts == len(cs)
        outfile.write(struct.pack('B', int(counts[useOffset])))

        val = int(tile['val'])
        outfile.write(struct.pack('B', val))

def writeOtherCounts(xlocToken, outfile):
    cs = xlocToken.get('COUNT_SIZES', '15').split()

    tiles = xlocToken['_TILES']
    # Write the size of the data so it can be skipped by the reader,
    # which won't know how many faces the tile set has yet.
    totalSiz = (len(cs) - 1) * (1 + len(tiles))
    outfile.write(struct.pack('B', totalSiz))

    for useOffset in range(len(cs)):
        siz = int(cs[useOffset])
        if siz == 15: continue
        outfile.write(struct.pack('B', siz))
        for tile in tiles:
            count = tile['counts'].split()[useOffset]
            outfile.write(struct.pack('B', int(count)))

def main():
    print('{}.main {} called'.format(sys.argv[0], sys.argv[1:]))
    args = mkParser().parse_args()

    infoFile = 'info.txt'
    if not os.path.exists(infoFile):
        errorOut('{} not found'.format(infoFile))
    xlocToken = parseTileInfo(infoFile, args.ENCODING)

    if args.INFO_KEY and args.OUTFILE:
        with open(args.OUTFILE, 'wb') as outfile:
            writeInfoFile(xlocToken, args.INFO_KEY, outfile);

    if args.DO_TABLE or args.TABLE_FILE:
        path = args.TABLE_FILE or args.OUTFILE
        with open(path, 'wb') as outfile:
            writeMapFile(xlocToken, outfile);

    if args.DO_SIZE or args.SIZE_FILE:
        path = args.SIZE_FILE or args.OUTFILE
        with open(path, 'wb') as outfile:
            count = len(xlocToken['_TILES'])
            outfile.write(struct.pack('B', count))

    if args.DO_VALS or args.VALS_FILE:
        path = args.VALS_FILE or args.OUTFILE
        with open(path, 'wb') as outfile:
            writeValuesFile( xlocToken, outfile )

    if args.DO_OTHERCOUNTS and args.OUTFILE:
        with open(args.OUTFILE, 'wb') as outfile:
            writeOtherCounts(xlocToken, outfile)

##############################################################################
if __name__ == '__main__':
    main()

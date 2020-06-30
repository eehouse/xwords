#!/usr/bin/env python3
import argparse, os, re, struct, sys

def errorOut(msg):
    print('ERROR: {}'.format(msg))
    sys.exit(1)

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('-enc', dest = 'ENCODING', type = str, help = 'use this encoding' )
    parser.add_argument('-tn', dest = 'DO_TABLE', action = 'store_true', help = 'output table file' )
    # parser.add_argument('-tn', dest = 'UNICODE', default = False,
    #                     action = 'store_true', help = 'assume unicode')
    # parser.add_argument('-t', dest = 'UNICODE', type = str, default = True,
    #                     action = 'store_false', help = 'DO NOT assume unicode')
    parser.add_argument('-v', dest = 'DO_VALS', action = 'store_true', help = 'output values file' )
    parser.add_argument('-s', dest = 'DO_SIZE', action = 'store_true', help = 'output size file')
    parser.add_argument('-out', dest = 'OUTFILE', type = str, help = 'outfile path')
    return parser

sPreComment = re.compile('^(.*)#.*$')
sVarAssign = re.compile('^(\w+):(.*)$')
sBeginTiles = re.compile('^<BEGIN_TILES>$')
sEndTiles = re.compile('^<END_TILES>$')
sSingleCharMatch = re.compile('\'(.(\|.)+)\'')
sSpecialsMatch = re.compile('{"(.+)"}')

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
                    (count, val, face) = line.split(None, 3)
                    result['_TILES'].append((count, val, face))
            elif sBeginTiles.match(line):
                inTiles = True
            else:
                match = sVarAssign.match(line)
                if match:
                    var = match.group(1)
                    if not var in result: result[var] = ''
                    result[var] += match.group(2)

    return result
            
class XLOC():
    None
        
def readXLOC():
    return XLOC()

# sub WriteMapFile($$$) {
#     my ( $hashR, $unicode, $fhr ) = @_;

#     my $count = GetNTiles($hashR);
#     my $specialCount = 0;
#     for ( my $i = 0; $i < $count; ++$i ) {
#         my $tileR = GetNthTile( $hashR, $i );
#         my $str = ${$tileR}[2];

#         if ( $str =~ /\'(.(\|.)*)\'/ ) {
#             printLetters( $1, $fhr );
#         } elsif ( $str =~ /\"(.+)\"/ ) {
#             print $fhr pack( "c", $specialCount++ );
#         } elsif ( $str =~ /(\d+)/ ) {
#             print $fhr pack( "n", $1 );
#         } else {
#             die "WriteMapFile: unrecognized face format $str, elem $i";
#         }
#     }
# } # WriteMapFile

def printLetters( letters, outfile ):
    letters = letters.split('|')
    letters = ' '.join(letters)
    outfile.write(letters.encode('utf8'))

def writeMapFile(xlocToken, outfile):
    print('writeMapFile()')
    tiles = xlocToken['_TILES']
    specialCount = 0
    for tile in tiles:
        face = tile[2]
        match = sSingleCharMatch.match(face)
        if match:
            print('single char: {}'.format(match.group(1)))
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

    print('writing header: {}'.format(header))
    outfile.write(struct.pack('!H', int(header, 16)))

    for tile in xlocToken['_TILES']:
        val = int(tile[0])
        count = int(tile[1])
        outfile.write(struct.pack('BB', val, count))

def main():
    print('{}.main {} called'.format(sys.argv[0], sys.argv[1:]))
    args = mkParser().parse_args()
    assert args.OUTFILE

    infoFile = 'info.txt'
    if not os.path.exists(infoFile):
        errorOut('{} not found'.format(infoFile))
    xlocToken = parseTileInfo(infoFile, args.ENCODING)

    xloc = readXLOC()
    
    with open(args.OUTFILE, 'wb') as outfile:
        if args.DO_TABLE:
            writeMapFile(xlocToken, outfile);
        elif args.DO_SIZE:
            assert not args.DO_VALS
            count = len(xlocToken['_TILES'])
            outfile.write(struct.pack('!B', count))
        elif args.DO_VALS:
            assert not args.DO_SIZE
            writeValuesFile( xlocToken, outfile )


##############################################################################
if __name__ == '__main__':
    main()

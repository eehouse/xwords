#!/usr/bin/env python3

import argparse, io, struct, sys

oneByteFmt = struct.Struct('B')
SPACE = ' '

def getNullTermParam(fh):
    msg = ""
    while True:
        (oneChar) = oneByteFmt.unpack(fh.read(oneByteFmt.size))
        if int(oneChar[0]) == 0: break
        msg += chr(oneChar[0])
    return msg

def addFace( faces, face ):
    assert face
    faces.append( face )
    # print( 'addFace(): added:', face, ' now have', len(faces), 'faces' )

# Each face is one or two synonym strings (typically the upper- and
# lower-case versions of a tile face), with a space as separator in the
# two-string case.
#
# Each letter read is a space/delimiter or not. If it's a delimiter,
# then we append what we had before. Otherwise if we have something
# already then this is a new face starting. Otherwise it's a synonym
# for something we're processing already.
    
def splitFaces( buf ):
    faces = []
    synonyms = None
    lastWasDelim = False
    for oneChar in buf:
        # print('read char', oneChar)

        if oneChar == SPACE:
            assert synonyms     # there's better be one already
            lastWasDelim = True
        else:
            # print( "read non-delim char:", oneChar )
            if lastWasDelim:
                assert len(synonyms) == 1
                synonyms.append( oneChar )
                addFace( faces, synonyms )
                synonyms = None
                lastWasDelim = False
            else:
                if synonyms:
                    addFace( faces, synonyms )
                synonyms = [ oneChar ]

    if synonyms: addFace( faces, synonyms )

    return faces

def loadCountsAndValues( fh, numFaces, data ):
    twoBytesFmt = struct.Struct('BB')
    for ii in range(numFaces):
        pair = twoBytesFmt.unpack(fh.read(twoBytesFmt.size))
        data[ii]['count'] = int(pair[0])
        data[ii]['val'] = int(pair[1])

def eatBitmap( fh ):
    nCols = int(oneByteFmt.unpack(fh.read(oneByteFmt.size))[0])
    if nCols > 0:
        nRows = int(oneByteFmt.unpack(fh.read(oneByteFmt.size))[0])
        nBytes = ((nRows*nCols)+7) // 8
        print('eatBitmap(): skipping {} bytes; nCols: {}, nRows: {}:'.format(nBytes,nCols, nRows),\
              file=sys.stderr)
        fh.read(nBytes)

def loadSpecialData( fh, data ):
    count = 0
    lastSpecial = ord(' ')
    for datum in data:
        # print('loadSpecialData: comparing', ord(datum['faces'][0]), 'with', lastSpecial)
        if len(datum['faces']) == 1 and ord(datum['faces'][0]) < lastSpecial:
            count += 1
            txtlen = int(oneByteFmt.unpack(fh.read(oneByteFmt.size))[0])
            txt = fh.read(txtlen).decode("UTF-8")
            # print('loadSpecialData(): found:', txt, 'of len', txtlen)
            datum['faces'] = txt.split( SPACE )
            eatBitmap( fh )
            eatBitmap( fh )

def loadNodes( dawg, nodeSize ):
    nodes = []
    fmtStr = ''
    for ii in range(nodeSize): fmtStr += 'B'
    fmt = struct.Struct(fmtStr)

    while True:
        buf = dawg.read(nodeSize)
        if len(buf) == 0: break
        assert len(buf) == nodeSize
        arr = fmt.unpack(buf)
        val = 0
        for elem in arr:
            val = (val << 8) + elem
        nodes.append(val)
        # print('loaded node 0x{:x} of len {}'.format(val, nodeSize))
    return nodes

def parseNode( node, nodeSize ):

    if nodeSize == 4:
        accepting = (node & 0x00008000) != 0
        isLast = (node & 0x00004000) != 0
        chrIndex = (node & 0x00003f00) >> 8
        nextEdge = (node >> 16) + ((node & 0x000000FF) << 16)
    elif nodeSize == 3:
        accepting = (node & 0x00000080) != 0
        isLast = (node & 0x00000040) != 0
        chrIndex = node & 0x0000001f
        nextEdge = (node >> 8) + ((node & 0x00000020) << 11)
    
    return (nextEdge, chrIndex, accepting, isLast )

def expandDAWG( nodes, nodeSize, delim, indx, data, words, letters = [] ):
    if len(letters) > 15: error( "infinite recursion???" )

    while True:
        node = nodes[indx]
        indx += 1
        ( nextEdge, chrIndex, accepting, lastEdge ) = parseNode( node, nodeSize )

        letters.append( data[chrIndex]['faces'][0] )
        if accepting:
            words.append( delim.join(letters) )

        if nextEdge != 0:
            expandDAWG( nodes, nodeSize, delim, nextEdge, data, words, letters )

        letters.pop()

        if lastEdge: break

def process(args):
    DICT_SYNONYMS_MASK = 0x10
    DICT_HEADER_MASK = 0x08

    with open(args.DAWG, "rb") as dawg:
        nWords = 0

        headerFmt = struct.Struct('!HH')
        (flags, headerLen) = headerFmt.unpack(dawg.read(headerFmt.size))
        print( 'read flags: {:x}, header len: {}'.format(flags, headerLen ),
               file=sys.stderr )
        if not 0 == DICT_HEADER_MASK & flags:
            flags &= ~DICT_HEADER_MASK
            header = io.BytesIO(dawg.read(headerLen))
            nWordsFmt = struct.Struct('!L')
            nWords = nWordsFmt.unpack(header.read(nWordsFmt.size))[0]
            
            print( 'header: read nWords: {}'.format(nWords ), file=sys.stderr )

            try: # older wordlists won't have these
                msg = getNullTermParam(header)
                if args.DUMP_MSG:
                    print( 'msg: {}'.format(msg))
                md5Sum = getNullTermParam(header)
                print( 'header: read sum: {}'.format(md5Sum), file=sys.stderr )
            except:
                md5Sum = None

            if args.GET_SUM:
                print( '{}'.format(md5Sum), file=sys.stdout )
                sys.exit(0)

        nodeSize = 0
        isUTF8 = False
        flags &= ~DICT_SYNONYMS_MASK
        if flags == 0x0002:
            nodeSize = 3
        elif flags == 0x0003:
            nodeSize = 4
        elif flags == 0x0004:
            isUTF8 = True
            nodeSize = 3
        elif flags == 0x0005:
            isUTF8 = True
            nodeSize = 4
        else:
            error("unexpected flags value")
        print( 'nodesize: {}, isUTF8: {}'.format(nodeSize, isUTF8), file=sys.stderr )

        numFaceBytes = 0
        if isUTF8:
            numFaceBytes = oneByteFmt.unpack(dawg.read(oneByteFmt.size))[0]
        numFaces = int(oneByteFmt.unpack(dawg.read(oneByteFmt.size))[0])
        if not isUTF8:
            numFaceBytes = numFaces * 2
        assert numFaces <= 64, 'too many faces: {}'.format(numFaces)
        print( 'numFaceBytes: {}, numFaces: {}'.format(numFaceBytes, numFaces), file=sys.stderr )

        print( 'TODO: confirm checksum', file=sys.stderr )

        data = []
        if isUTF8:
            faceBytes = dawg.read(numFaceBytes).decode("UTF-8")
            faces = splitFaces( faceBytes )
            assert( len(faces) == numFaces )
            # print( 'loaded', len(faces), 'faces:', faces )
            for datum in faces:
                data.append({'faces' : datum })
        else:
            error('I don\'t handle obsolete ascii case')

        langCode = 0x7F & oneByteFmt.unpack(dawg.read(oneByteFmt.size))[0]
        dawg.read( oneByteFmt.size ) # skip byte

        loadCountsAndValues( dawg, numFaces, data )
        loadSpecialData( dawg, data )

        offsetStruct = struct.Struct('!L')
        assert offsetStruct.size == 4
        offset = int(offsetStruct.unpack(dawg.read(offsetStruct.size))[0])

        if args.DUMP_TILES:
            for ii in range(len(data)):
                print( 'tile {:2d}: {}:'.format(ii, data[ii]) )

        nodes = loadNodes( dawg, nodeSize )
        words = []
        if nodes:
            expandDAWG( nodes, nodeSize, args.DELIM, offset, data, words )
            if not len(words) == nWords:
                print("loaded {} words but header claims {}".format(len(words), nWords), file=sys.stderr)
                # assert len(words) == nWords
        if args.DUMP_WORDS:
            for word in words:
                # if we're piped to head we'll get an exception, so just exit
                try: print(word)
                except: break

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dawg', dest = 'DAWG', type = str, required = True,
                        help = 'the .xwd file to load')
    parser.add_argument('--dump-words', dest = 'DUMP_WORDS', default = False,
                        action = 'store_true', help = 'write wordlist to stdout')
    parser.add_argument('--dump-tiles', dest = 'DUMP_TILES', default = False,
                        action = 'store_true', help = 'write tile metadata to stdout')
    parser.add_argument('--dump-msg', dest = 'DUMP_MSG', default = False,
                        action = 'store_true', help = 'write header user-visible message to stdout')
    parser.add_argument('--get-sum', dest = 'GET_SUM', default = False,
                        action = 'store_true', help = 'write md5sum to stdout')
    parser.add_argument('--separator', dest = 'DELIM', default = '', help = 'printed between tiles')

    # [-raw | -json]  [-get-sum] [-get-desc] -dict <xwdORpdb>

    return parser

def parseArgs():
    args = mkParser().parse_args()
    process( args )

def main():
    args = parseArgs()

##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/python

import difflib, getopt, glob, os, sys, zipfile

def usage(msg = None):
    if msg: print 'Error:', msg
    print 'usage: ', sys.argv[0], '[-d indx]* [-a <path>]{2}  # picks newest gradle and ant builds by default'
    sys.exit(1)

def getOrderedGradleApks():
    paths = glob.glob('./app/build/outputs/apk/app-debug.apk')
    sorted(paths, key=lambda path: os.path.getmtime(path))
    # print 'getOrderedGradleApks() =>', paths
    return paths
    
def getOrderedAntApks():
    paths = glob.glob('./*.apk')
    sorted(paths, key=lambda path: os.path.getmtime(path))
    # print 'getOrderedAntApks() =>', paths
    return paths

# Check that files given exist, and if there aren't enough add more,
# trying to make them the newest gradle and ant builds.
def checkApks( apks ):
    for apk in apks:
        if not os.path.exists( apk ):
            usage( 'no such file: {path}'.format(path=apk))
            
    if 2 < len(apks):
        usage( 'too many apk files to compare' )
    elif 2 == len(apks):
        None
    else:
        gradleApks = getOrderedGradleApks()
        antApks = getOrderedAntApks()
        count = len(apks)
        for ii in range(2):
            if ii < count:
                apk = apks[ii]
                if apk in gradleApks:
                    gradleApks = []
                elif apk in antApks:
                    antApks = []
                else:
                    usage( 'something wrong' )
            elif 0 < len(gradleApks):
                apks.append( gradleApks[0] )
                gradleApks = []
            elif 0 < len(antApks):
                apks.append( antApks[0] )
                antApks = []
            else:
                usage( 'unable to find suitable default .apk' )
    return apks

def load( apks ):
    results = []
    for apk in apks:
        files = {}
        if not zipfile.is_zipfile( apk ): usage( apk + " does not appear to be an apk")
        zf = zipfile.ZipFile(apk, 'r')
        for info in zf.infolist():
            data = {'size': info.file_size, 'crc': info.CRC, }
            files[str(info.filename)] = data
        results.append( {'files' : files, 'name' : apk} )
    return results

def printDiff( apks, key ):
    data0 = zipfile.ZipFile(apks[0], 'r').read(key)
    data1 = zipfile.ZipFile(apks[1], 'r').read(key)
    print '---- diff:', key, apks[0], apks[1], len(data0), len(data1)
    # print data0
    diff = difflib.ndiff( data0.splitlines(1), data1.splitlines(1) )
    print ''.join(diff)

def compare( apks, apkData, indices ):
    print 'Comparing {one} and {two}'.format(one=apks[0], two=apks[1])
    same = 0
    index = 0
    keys = list( set( apkData[0]['files'].keys() + apkData[1]['files'].keys() ) )
    keys.sort()
    for key in keys:
        lines = []
        sizes = set()
        crcs = set()
        for apk in apkData:
            apk = apk['files']
            if key in apk:
                size = apk[key]['size']
                sizes.add(size)
                crc = apk[key]['crc']
                crcs.add( crc )
            else:
                size = 0
            lines.append( '{size: >6}b {crc:x}'.format(name=key, size=size, crc=crc) )
        if 1 == len(sizes) and 1 == len(crcs):
            same = same + 1 # same size?
        else:
            if not indices or index in indices:
                print '{index: >4}: {name: <60} '.format(index=index, name=key), ' '.join(lines)
            if index in indices:
                printDiff( apks, key )
            index += 1
    if 0 != same:
        print "(skipped {same} files)".format(same=same)

def main():
    apks = []
    indices = []
    try:
        pairs, rest = getopt.getopt(sys.argv[1:], "a:d:")
        for option, value in pairs:
            if option == '-a': apks.append(value)
            elif option == '-d': indices.append(int(value))
            else: usage()
    except:
        usage()

    apks = checkApks( apks )
    print apks[0], apks[1]
    apkData = load( apks )
    compare( apks, apkData, indices )


##############################################################################
if __name__ == '__main__':
    main()

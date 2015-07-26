#!/usr/bin/python
# Script meant to be installed on eehouse.org.

import logging, shelve, hashlib, sys, json, subprocess, glob, os, struct, random, string
import mk_for_download, mygit
import xwconfig

from stat import ST_CTIME
try:
    from mod_python import apache
    apacheAvailable = True
except ImportError:
    apacheAvailable = False

# constants that are also used in UpdateCheckReceiver.java
VERBOSE = False
k_NAME = 'name'
k_AVERS = 'avers'
k_GVERS = 'gvers'
k_INSTALLER = 'installer'
k_DEVOK = 'devOK'
k_APP = 'app'
k_DEBUG = "dbg"
k_DICTS = 'dicts'
k_XLATEINFO = 'xlatinfo'
k_CALLBACK = 'callback'
k_LOCALE = 'locale'
k_XLATPROTO = 'proto'
k_XLATEVERS = 'xlatevers'
k_STRINGSHASH = 'strings'

k_DICT_HEADER_MASK = 0x08


k_OLD = 'old'
k_NEW = 'new'
k_PAIRS = 'pairs'

k_LANG = 'lang'
k_MD5SUM = 'md5sum'
k_INDEX = 'index'
k_ISUM = 'isum'
k_SUCCESS = 'success'
k_URL = 'url'

k_SUMS = 'sums'
k_COUNT = 'count'
k_LANGS = 'langs'
k_LANGSVERS = 'lvers'

# Version for those sticking with RELEASES
k_REL_REV = 'android_beta_88'

# Version for those getting intermediate builds

k_suffix = '.xwd'
k_filebase = "/var/www/"
k_apkDir = "xw4/android/"
k_shelfFile = k_filebase + 'xw4/info_shelf_2'
k_urlbase = "http://eehouse.org"
k_versions = { 'org.eehouse.android.xw4': {
        'version' : 76,
        k_AVERS : 76,
        k_URL : k_apkDir + 'XWords4-release_' + k_REL_REV + '.apk',
        },
               }

# k_versions_dbg = { 'org.eehouse.android.xw4': {
#         'version' : 74,
#         k_AVERS : 74,
#         k_GVERS : k_DBG_REV,
#         k_URL : k_apkDir + 'XWords4-release_' + k_DBG_REV + '.apk',
#         },
#                }
s_shelf = None

g_langs = {'English' : 'en',
           'Swedish' : 'se',
           'Portuguese' : 'pt',
           'Dutch' : 'nl',
           'Danish' : 'dk',
           'Czech' : 'cz',
           'French' : 'fr',
           'German' : 'de',
           'Catalan' : 'ca',
           'Slovak' : 'sk',
           'Spanish' : 'es',
           'Polish' : 'pl',
           'Italian' : 'it',
}

logging.basicConfig(level=logging.DEBUG
        ,format='%(asctime)s [[%(levelname)s]] %(message)s'
        ,datefmt='%d %b %y %H:%M'
        ,filename='/tmp/info_py.log')
#        ,filemode='w')

# This seems to be required to prime the pump somehow.
# logging.debug( "loaded...." )

def languageCodeFor( lang ):
    result = ''
    if lang in g_langs: result = g_langs[lang]
    return result

def getInternalSum( filePath ):
    filePath = k_filebase + "and_wordlists/" + filePath
    proc = subprocess.Popen(['/usr/bin/perl', 
                             '--',
                             '/var/www/xw4/dawg2dict.pl', 
                             '-get-sum',
                             '-dict', filePath ],
                            stdout = subprocess.PIPE,
                            stderr = subprocess.PIPE)
    return proc.communicate()[0].strip()

def md5Checksums( sums, filePath ):
    if not filePath.endswith(k_suffix): filePath += k_suffix
    if filePath in sums:
        result = sums[filePath]
    else:
        logging.debug( "opening %s" % (k_filebase + "and_wordlists/" + filePath))
        try:
            file = open( k_filebase + "and_wordlists/" + filePath, 'rb' )
            md5 = hashlib.md5()
            while True:
                buffer = file.read(128)
                if not buffer:  break
                md5.update( buffer )

            sums[filePath] = [ md5.hexdigest(), 
                               getInternalSum( filePath ) ]
            logging.debug( "figured sum for %s: %s" % (filePath, 
                                                       sums[filePath] ) )
            result = sums[filePath]
        except:
            # logging.debug( "Unexpected error: " + sys.exc_info()[0] )
            result = None
    return result

def openShelf():
    global s_shelf
    # shelve will fail if permissions are wrong.  That's ok for some
    # testing: just make a fake shelf and test before saving it later.
    if not s_shelf:
        try:
            s_shelf = shelve.open(k_shelfFile)
        except:
            s_shelf = {}
        if not k_SUMS in s_shelf: s_shelf[k_SUMS] = {}
        if not k_COUNT in s_shelf: s_shelf[k_COUNT] = 0
    s_shelf[k_COUNT] += 1
    logging.debug( "Count now %d" % s_shelf[k_COUNT] )

def closeShelf():
    global s_shelf
    if 'close' in s_shelf: s_shelf.close()

def getDictSums():
    global s_shelf
    openShelf()
    return s_shelf[k_SUMS]

def getOrderedApks( path, debug ):
    # logging.debug( "getOrderedApks(" + path + ")" )
    apks = []

    pattern = path
    if debug: pattern += "/XWords4-debug-android_*.apk"
    else: pattern += "/XWords4-release_*android_beta_*.apk"

    files = ((os.stat(apk).st_mtime, apk) for apk in glob.glob(pattern))
    for mtime, file in sorted(files, reverse=True):
        # logging.debug( file + ": " + str(mtime) )
        apks.append( file )

    return apks

def getVariantDir( name ):
    result = ''
    splits = string.split( name, '.' )
    last = splits[-1]
    if not last == 'xw4': result = last + '/'
    # logging.debug( 'getVariantDir(' + name + ") => " + result )
    return result

# public, but deprecated
def curVersion( req, name, avers = 41, gvers = None, installer = None ):
    global k_versions
    result = { k_SUCCESS : True }
    if apacheAvailable:
        logging.debug( 'IP address of requester is %s' 
                       % req.get_remote_host(apache.REMOTE_NAME) )

    logging.debug( "name: %s; avers: %s; installer: %s; gvers: %s"
                   % (name, avers, installer, gvers) )
    if name in k_versions:
        versions = k_versions[name]
        if versions[k_AVERS] > int(avers):
            logging.debug( avers + " is old" )
            result[k_URL] = k_urlbase + '/' + versions[k_URL]
        else:
            logging.debug(name + " is up-to-date")
    else:
        logging.debug( 'Error: bad name ' + name )
    return json.dumps( result )

# public, but deprecated
def dictVersion( req, name, lang, md5sum ):
    result = { k_SUCCESS : True }
    if not name.endswith(k_suffix): name += k_suffix
    dictSums = getDictSums()
    path = lang + "/" + name
    if not path in dictSums:
        sums = md5Checksums( dictSums, path )
        if sums:
            dictSums[path] = sums
            s_shelf[k_SUMS] = dictSums
    if path in dictSums:
        if not md5sum in dictSums[path]:
            result[k_URL] = k_urlbase + "/and_wordlists/" + path
    else:
        logging.debug( path + " not known" )
    closeShelf()
    return json.dumps( result )

def getApp( params, name ):
    result = None
    if k_NAME in params:
        name = params[k_NAME]
    if name:
        variantDir = getVariantDir( name )
        # If we're a dev device, always push the latest
        if k_DEBUG in params and params[k_DEBUG]:
            dir = k_filebase + k_apkDir + variantDir
            apks = getOrderedApks( dir, True )
            if 0 < len(apks):
                url = k_urlbase + '/' + k_apkDir + variantDir + apks[0][len(dir):]
                result = {k_URL: url}
        elif k_DEVOK in params and params[k_DEVOK]:
            apks = getOrderedApks( k_filebase + k_apkDir, False )
            if 0 < len(apks):
                apk = apks[0]
                # Does path NOT contain name of installed file
                curApk = params[k_GVERS] + '.apk'
                if curApk in apk:
                    logging.debug( "already have " + curApk )
                else:
                    url = k_urlbase + '/' + apk[len(k_filebase):]
                    result = {k_URL: url}
                    logging.debug( result )
                    
        elif k_AVERS in params and k_GVERS in params:
            avers = params[k_AVERS]
            gvers = params[k_GVERS]
            if k_INSTALLER in params: installer = params[k_INSTALLER]
            else: installer = ''

            logging.debug( "name: %s; avers: %s; installer: %s; gvers: %s"
                           % (name, avers, installer, gvers) )
            if name in k_versions:
                versForName = k_versions[name]
                if versForName[k_AVERS] > int(avers):
                    result = {k_URL: k_urlbase + '/' + versForName[k_URL]}
                elif k_GVERS in versForName and not gvers == versForName[k_GVERS]:
                    result = {k_URL: k_urlbase + '/' + versForName[k_URL]}
                else:
                    logging.debug(name + " is up-to-date")
        else:
            logging.debug( 'Error: bad name ' + name )
    else:
        logging.debug( 'missing param' )
    return result

def getStats( path ):
    nBytes = int(os.stat( path ).st_size)

    nWords = -1
    note = md5sum = None
    with open(path, "rb") as f:
        flags = struct.unpack('>h', f.read(2))[0]
        hasHeader = not 0 == (flags & k_DICT_HEADER_MASK)
        if hasHeader:
            headerLen = struct.unpack('>h', f.read(2))[0]
            nWords = struct.unpack('>i', f.read(4))[0]
            headerLen -= 4
            if 0 < headerLen:
                rest = f.read(headerLen)
                for ii in range(len(rest)):
                    if '\0' == rest[ii]:
                        if not note:
                            note = rest[:ii]
                            start = ii + 1
                        elif not md5sum:
                            md5sum = rest[start:ii]

        f.close()

    result = { 'nWords' : nWords, 'nBytes' : nBytes }
    if note: result['note'] = note
    if md5sum: result['md5sum'] = md5sum
    return result

# create obj containing array of objects each with 'lang' and 'xwds',
# the latter an array of objects giving info about a dict.
def listDicts( lc = None ):
    global s_shelf
    langsVers = 2
    # langsVers = random.random()            # change this to force recalc of shelf langs data
    ldict = {}
    root = k_filebase + "and_wordlists/"
    openShelf()
    if not k_LANGS in s_shelf or not k_LANGSVERS in s_shelf \
       or s_shelf[k_LANGSVERS] != langsVers:
        dictSums = getDictSums()
        for path in glob.iglob( root + "*/*.xwd" ):
            entry = getStats( path )
            path = path.replace( root, '' )
            lang, xwd = path.split( '/' )
            entry.update( { 'xwd' : xwd,
                            'md5sums' : md5Checksums( dictSums, path ),
                        } )
            if not lang in ldict: ldict[lang] = []
            ldict[lang].append( entry )

        # now format as we want 'em
        langs = []
        for lang, entry in ldict.iteritems():
            obj = { 'lang' : lang,
                    'lc' : languageCodeFor(lang),
                    'dicts' : entry,
                }
            langs.append( obj )
        s_shelf[k_LANGS] = langs
        s_shelf[k_LANGSVERS] = langsVers

    result = { 'langs' : s_shelf[k_LANGS] }
    closeShelf();

    print "looking for", lc
    if lc:
        result['langs'] = [elem for elem in result['langs'] if elem['lc'] == lc]

    return result

def getDicts( params ):
    result = []
    dictSums = getDictSums()
    for param in params:
        name = param[k_NAME]
        lang = param[k_LANG]
        md5sum = param[k_MD5SUM]
        index = param[k_INDEX]
        if not name.endswith(k_suffix): name += k_suffix
        path = lang + "/" + name
        if not path in dictSums:
            sums = md5Checksums( dictSums, path )
            if sums:
                dictSums[path] = sums
                s_shelf[k_SUMS] = dictSums
        if path in dictSums:
            if not md5sum in dictSums[path]:
                cur = { k_URL : k_urlbase + "/and_wordlists/" + path,
                        k_INDEX : index, k_ISUM: dictSums[path][1] }
                result.append( cur )
        else:
            logging.debug( path + " not known" )

    closeShelf()
    if 0 == len(result): result = None
    return result

def variantFor( name ):
    if name == 'xw4': result = 'XWords4'
    logging.debug( 'variantFor(%s)=>%s' % (name, result))
    return result

def getXlate( params, name, stringsHash ):
    result = []
    path = xwconfig.k_REPOPATH
    logging.debug('creating repo with path ' + path)
    repo = mygit.GitRepo( path )
    logging.debug( "getXlate: %s, hash=%s" % (json.dumps(params), stringsHash) )
    # logging.debug( 'status: ' + repo.status() )

    # reduce org.eehouse.anroid.xxx to xxx, then turn it into a
    # variant and get the contents of the R.java file
    splits = name.split('.')
    name = splits[-1]
    variant = variantFor( name );
    rPath = '%s/archive/R.java' % variant
    rDotJava = repo.cat( rPath, stringsHash )

    # Figure out the newest hash possible for translated strings.xml
    # files.  If our R.java's the newest, that's HEAD.  Otherwise it's
    # the revision BEFORE the revision that changed R.java

    head = repo.getHeadRev()
    logging.debug('head = %s' % head)
    rjavarevs = repo.getRevsBetween(head, stringsHash, rPath)
    if rjavarevs:
        assert( 1 >= len(rjavarevs) )
        assert( stringsHash == rjavarevs[-1] )
        if 1 == len(rjavarevs): 
            firstPossible = head
        else: 
            firstPossible = rjavarevs[-2] + '^'
            # get actual number for rev^
            firstPossible = repo.getRevsBetween( firstPossible, firstPossible )[0]
        logging.debug('firstPossible: %s' % firstPossible)

        for entry in params:
            curVers = entry[k_XLATEVERS]
            if not curVers == firstPossible: 
                locale = entry[k_LOCALE]

                data = mk_for_download.getXlationFor( repo, rDotJava, locale, \
                                                          firstPossible )
                if data: result.append( { k_LOCALE: locale,
                                          k_OLD: curVers,
                                          k_NEW: firstPossible,
                                          k_PAIRS: data,
                                          } )

    if 0 == len(result): result = None
    logging.debug( "getXlate=>%s" % (json.dumps(result)) )
    return result

# public
def getUpdates( req, params ):
    result = { k_SUCCESS : True }
    appResult = None
    logging.debug( "getUpdates: got params: %s" % params )
    asJson = json.loads( params )
    if k_APP in asJson:
        name = None
        if k_NAME in asJson: name = asJson[k_NAME]
        appResult = getApp( asJson[k_APP], name )
        if appResult: 
            result[k_APP] = appResult
    if k_DICTS in asJson:
        dictsResult = getDicts( asJson[k_DICTS] )
        if dictsResult:
            result[k_DICTS] = dictsResult

    # Let's not upgrade strings at the same time as we're upgrading the app
    if appResult:
        logging.debug( 'skipping xlation upgrade because app being updated' )
    elif k_XLATEINFO in asJson and k_NAME in asJson and k_STRINGSHASH in asJson:
        xlateResult = getXlate( asJson[k_XLATEINFO], asJson[k_NAME], asJson[k_STRINGSHASH] )
        if xlateResult: 
            logging.debug( xlateResult )
            result[k_XLATEINFO] = xlateResult;
    else:
        logging.debug( "NOT FOUND xlate info" )
        
    result = json.dumps( result )
    # logging.debug( result )
    return result

def clearShelf():
    shelf = shelve.open(k_shelfFile)
    for key in shelf: del shelf[key]
    shelf.close()

def usage():
    print "usage:", sys.argv[0], '--get-sums [lang/dict]*'
    print '                    | --test-get-app app <org.eehouse.app.name> avers gvers'
    print '                    | --test-get-dicts name lang curSum'
    print '                    | --list-apks [path/to/apks]'
    print '                    | --list-dicts'
    print '                    | --clear-shelf'
    sys.exit(-1)

def main():
    if 1 >= len(sys.argv): usage();
    arg = sys.argv[1]
    if arg == '--clear-shelf':
        clearShelf()
    elif arg == '--list-dicts':
        if 2 < len(sys.argv): lc = sys.argv[2]
        else: lc = None
        dictsJson = listDicts( lc )
        print json.dumps( dictsJson )
    elif arg == '--get-sums':
        dictSums = getDictSums()
        for arg in sys.argv[2:]:
            print arg, md5Checksums(dictSums, arg)
        s_shelf[k_SUMS] = dictSums
        closeShelf()
    elif arg == '--test-get-app':
        if not 5 == len(sys.argv): usage()
        params = { k_NAME: sys.argv[2], 
                   k_AVERS: int(sys.argv[3]),
                   k_GVERS: sys.argv[4],
                   }
        print getApp( params )
    elif arg == '--test-get-dicts':
        if not 5 == len(sys.argv): usage()
        params = { k_NAME: sys.argv[2], 
                   k_LANG : sys.argv[3], 
                   k_MD5SUM : sys.argv[4], 
                   k_INDEX : 0,
                   }
        print getDicts( [params] )
    elif arg == '--list-apks':
        argc = len(sys.argv)
        if argc >= 4: usage()
        path = ""
        if argc >= 3: path = sys.argv[2]
        apks = getOrderedApks( path, False )
        if 0 == len(apks): print "No apks in", path
        for apk in apks:
            print apk
    else:
        usage()

##############################################################################
if __name__ == '__main__':
    main()

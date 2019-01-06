#!/usr/bin/python
# Script meant to be installed on eehouse.org.

import shelve, hashlib, sys, re, json, subprocess, glob, os
import struct, random, string, psycopg2, zipfile
import mk_for_download, mygit
import xwconfig

# I'm not checking my key in...
try :
    import mykey
except:
    print('unable to load mykey')

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
k_REL_REV = 'android_beta_98'

# newer build-info.txt file contain lines like this:
# git: android_beta_123
pat_git_tag = re.compile( 'git: (\S*)', re.DOTALL | re.MULTILINE )

# Version for those getting intermediate builds

k_suffix = '.xwd'
k_filebase = "/var/www/html/"
k_apkDir = "xw4/android/"
k_shelfFile = k_filebase + 'xw4/info_shelf_2'
k_urlbase = "http://eehouse.org"

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

def languageCodeFor( lang ):
    result = ''
    if lang in g_langs: result = g_langs[lang]
    return result

def getInternalSum( filePath ):
    filePath = k_filebase + "and_wordlists/" + filePath
    proc = subprocess.Popen(['/usr/bin/perl', 
                             '--',
                             k_filebase + 'xw4/dawg2dict.pl', 
                             '-get-sum',
                             '-dict', filePath ],
                            stdout = subprocess.PIPE,
                            stderr = subprocess.PIPE)
    results = proc.communicate()
    # apache.log_error(filePath + ': ' + results[1].strip())
    return results[0].strip()

def md5Checksums( sums, filePath ):
    if not filePath.endswith(k_suffix): filePath += k_suffix
    if filePath in sums:
        result = sums[filePath]
    else:
        # logging.debug( "opening %s" % (k_filebase + "and_wordlists/" + filePath))
        try:
            file = open( k_filebase + "and_wordlists/" + filePath, 'rb' )
            md5 = hashlib.md5()
            while True:
                buffer = file.read(128)
                if not buffer:  break
                md5.update( buffer )

            sums[filePath] = [ md5.hexdigest(), 
                               getInternalSum( filePath ) ]
            apache.log_error( "figured sum for %s: %s" % (filePath, 
                                                       sums[filePath] ) )
            result = sums[filePath]
        except:
            # apache.log_error( "Unexpected error: " + sys.exc_info()[0] )
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
    # apache.log_error( "Count now %d" % s_shelf[k_COUNT] )

def closeShelf():
    global s_shelf
    if 'close' in s_shelf: s_shelf.close()

def getDictSums():
    global s_shelf
    openShelf()
    return s_shelf[k_SUMS]

def getGitRevFor(file, repo):
    result = None
    zip = zipfile.ZipFile(file);

    try:
        result = zip.read('assets/gitvers.txt').split("\n")[0]
    except KeyError, err:
        result = None

    if not result:
        try:
            data = zip.read('assets/build-info.txt')
            match = pat_git_tag.match(data)
            if match:
                tag = match.group(1)
                if not 'dirty' in tag:
                    result = repo.tagToRev(tag)
        except KeyError, err:
            None

    # print "getGitRevFor(", file, "->", result
    return result


pat_badge_info = re.compile("package: name='([^']*)' versionCode='([^']*)' versionName='([^']*)'", re.DOTALL )

def getAAPTInfo(file):
    result = None
    test = subprocess.Popen(["aapt", "dump", "badging", file], shell = False, stdout = subprocess.PIPE)
    for line in test.communicate():
        if line:
            match = pat_badge_info.match(line)
            if match:
                result = { 'appID' : match.group(1),
                           'versionCode' : int(match.group(2)),
                           'versionName' : match.group(3),
                           }
                break
    return result

def getOrderedApks( path, appID, debug ):
    apkToCode = {}
    apkToMtime = {}
    if debug: pattern = path + "/*debug*.apk"
    else: pattern = path + "/*release*.apk"
    files = ((os.stat(apk).st_mtime, apk) for apk in glob.glob(pattern))
    for mtime, file in sorted(files, reverse=True):
        info = getAAPTInfo(file)
        if info['appID'] == appID:
            apkToCode[file] = info['versionCode']
            apkToMtime[file] = mtime
    result = sorted(apkToCode.keys(), reverse=True, key=lambda file: (apkToCode[file], apkToMtime[file]))
    return result

# Given a version, find the apk that has the next highest version
def getNextAfter(path, appID, curVers, debug):
    # print 'getNextAfter(', path, ')'
    apks = getOrderedApks(path, appID, debug)

    map = {}
    max = 0
    for apk in apks:
        versionCode = getAAPTInfo(apk)['versionCode']
        if versionCode > curVers:
            map[versionCode] = apk
            if max < versionCode: max = versionCode

    # print map

    result = None
    if map:
        print 'looking between', curVers+1, 'and', max
        for nextVersion in range(curVers+1, max+1):
            if nextVersion in map:
                result = map[nextVersion]
                break

    if result:
        print nextVersion, ':', result
    return result

# Returns '' for xw4, <variant> for anything else
def getVariantDir( name ):
    result = ''
    splits = string.split( name, '.' )
    last = splits[-1]
    if not last == 'xw4': result = last + '/'
    # apache.log_error( 'getVariantDir(' + name + ") => " + result )
    return result

# public, but deprecated
def curVersion( req, name, avers = 41, gvers = None, installer = None ):
    global k_versions
    result = { k_SUCCESS : True }
    if apacheAvailable:
        apache.log_error( 'IP address of requester is %s'
                       % req.get_remote_host(apache.REMOTE_NAME) )

    apache.log_error( "name: %s; avers: %s; installer: %s; gvers: %s"
                   % (name, avers, installer, gvers) )
    if name in k_versions:
        versions = k_versions[name]
        if versions[k_AVERS] > int(avers):
            apache.log_error( avers + " is old" )
            result[k_URL] = k_urlbase + '/' + versions[k_URL]
        else:
            apache.log_error(name + " is up-to-date")
    else:
        apache.log_error( 'Error: bad name ' + name )
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
        apache.log_error( path + " not known" )
    closeShelf()
    return json.dumps( result )

def getApp( params, name = None, debug = False):
    result = None
    if k_DEBUG in params: debug = params[k_DEBUG]
    if k_NAME in params: name = params[k_NAME]
    if name:
        variantDir = getVariantDir( name )
        # If we're a dev device, always push the latest
        if k_DEBUG in params and params[k_DEBUG]:
            dir = k_filebase + k_apkDir + variantDir
            apks = getOrderedApks( dir, name, True )
            if 0 < len(apks):
                apk = apks[0]
                curApk = params[k_GVERS] + '.apk'
                if curApk in apk:
                    apache.log_error( "already have " + curApk )
                else:
                    url = k_urlbase + '/' + k_apkDir + variantDir + apk[len(dir):]
                    apache.log_error("url: " + url)
                    result = {k_URL: url}
        elif k_DEVOK in params and params[k_DEVOK]:
            apks = getOrderedApks( k_filebase + k_apkDir, name, False )
            if 0 < len(apks):
                apk = apks[0]
                # Does path NOT contain name of installed file
                curApk = params[k_GVERS] + '.apk'
                if curApk in apk:
                    apache.log_error( "already have " + curApk )
                else:
                    url = k_urlbase + '/' + apk[len(k_filebase):]
                    result = {k_URL: url}
                    apache.log_error( result )
                    
        elif k_AVERS in params:
            vers = params[k_AVERS]
            if k_INSTALLER in params: installer = params[k_INSTALLER]
            else: installer = ''

            apache.log_error( "name: %s; installer: %s; gvers: %s"
                           % (name, installer, vers) )
            print "name: %s; installer: %s; vers: %s" % (name, installer, vers)
            dir = k_filebase + k_apkDir + 'rel/'
            apk = getNextAfter( dir, name, vers, debug )
            if apk:
                apk = apk[len(k_filebase):] # strip fs path
                result = {k_URL: k_urlbase + '/' + apk}
            else:
                apache.log_error(name + " is up-to-date")
        else:
            apache.log_error( 'Error: bad name ' + name )
    else:
        apache.log_error( 'missing param' )
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
            apache.log_error( path + " not known" )

    closeShelf()
    if 0 == len(result): result = None
    return result

def variantFor( name ):
    if name == 'xw4': result = 'XWords4'
    apache.log_error( 'variantFor(%s)=>%s' % (name, result))
    return result

def getXlate( params, name, stringsHash ):
    result = []
    path = xwconfig.k_REPOPATH
    apache.log_error('creating repo with path ' + path)
    repo = mygit.GitRepo( path )
    apache.log_error( "getXlate: %s, hash=%s" % (json.dumps(params), stringsHash) )
    # apache.log_error( 'status: ' + repo.status() )

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
    apache.log_error('head = %s' % head)
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
        apache.log_error('firstPossible: %s' % firstPossible)

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
    apache.log_error( "getXlate=>%s" % (json.dumps(result)) )
    return result

def init():
    try:
        con = psycopg2.connect(port=mykey.psqlPort, database='xwgames', user='relay',
                               password=mykey.relayPwd, host='localhost')
    except psycopg2.DatabaseError, e:
        print 'Error %s' % e 
        sys.exit(1)
    return con

# public

# Give a list of relayIDs, e.g. eehouse.org:56022505:64/2, assumed to
# represent the caller's position in games, return for each a list of
# relayIDs representing the other devices in the game.
def opponentIDsFor( req, params ):
    # build array of connnames by taking the part before the slash
    params = json.loads( params )
    relayIDs = params['relayIDs']
    me = int(params['me'])
    connnames = {}
    for relayID in relayIDs:
        (connname, index) = string.split(relayID, '/')
        if connname in connnames:
            connnames[connname].append(int(index))
        else:
            connnames[connname] = [int(index)]

    query = "SELECT connname, devids FROM games WHERE connname in ('%s')" % \
            string.join(connnames.keys(), '\',\'')
    con = init()
    cur = con.cursor()
    cur.execute(query)
    results = []
    for row in cur:
        connname = row[0]
        indices = connnames[connname]
        for index in indices:
            devids = []
            for devid in row[1]:
                if not devid == me:
                    devids.append(str(devid))
            if 0 < len(devids):
                results.append({"%s/%d" % (connname, index) : devids})
        
    result = { k_SUCCESS : True,
               'devIDs' : results,
               'me' : me,
    }
    return result

def getUpdates( req, params ):
    result = { k_SUCCESS : True }
    appResult = None
    apache.log_error( "getUpdates: got params: %s" % params )
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
    # if appResult:
    #     apache.log_error( 'skipping xlation upgrade because app being updated' )
    # elif k_XLATEINFO in asJson and k_NAME in asJson and k_STRINGSHASH in asJson:
    #     xlateResult = getXlate( asJson[k_XLATEINFO], asJson[k_NAME], asJson[k_STRINGSHASH] )
    #     if xlateResult:
    #         apache.log_error( xlateResult )
    #         result[k_XLATEINFO] = xlateResult;
        
    result = json.dumps( result )
    # apache.log_error( result )
    return result

def clearShelf():
    shelf = shelve.open(k_shelfFile)
    for key in shelf: del shelf[key]
    shelf.close()

def usage(msg=None):
    if msg: print "ERROR:", msg
    print "usage:", sys.argv[0], '--get-sums [lang/dict]*'
    print '                    | --get-app --appID <org.something> --vers <avers> --gvers <gvers> [--debug]'
    print '                    | --test-get-dicts name lang curSum'
    print '                    | --list-apks [--path <path/to/apks>] [--debug] --appID org.something'
    print '                    | --list-dicts'
    print '                    | --opponent-ids-for'
    print '                    | --clear-shelf'
    sys.exit(-1)

def main():
    argc = len(sys.argv)
    if 1 >= argc: usage('too few args')
    arg = sys.argv[1]
    args = sys.argv[2:]
    if arg == '--clear-shelf':
        clearShelf()
    elif arg == '--list-dicts':
        if 2 < argc: lc = sys.argv[2]
        else: lc = None
        dictsJson = listDicts( lc )
        print json.dumps( dictsJson )
    elif arg == '--get-sums':
        dictSums = getDictSums()
        for arg in sys.argv[2:]:
            print arg, md5Checksums(dictSums, arg)
        s_shelf[k_SUMS] = dictSums
        closeShelf()
    elif arg == '--get-app':
        appID = None
        vers = 0
        debug = False
        while len(args):
            arg = args.pop(0)
            if arg == '--appID': appID = args.pop(0)
            elif arg == '--vers': vers = int(args.pop(0))
            elif arg == '--debug': debug = True
            else: usage('unexpected arg: ' + arg)
        if not appID: usage('--appID required')
        elif not vers: usage('--vers required')
        params = { k_NAME: appID,
                   k_AVERS: vers,
                   k_DEBUG: debug,
                   k_DEVOK: False, # FIX ME
                   }
        print getApp( params )
    elif arg == '--test-get-dicts':
        if not 5 == argc: usage()
        params = { k_NAME: sys.argv[2], 
                   k_LANG : sys.argv[3], 
                   k_MD5SUM : sys.argv[4], 
                   k_INDEX : 0,
                   }
        print getDicts( [params] )
    elif arg == '--list-apks':
        path = ""
        debug = False
        appID = ''
        while len(args):
            arg = args.pop(0)
            if arg == '--appID': appID = args.pop(0)
            elif arg == '--debug': debug = True
            elif arg == '--path': path = args.pop(0)
        if not appID: usage('--appID not optional')
        apks = getOrderedApks( path, appID, debug )
        if not len(apks): print "No apks in", path
        else: print
        for apk in apks:
            print apk
    elif arg == '--opponent-ids-for':
        ids = ['eehouse.org:55f90207:7/1',
               'eehouse.org:55f90207:7/2',
               'eehouse.org:56022505:5/2',
               'eehouse.org:56022505:6/1',
               'eehouse.org:56022505:10/1',
               'eehouse.org:56022505:64/2',
               'eehouse.org:56022505:64/1',
        ]
        params = {'relayIDs' : ids, 'me' : '80713149'}
        result = opponentIDsFor(None, json.dumps(params))
        print json.dumps(result)
    else:
        usage()

##############################################################################
if __name__ == '__main__':
    main()

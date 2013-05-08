#!/usr/bin/python
# Script meant to be installed on eehouse.org.

import logging, shelve, hashlib, sys, json, subprocess
try:
    from mod_python import apache
    apacheAvailable = True
except ImportError:
    apacheAvailable = False

# constants that are also used in UpdateCheckReceiver.java
k_NAME = 'name'
k_AVERS = 'avers'
k_GVERS = 'gvers'
k_INSTALLER = 'installer'
k_DEVOK = 'devOK'
k_APP = 'app'
k_DICTS = 'dicts'
k_LANG = 'lang'
k_MD5SUM = 'md5sum'
k_INDEX = 'index'
k_ISUM = 'isum'
k_SUCCESS = 'success'
k_URL = 'url'

k_SUMS = 'sums'
k_COUNT = 'count'

# Version for those sticking with RELEASES
k_REL_REV = 'android_beta_60'

# Version for those getting intermediate builds
k_DBG_REV = 'android_beta_58-33-ga18fb62'
k_DBG_REV = 'android_beta_59-24-gc31a1d9'

k_suffix = '.xwd'
k_filebase = "/var/www/"
k_shelfFile = k_filebase + 'xw4/info_shelf_2'
k_urlbase = "http://eehouse.org/"
k_versions = { 'org.eehouse.android.xw4': {
        'version' : 52,
        k_AVERS : 52,
        k_URL : 'xw4/android/XWords4-release_' + k_REL_REV + '.apk',
        },
               }

k_versions_dbg = { 'org.eehouse.android.xw4': {
        'version' : 52,
        k_AVERS : 52,
        k_GVERS : k_DBG_REV,
        k_URL : 'xw4/android/XWords4-release_' + k_DBG_REV + '.apk',
        },
               }
s_shelf = None


logging.basicConfig(level=logging.DEBUG
        ,format='%(asctime)s [[%(levelname)s]] %(message)s'
        ,datefmt='%d %b %y %H:%M'
        ,filename='/tmp/info_py.log')
#        ,filemode='w')

# This seems to be required to prime the pump somehow.
# logging.debug( "loaded...." )

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

def getDictSums():
    global s_shelf
    # shelve will fail if permissions are wrong.  That's ok for some
    # testing: just make a fake shelf and test before saving it later.
    try:
        s_shelf = shelve.open(k_shelfFile)
    except:
        s_shelf = {}

    if not k_SUMS in s_shelf: s_shelf[k_SUMS] = {}
    if not k_COUNT in s_shelf: s_shelf[k_COUNT] = 0
    s_shelf[k_COUNT] += 1
    logging.debug( "Count now %d" % s_shelf[k_COUNT] )
    return s_shelf[k_SUMS]

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
            result[k_URL] = k_urlbase + versions[k_URL]
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
            result[k_URL] = k_urlbase + "and_wordlists/" + path
    else:
        logging.debug( path + " not known" )
    if 'close' in s_shelf: s_shelf.close()
    return json.dumps( result )

def getApp( params ):
    global k_versions, k_versions_dbg
    result = None
    if k_NAME in params:
        name = params[k_NAME]
        if k_AVERS in params and k_GVERS in params:
            avers = params[k_AVERS]
            gvers = params[k_GVERS]
            if k_INSTALLER in params: installer = params[k_INSTALLER]
            else: installer = ''

            logging.debug( "name: %s; avers: %s; installer: %s; gvers: %s"
                           % (name, avers, installer, gvers) )
            if k_DEVOK in params and params[k_DEVOK]: versions = k_versions_dbg
            else: versions = k_versions
            if name in versions:
                versForName = versions[name]
                if versForName[k_AVERS] > int(avers):
                    result = {k_URL: k_urlbase + versForName[k_URL]}
                elif k_GVERS in versForName and not gvers == versForName[k_GVERS]:
                    result = {k_URL: k_urlbase + versForName[k_URL]}
                else:
                    logging.debug(name + " is up-to-date")
        else:
            logging.debug( 'Error: bad name ' + name )
    else:
        logging.debug( 'missing param' )
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
                cur = { k_URL : k_urlbase + "and_wordlists/" + path,
                        k_INDEX : index, k_ISUM: dictSums[path][1] }
                result.append( cur )
        else:
            logging.debug( path + " not known" )

    if 'close' in s_shelf: s_shelf.close()
    if 0 == len(result): result = None
    return result

# public
def getUpdates( req, params ):
    result = { k_SUCCESS : True }
    logging.debug( "getUpdates: got params: %s" % params )
    asJson = json.loads( params )
    if k_APP in asJson:
        appResult = getApp( asJson[k_APP] )
        if appResult: result[k_APP] = appResult
    if k_DICTS in asJson:
        dictsResult = getDicts( asJson[k_DICTS] )
        if dictsResult: result[k_DICTS] = dictsResult
    return json.dumps( result )

def clearShelf():
    shelf = shelve.open(k_shelfFile)
    shelf[k_SUMS] = {}
    shelf.close()

def usage():
    print "usage:", sys.argv[0], '--get-sums [lang/dict]*'
    print '                    | --test-get-app app <org.eehouse.app.name> avers gvers'
    print '                    | --test-get-dicts name lang curSum'
    print '                    | --clear-shelf'
    sys.exit(-1)

def main():
    if 1 >= len(sys.argv): usage();
    arg = sys.argv[1]
    if arg == '--clear-shelf':
        clearShelf()
    elif arg == '--get-sums':
        dictSums = getDictSums()
        for arg in sys.argv[2:]:
            print arg, md5Checksums(dictSums, arg)
        s_shelf[k_SUMS] = dictSums
        if 'close' in s_shelf: s_shelf.close()
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
    else:
        usage()

##############################################################################
if __name__ == '__main__':
    main()

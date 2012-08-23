# Script meant to be installed on eehouse.org.

import logging, shelve, hashlib, sys, json
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
k_SUCCESS = 'success'
k_URL = 'url'

k_SUMS = 'sums'
k_COUNT = 'count'

k_suffix = '.xwd'
k_filebase = "/var/www/"
k_shelfFile = k_filebase + 'xw4/info_shelf'
k_urlbase = "http://eehouse.org/"
k_versions = { 'org.eehouse.android.xw4': {
        'version' : 43,
        k_AVERS : 43,
        k_URL : 'xw4/android/XWords4-release_android_beta_51.apk',
        },
               'org.eehouse.android.xw4sms' : {
        'version' : 43,
        k_AVERS : 43,
        k_URL : 'xw4/android/sms/XWords4-release_android_beta_51.apk',
        },
               }

k_versions_dbg = { 'org.eehouse.android.xw4': {
        'version' : 43,
        k_AVERS : 43,
        k_GVERS : 'android_beta_51',
        k_URL : 'xw4/android/XWords4-release_android_beta_51.apk',
        },
               'org.eehouse.android.xw4sms' : {
        'version' : 43,
        k_AVERS : 43,
        k_GVERS : 'android_beta_51',
        k_URL : 'xw4/android/sms/XWords4-release_android_beta_51.apk',
        },
               }
s_shelf = None


logging.basicConfig(level=logging.DEBUG
        ,format='%(asctime)s [[%(levelname)s]] %(message)s'
        ,datefmt='%d %b %y %H:%M'
        ,filename='/tmp/info_py.log')
#        ,filemode='w')

# This seems to be required to prime the pump somehow.
logging.debug( "loaded...." )

def md5Checksum(sums, filePath):
    if not filePath.endswith(k_suffix): filePath += k_suffix
    if not filePath in sums:
        file = open( k_filebase + "and_wordlists/" + filePath, 'rb' )
        md5 = hashlib.md5()
        while True:
            buffer = file.read(128)
            if not buffer:  break
            md5.update( buffer )
        sums[filePath] = md5.hexdigest()
        logging.debug( "figured sum for %s" % filePath )
    return sums[filePath]

def getDictSums():
    global s_shelf
    s_shelf = shelve.open(k_shelfFile)
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
        sum = md5Checksum( dictSums, path )
        if sum:
            dictSums[path] = sum
            s_shelf[k_SUMS] = dictSums
    if path in dictSums:
        if dictSums[path] != md5sum:
            result[k_URL] = k_urlbase + "and_wordlists/" + path
    else:
        logging.debug( path + " not known" )
    s_shelf.close()
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
            sum = md5Checksum( dictSums, path )
            if sum:
                dictSums[path] = sum
                s_shelf[k_SUMS] = dictSums
        if path in dictSums:
            if dictSums[path] != md5sum:
                cur = { k_URL : k_urlbase + "and_wordlists/" + path,
                        k_INDEX : index }
                result.append( cur )
        else:
            logging.debug( path + " not known" )

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
    print "                    | --test-get-app app <org.eehouse.app.name> avers gvers"
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
            print arg, md5Checksum(dictSums, arg)
        s_shelf[k_SUMS] = dictSums
        s_shelf.close()
    elif arg == '--test-get-app':
        if not 5 == len(sys.argv): usage()
        params = { k_NAME: sys.argv[2], 
                   k_AVERS: int(sys.argv[3]),
                   k_GVERS: sys.argv[4],
                   }
        print getApp( params )
    else:
        usage()

##############################################################################
if __name__ == '__main__':
    main()

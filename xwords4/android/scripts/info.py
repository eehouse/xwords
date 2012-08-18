# Script meant to be installed on eehouse.org.

import logging, shelve, hashlib, sys, json
from mod_python import apache

k_suffix = '.xwd'
k_filebase = "/var/www/"
k_shelfFile = k_filebase + "xw4/info_shelf"
k_urlbase = "http://eehouse.org/"
k_versions = { 'org.eehouse.android.xw4': {
        'version' : 42,
        'url' : 'xw4/android/XWords4-release_android_beta_49.apk'
        }
               ,'org.eehouse.android.xw4sms' : {
        'version' : 41,
        'url' : 'xw4/android/sms/XWords4-release_android_beta_49-3-g8b6af3f.apk'
        }
               }

s_shelf = None


logging.basicConfig(level=logging.DEBUG
        ,format='%(asctime)s [[%(levelname)s]] %(message)s'
        ,datefmt='%d %b %y %H:%M'
        ,filename='/tmp/info_py.log')
#        ,filemode='w')

# This seems to be required to prime the pump somehow.
# logging.debug( "loaded...." )

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
    if not 'sums' in s_shelf: s_shelf['sums'] = {}
    if not 'count' in s_shelf: s_shelf['count'] = 0
    s_shelf['count'] += 1
    logging.debug( "Count now %d" % s_shelf['count'] )
    return s_shelf['sums']

# public
def curVersion( req, name, avers, gvers, installer ):
    global k_versions
    result = { 'success' : True }
    logging.debug('IP address of requester is %s' % req.get_remote_host(apache.REMOTE_NAME))
    logging.debug( "name: %s; avers: %s; installer: %s; gvers: %s"
                   % (name, avers, installer, gvers) )
    if name in k_versions:
        if k_versions[name]['version'] > int(avers):
            logging.debug( name + " is old" )
            result['url'] = k_urlbase + k_versions[name]['url']
        else:
            logging.debug(name + " is up-to-date")
    else:
        logging.debug( 'Error: bad name ' + name )
    return json.dumps( result )

# public
def dictVersion( req, name, lang, md5sum ):
    result = { 'success' : True }
    if not name.endswith(k_suffix): name += k_suffix
    dictSums = getDictSums()
    path = lang + "/" + name
    if not path in dictSums:
        sum = md5Checksum( dictSums, path )
        if sum:
            dictSums[path] = sum
            s_shelf['sums'] = dictSums
    if path in dictSums:
        if dictSums[path] != md5sum:
            result['url'] = k_urlbase + "and_wordlists/" + path
    else:
        logging.debug( path + " not known" )
    s_shelf.close()
    return json.dumps( result )

def getApp( params ):
    global k_versions
    result = None
    if 'avers' in params and 'name' in params and 'gvers' in params:
        avers = params['avers']
        name = params['name']
        gvers = params['gvers']
        if 'installer' in params: installer = params['installer']
        else: installer = ''
        logging.debug( "name: %s; avers: %s; installer: %s; gvers: %s"
                       % (name, avers, installer, gvers) )
        if name in k_versions:
            if True or k_versions[name]['version'] > int(avers):
                result = {'url': k_urlbase + k_versions[name]['url']}
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
        name = param['name']
        lang = param['lang']
        md5sum = param['md5sum']
        index = param['index']
        if not name.endswith(k_suffix): name += k_suffix
        path = lang + "/" + name
        if not path in dictSums:
            sum = md5Checksum( dictSums, path )
            if sum:
                dictSums[path] = sum
                s_shelf['sums'] = dictSums
        if path in dictSums:
            if True or dictSums[path] != md5sum:
                cur = { 'url' : k_urlbase + "and_wordlists/" + path,
                        'index' : index }
                result.append( cur )
        else:
            logging.debug( path + " not known" )

    if 0 == len(result): result = None
    return result

# public
def getUpdates( req, params ):
    result = { 'success' : True }
    logging.debug( "getUpdates: got params: %s" % params )
    asJson = json.loads( params )
    if 'app' in asJson:
        appResult = getApp( asJson['app'] )
        if appResult: result['app'] = appResult
    if 'dicts' in asJson:
        dictsResult = getDicts( asJson['dicts'] )
        if dictsResult: result['dicts'] = dictsResult
    return json.dumps( result )

def clearShelf():
    shelf = shelve.open(k_shelfFile)
    shelf['sums'] = {}
    shelf.close()

def usage():
    print "usage:", sys.argv[0], '--get-sums [lang/dict]* | --clear-shelf'

def main():
    arg = sys.argv[1]
    if arg == '--clear-shelf':
        clearShelf()
    elif arg == '--get-sums':
        dictSums = getDictSums()
        for arg in sys.argv[2:]:
            print arg, md5Checksum(dictSums, arg)
        s_shelf['sums'] = dictSums
        s_shelf.close()
    else:
        usage()

##############################################################################
if __name__ == '__main__':
    main()

# Script meant to be installed on eehouse.org that will hard-code the
# latest version of a bunch of stuff and reply with empty string if
# the client's version is up-to-date or with the newer version if it's
# not.  May include md5 sums in liu of versions for .xwd files.

import logging, shelve, hashlib, sys

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
        logging.debug( "figured sum for " + filePath )
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
def curVersion( req, name, version ):
    global k_versions
    result = ""
    logging.debug( "version: " + version )
    if name in k_versions:
        if k_versions[name]['version'] > int(version):
            logging.debug( name + " is old" )
            result = k_urlbase + k_versions[name]['url']
        else:
            logging.debug(name + " is up-to-date")
    else:
        logging.debug( 'Error: bad name ' + name )
    return result

# Order determined by language_names in
# android/XWords4/res/values/common_rsrc.xml
def langStr( lang ):
    langs = ( '<unknown>'
              ,'English'
              ,'French'
              ,'German'
              ,'Turkish'
              ,'Arabic'
              ,'Spanish'
              ,'Swedish'
              ,'Polish'
              ,'Danish'
              ,'Italian'
              ,'Dutch'
              ,'Catalan'
              ,'Portuguese'
              ,''
              ,'Russian'
              ,''
              ,'Czech'
              ,'Greek'
              ,'Slovak' )
    return langs[int(lang)]

# public
def dictVersion( req, name, lang, md5sum ):
    result = ''
    if not name.endswith(k_suffix): name += k_suffix
    dictSums = getDictSums()
    path = langStr(lang) + "/" + name
    if not path in dictSums:
        sum = md5Checksum( dictSums, path )
        if sum:
            dictSums[path] = sum
            s_shelf['sums'] = dictSums
    if path in dictSums and dictSums[path] != md5sum:
        logging.debug( dictSums[path] + " vs " + md5sum )
        result = k_urlbase + "and_wordlists/" + path
    else:
        logging.debug( path + " not known" )
    s_shelf.close()
    return result

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

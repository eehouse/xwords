# Script meant to be installed on eehouse.org that will hard-code the
# latest version of a bunch of stuff and reply with empty string if
# the client's version is up-to-date or with the newer version if it's
# not.  May include md5 sums in liu of versions for .xwd files.

import logging

s_versions = { 'org.eehouse.android.xw4' : '42'
               ,'org.eehouse.android.xw4sms' : '41'
               }

s_dictSums = { 'Catalan/DISC2_2to9' : 'd02349fd4021f7a5a5dfd834dc4f2491'
               ,'English/CSW12_2to8': '2314a99c1a6af2900db3aefcd2186060'
               ,'English/CSW12_2to9': '0b4b1c49d58fb8149535a29b786b8638_x'
               }

logging.basicConfig(level=logging.DEBUG
        ,format='%(asctime)s [[%(levelname)s]] %(message)s'
        ,datefmt='%d %b %y %H:%M'
        ,filename='/tmp/info_py.log')
#        ,filemode='w')

# This seems to be required to prime the pump somehow.
logging.debug( "loaded...." )

# public
def curVersion( req, name, version ):
    global s_versions
    logging.debug( "version: " + version )
    if name in s_versions:
        if s_versions[name] == version:
            logging.debug(name + " is up-to-date")
            return ""
        else:
            logging.debug( name + " is old" )
            return s_versions[name]
    else:
        logging.debug( 'Error: bad name ' + name )

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
    global s_dictSums
    result = ''
    path = langStr(lang) + "/" + name
    if path in s_dictSums:
        if s_dictSums[path] != md5sum:
            result = "http://eehouse.org/and_wordlists/" + path + ".xwd"
    else:
        logging.debug( path + " not known" )
    return result

# Script meant to be installed on eehouse.org that will hard-code the
# latest version of a bunch of stuff and reply with empty string if
# the client's version is up-to-date or with the newer version if it's
# not.  May include md5 sums in liu of versions for .xwd files.

import logging

s_versions = { 'org.eehouse.android.xw4' : '42'
               ,'org.eehouse.android.xw4sms' : '41'
               }

logging.basicConfig(level=logging.DEBUG
        ,format='%(asctime)s [[%(levelname)s]] %(message)s'
        ,datefmt='%d %b %y %H:%M'
        ,filename='/tmp/info.py.log'
        ,filemode='a')

def curVersion( req, name, version ):
    global s_versions
    if name in s_versions:
        if s_versions[name] == version:
            logging.debug(name + " is up-to-date")
            return ""
        else:
            return s_versions[name]
    else:
        logging.debug( 'Error: bad name ' + name )

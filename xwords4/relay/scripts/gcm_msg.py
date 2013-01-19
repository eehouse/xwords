#!/usr/bin/python

import sys, psycopg2, json, urllib, urllib2

# I'm not checking my key in...
import mykey

GCM_URL = 'https://android.googleapis.com/gcm/send'

def usage():
    print 'usage:', sys.argv[0], '[--to <name>] msg'
    sys.exit()

def sendMsg( devid, msg ):
    values = {
        'registration_ids': [ devid ],
        'data' : { 'title' : 'Msg from Darth2',
                   'msg' : msg,
                   }
        }
    params = json.dumps( values )
    req = urllib2.Request("https://android.googleapis.com/gcm/send", params )
    req.add_header( 'Content-Type' , 'application/x-www-form-urlencoded;charset=UTF-8' )
    req.add_header( 'Authorization' , 'key=' + mykey.myKey )
    req.add_header('Content-Type', 'application/json' )
    response = urllib2.urlopen( req )

    response = response.read()
    print response

def main():
    to = None
    msg = sys.argv[1]
    if msg == '--to':
        to = sys.argv[2]
        msg = sys.argv[3]
    elif 2 < len(sys.argv):
        usage()
    if not to in mykey.devids.keys():
        print 'Unknown --to param;', to, 'not in', ','.join(mykey.devids.keys())
        usage()
    if not to: usage()
    devid = mykey.devids[to]
    print 'sending: "%s" to' % msg, to
    sendMsg( devid, msg )

##############################################################################
if __name__ == '__main__':
    main()

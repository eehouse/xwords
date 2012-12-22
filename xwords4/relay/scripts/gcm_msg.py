#!/usr/bin/python

import sys, gcm, psycopg2, json

# I'm not checking my key in...
import mykey

def usage():
    print 'usage:', sys.argv[0], '[--to <name>] msg'
    sys.exit()

def msgViaGCM( devid, msg ):
    instance = gcm.GCM( mykey.myKey )
    data = { 'title' : 'Msg from Darth',
             'msg' : msg,
             }
    
    response = instance.json_request( registration_ids = [devid],
                                      data = data )
    if 'errors' in response:
        response = response['errors']
        if 'NotRegistered' in response:
            ids = response['NotRegistered']
            for id in ids:
                print 'need to remove "', id, '" from db'
    else:
        print 'no errors'

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
    msgViaGCM( devid, msg )

##############################################################################
if __name__ == '__main__':
    main()

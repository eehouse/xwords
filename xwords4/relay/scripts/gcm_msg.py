#!/usr/bin/python

import sys, gcm, psycopg2

# I'm not checking my key in...
import mykey


def msgViaGCM( devid, msg ):
    instance = gcm.GCM( mykey.myKey )
    data = { 'title' : 'Msg from Darth',
             'msg' : msg,
             }
    
    response = instance.json_request( registration_ids = [devid],
                                      data = data )

    if 'errors' in response:
        for error, reg_ids in response.items():
            print error
    else:
        print 'no errors'


def main():
    msg = sys.argv[1]
    print 'got "%s"' % msg
    msgViaGCM( mykey.myBlaze, msg )

##############################################################################
if __name__ == '__main__':
    main()

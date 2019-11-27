#!/usr/bin/python

import argparse, json, psycopg2, sys, urllib, urllib2
from oauth2client.service_account import ServiceAccountCredentials

# I'm not checking my key in...
import mykey2

FCM_URL = 'https://fcm.googleapis.com/v1/projects/fcmtest-9fe99/messages:send'
SCOPES = ["https://www.googleapis.com/auth/firebase.messaging"]

def usage():
    print 'usage:', sys.argv[0], '[--to <name>] msg'
    sys.exit()

def get_access_token():
    credentials = ServiceAccountCredentials.from_json_keyfile_name(
        'service-account.json', SCOPES)
    access_token_info = credentials.get_access_token()
    print 'token:', access_token_info.access_token
    return access_token_info.access_token
    
def sendMsg( devid, msg ):
    values = {
        'message' : {
            'token' : devid,
            'data' : { 'title' : 'Re: CrossWords',
                       'teaser' : 'Please tap to read in the app',
                       'msg' : msg,
            }
        }
    }
    
    params = json.dumps( values )
    print params
    for ignore in [True, True]:
        req = urllib2.Request( FCM_URL, params )
        # req.add_header( 'Content-Type' , 'application/x-www-form-urlencoded;charset=UTF-8' )
        req.add_header('Authorization', 'Bearer ' + get_access_token())
        req.add_header('Content-Type', 'application/json')
        print 'added headers'
        # req.add_header( 'Authorization' , 'key=' + mykey2.myKey )
        try:
            response = urllib2.urlopen( req )
            print 'urlopen done'
            response = response.read()
            print response
        except urllib2.URLError as e:
            print 'error from urlopen:', e.reason
            if e.reason == 'Unauthorized':
                g_accessToken = get_access_token()
            else:
                break

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("msg")
    parser.add_argument('--dest-devid', dest = 'DEVID', type = str, required = True,
                        help = 'fcm devid of target device')

    args = parser.parse_args()
    devid = args.DEVID
    msg = args.msg

    print 'sending: "%s" to' % msg, devid
    sendMsg( devid, msg )

##############################################################################
if __name__ == '__main__':
    main()

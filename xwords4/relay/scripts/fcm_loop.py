#!/usr/bin/python

# Meant to be run on the server that's hosting the relay, loops,
# checking the relay for new messages whose target devices have GCM
# ids and sending GCM notifications to them.
#
# Depends on the gcm module

import argparse, getpass, sys, psycopg2, time, signal, shelve, json, urllib2
from time import gmtime, strftime
from os import path
from oauth2client.service_account import ServiceAccountCredentials

import mykey

FCM_URL = 'https://fcm.googleapis.com/v1/projects/fcmtest-9fe99/messages:send'
SCOPES = ["https://www.googleapis.com/auth/firebase.messaging"]

def get_access_token():
    credentials = ServiceAccountCredentials.from_json_keyfile_name(
        'service-account.json', SCOPES)
    access_token_info = credentials.get_access_token()
    print 'token:', access_token_info.access_token
    return access_token_info.access_token

# g_accessToken = "abcd"          # start with illegal value
g_accessToken = get_access_token()

# Backoff strategy
#
# A message is considered in need of delivery as long as its in the
# msgs table, and the expected behavior is that as soon as a device
# receives a GCM notification it fetches all messages so that they're
# deleted.  But when a device is offline we don't get any errors, so
# while a message remains in that table we need to be sure we don't
# ask GCM to contact the device too often.
#
# But it's devices we contact, not messages.  A device is in the
# contact list if it is the target of at least one message in the msgs
# table.

k_shelfFile = path.splitext( path.basename( sys.argv[0]) )[0] + ".shelf"
k_SENT = 'SENT'
g_useStime = True
g_con = None
g_sent = None
g_debug = False
g_skipSend = False               # for debugging
g_sendAll = False
g_columns = [ 'id', 'devid', 'connname', 'hid', 'msg64' ]
DEVTYPE_GCM = 3                     # 3 == GCM
DEVTYPE_FCM = 6                     # FCM, from DUtilCtxt.java
LINE_LEN = 76

def init():
    global g_sent
    try:
        user = getpass.getuser()
        print 'user:', user
        port = mykey.psqlPort
        con = psycopg2.connect(port=port, database='xwgames', user=user)
    except psycopg2.DatabaseError, e:
        print 'Error %s' % e 
        sys.exit(1)

    shelf = shelve.open( k_shelfFile )
    if k_SENT in shelf: g_sent = shelf[k_SENT]
    else: g_sent = {}
    shelf.close();
    if g_debug: print 'init(): g_sent:', g_sent

    return con

#  WHERE stime IS NULL

def getPendingMsgs( con, typ ):
    cur = con.cursor()
    query = "SELECT %s FROM msgs WHERE "
    if g_useStime:
        query += " stime = 'epoch' AND "
    query += """ devid IN (SELECT id FROM devices WHERE devtypes[1]=%d and NOT unreg) 
        AND (connname IS NULL OR NOT connname IN (SELECT connname FROM games WHERE dead));"""
    cur.execute(query % (",".join( g_columns ), typ))

    result = []
    for row in cur:
        rowObj = {}
        for ii in range( len( g_columns ) ):
            rowObj[g_columns[ii]] = row[ii]
        result.append( rowObj )
    if g_debug: print "getPendingMsgs=>", result
    return result

def addClntVers( con, rows ):
    query = """select clntVers from devices where id = %d;"""
    cur = con.cursor()
    for row in rows:
        cur.execute( query % (row['devid']) )
        if cur.rowcount == 1: row['clntVers'] = cur.fetchone()[0]
        else: print "bad row count: ", cur.rowcount
    con.commit()
    return rows

def deleteMsgs( con, msgIDs ):
    if 0 < len( msgIDs ):
        if g_useStime:
            query = "UPDATE msgs SET stime = 'now' where id in (%s);" % ",".join(msgIDs)
        else:
            query = "DELETE from msgs where id in (%s);" % ",".join(msgIDs)
        try:
            cur = con.cursor()
            cur.execute(query)
            con.commit()
        except psycopg2.DatabaseError, e:
            print 'Error %s' % e 
        except Exception as inst:
            print "failed to execute", query
            print type(inst)
            print inst.args
            print inst

def unregister( gcmid ):
    global g_con
    print "unregister(", gcmid, ")"
    query = "UPDATE devices SET unreg=TRUE WHERE devids[1] = '%s' and devtypes[1] = 3" % gcmid
    g_con.cursor().execute( query )
    g_con.commit()

def asGCMIds(con, devids, typ):
    cur = con.cursor()
    query = "SELECT devids[1] FROM devices WHERE devtypes[1] = %d AND id IN (%s)" \
        % (typ, ",".join([str(y) for y in devids]))
    cur.execute( query )
    result = [elem[0] for elem in cur.fetchall()]
    if g_debug: print 'asGCMIds() =>', result
    return result

def notifyViaFCM(devids, typ, target):
    success = False
    if typ == DEVTYPE_FCM:
        if 'clntVers' in target and 3 <= target['clntVers'] and target['msg64']:
            # data = { 'msgs64': str([target['msg64']]) },
            data = { 'msgs64': json.dumps([target['msg64']]) }
            if target['connname'] and target['hid']:
                data['connname'] = "%s/%d" % (target['connname'], target['hid'])
        else:
            data = { 'getMoves': True, }
        values = {
            'message' : {
                'token' : devids[0],
                'data' : data,
            }
        }
        success = send(values)
    else:
        print "not sending to", len(devids), "devices because typ ==", typ
    return success

def send(values):
    global g_accessToken
    success = False
    params = json.dumps(values)

    if g_skipSend:
        print
        print "not sending:", params
    else:
        for ignore in [True, True]: # try twice at most
            req = urllib2.Request( FCM_URL, params )
            req.add_header( 'Authorization', 'Bearer ' + g_accessToken )
            req.add_header( 'Content-Type', 'application/json' )
            try:
                response = urllib2.urlopen( req ).read()
                asJson = json.loads( response  )

                # not sure what the response format looks like to test for success....
                if 'name' in asJson: # and len(asJson['name']) == len(devids):
                    print "OK; no failures: ", response
                    success = True
                else:
                    print "Errors: "
                    print response
                break

            except urllib2.URLError as e:
                print 'error from urlopen:', e.reason
                if e.reason == 'Unauthorized':
                    g_accessToken = get_access_token()
                else:
                    break
    return success

def shouldSend(val):
    pow = 1
    while pow < val: pow *= 2
    result = pow == val
    # print "shouldSend(", val, ") =>", result
    return result

# given a list of msgid, devid lists, figure out which messages should
# be sent/resent now and mark them as sent.  Backoff is based on
# msgids: if the only messages a device has pending have been seen
# before, backoff applies.
def targetsAfterBackoff( msgs, ignoreBackoff ):
    global g_sent
    targets = {}
    for row in msgs:
        msgid = row['id']
        devid = row['devid']
        if not msgid in g_sent:
            g_sent[msgid] = 0
        g_sent[msgid] += 1
        if ignoreBackoff or shouldSend( g_sent[msgid] ):
            if not devid in targets: targets[devid] = []
            targets[devid].append(row)
    print "targetsAfterBackoff() using:", g_sent
    return targets

# devids is an array of (msgid, devid) tuples
def pruneSent( devids ):
    global g_sent
    if g_debug: print "pruneSent: before:", g_sent
    lenBefore = len(g_sent)
    msgids = []
    for row in devids:
        msgids.append(row['id'])
    for msgid in g_sent.keys():
        if not msgid in msgids:
            del g_sent[msgid]
    if g_debug: print "pruneSent: after:", g_sent

def cleanup():
    global g_con, g_sent
    if g_con: 
        g_con.close()
        g_con = None
    shelf = shelve.open( k_shelfFile )
    shelf[k_SENT] = g_sent
    shelf.close();

def handleSigTERM( one, two ):
    print 'handleSigTERM called: ', one, two
    cleanup()

def loop(args):
    global g_con, g_sent, g_debug
    g_con = init()
    emptyCount = 0
    signal.signal( signal.SIGTERM, handleSigTERM )
    signal.signal( signal.SIGINT, handleSigTERM )

    while g_con:
        if g_debug: print
        nSent = 0
        devids = getPendingMsgs( g_con, args.TYPE )
        # print "got msgs:", len(devids)
        if 0 < len(devids):
            devids = addClntVers( g_con, devids )
            targets = targetsAfterBackoff( devids, False )
            print 'got', len(targets), 'targets'
            if 0 < len(targets):
                if 0 < emptyCount: print ""
                emptyCount = 0
                print strftime("%Y-%m-%d %H:%M:%S", time.localtime()),
                if g_debug: print "devices needing notification:", targets, '=>',
                toDelete = []
                for devid in targets.keys():
                    for targetRow in targets[devid]:
                        if notifyViaFCM( asGCMIds(g_con, [devid], args.TYPE), args.TYPE, targetRow ) \
                           and 3 <= targetRow['clntVers'] \
                           and targetRow['msg64']:
                            toDelete.append( str(targetRow['id']) )
                            nSent += 1
                pruneSent( devids )
                deleteMsgs( g_con, toDelete )
            elif g_debug: print "no targets after backoff"

        if nSent == 0:
            emptyCount += 1
            if (0 == (emptyCount%5)) and not g_debug:
                sys.stdout.write('.')
                sys.stdout.flush()
            if 0 == (emptyCount % (LINE_LEN*5)): print ""
        if 0 == args.LOOP_SECONDS: break
        time.sleep( args.LOOP_SECONDS )

    cleanup()

def sendMessage(args):
    message = args.SEND_MSG
    fcmid = args.FCMID
    if not message:
        print('--send-msg required')
    elif not fcmid:
        print('--fcmid required')
    else:
        data = {'msg': message, 'title' : 'needs title'}
        values = {
            'message' : {
                'token' : fcmid,
                'data' : data,
            }
        }
        success = send(values)
        print( 'sendMessage({}): send() => {}'.format(message, success))

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--send-msg', dest = 'SEND_MSG', type = str, default = None,
                        help = 'a message to send (then exit)')
    parser.add_argument('--fcmid', dest = 'FCMID', type = str, default = None,
                        help = 'the FCMID of the device to send to (then exit)')
    parser.add_argument('--loop', dest = 'LOOP_SECONDS', type = int, default = 5,
                        help = 'loop forever, checking the relay every <loop> seconds' )
    parser.add_argument('--type', dest = 'TYPE', type = int, default = DEVTYPE_FCM,
                        help = 'type. Just use the default')
    parser.add_argument('--verbose', dest = 'VERBOSE', action = 'store_true', default = False)
    return parser

def main():
    args = mkParser().parse_args()
    global g_debug
    g_debug = args.VERBOSE
    if args.SEND_MSG or args.FCMID: sendMessage( args )
    else: loop(args);

##############################################################################
if __name__ == '__main__':
    main()

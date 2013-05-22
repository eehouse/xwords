#!/usr/bin/python

# Meant to be run on the server that's hosting the relay, loops,
# checking the relay for new messages whose target devices have GCM
# ids and sending GCM notifications to them.
#
# Depends on the gcm module

import getpass, sys, psycopg2, time, signal, shelve, json, urllib2
from time import gmtime, strftime
from os import path

# I'm not checking my key in...
import mykey

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
g_con = None
g_sent = None
g_debug = False
g_skipSend = False               # for debugging
g_columns = [ 'id', 'devid', 'connname', 'hid', 'msg64' ]
DEVTYPE_GCM = 3                     # 3 == GCM
LINE_LEN = 76

def init():
    global g_sent
    try:
        con = psycopg2.connect(database='xwgames', user=getpass.getuser())
    except psycopg2.DatabaseError, e:
        print 'Error %s' % e 
        sys.exit(1)

    shelf = shelve.open( k_shelfFile )
    if k_SENT in shelf: g_sent = shelf[k_SENT]
    else: g_sent = {}
    shelf.close();
    if g_debug: print 'g_sent:', g_sent

    return con

#  WHERE stime IS NULL

def getPendingMsgs( con, typ ):
    cur = con.cursor()
    query = """SELECT %s FROM msgs 
        WHERE devid IN (SELECT id FROM devices WHERE devtype=%d and NOT unreg) 
        AND NOT connname IN (SELECT connname FROM games WHERE dead); """
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
    query = """select clntVers[%s] from games where connname = '%s';"""
    cur = con.cursor()
    for row in rows:
        cur.execute( query % (row['hid'], row['connname']))
        if cur.rowcount == 1: row['clntVers'] = cur.fetchone()[0]
        else: print "bad row count: ", cur.rowcount
    con.commit()
    return rows

def deleteMsgs( con, msgIDs ):
    if 0 < len( msgIDs ):
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
    query = "UPDATE devices SET unreg=TRUE WHERE devid = '%s' and devtype = 3" % gcmid
    g_con.cursor().execute( query )
    g_con.commit()

def asGCMIds(con, devids, typ):
    cur = con.cursor()
    query = "SELECT devid FROM devices WHERE devtype = %d AND id IN (%s)" \
        % (typ, ",".join([str(y) for y in devids]))
    cur.execute( query )
    return [elem[0] for elem in cur.fetchall()]

def notifyGCM( devids, typ, target ):
    success = False
    if typ == DEVTYPE_GCM:
        if 3 <= target['clntVers']:
            connname = "%s/%d" % (target['connname'], target['hid'])
            data = { 'msgs64': [ target['msg64'] ],
                     'connname': connname,
                     }
        else:
            data = { 'getMoves': True, }
        values = {
            'data' : data,
            'registration_ids': devids,
            }
        params = json.dumps( values )

        req = urllib2.Request("https://android.googleapis.com/gcm/send", params )
        req.add_header( 'Content-Type' , 'application/x-www-form-urlencoded;charset=UTF-8' )
        req.add_header( 'Authorization' , 'key=' + mykey.myKey )
        req.add_header('Content-Type', 'application/json' )
        response = urllib2.urlopen( req ).read()
        asJson = json.loads( response  )

        if 'success' in asJson and 'failure' in asJson and len(devids) == asJson['success'] and 0 == asJson['failure']:
            print "OK"
            success = True
        else:
            print "Errors: "
            print response
    else:
        print "not sending to", len(devids), "devices because typ ==", typ
    return success

def shouldSend(val):
    return val == 1
    # pow = 1
    # while pow < val:
    #     pow *= 3
    # return pow == val

# given a list of msgid, devid lists, figure out which messages should
# be sent/resent now and mark them as sent.  Backoff is based on
# msgids: if the only messages a device has pending have been seen
# before, backoff applies.
def targetsAfterBackoff( msgs ):
    global g_sent
    targets = {}
    for row in msgs:
        msgid = row['id']
        devid = row['devid']
        if not msgid in g_sent:
            g_sent[msgid] = 0
        g_sent[msgid] += 1
        if shouldSend( g_sent[msgid] ):
            targets[devid] = row
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

def usage():
    print "usage:", sys.argv[0], "[--loop <nSeconds>] [--type typ] [--verbose]"
    sys.exit();

def main():
    global g_con, g_sent, g_debug
    loopInterval = 0
    g_con = init()
    emptyCount = 0
    typ = DEVTYPE_GCM

    ii = 1
    while ii < len(sys.argv):
        arg = sys.argv[ii]
        if arg == '--loop':
            ii += 1
            loopInterval = float(sys.argv[ii])
        elif arg == '--type':
            ii += 1
            typ = int(sys.argv[ii])
        elif arg == '--verbose':
            g_debug = True
        else:
            usage()
        ii = ii + 1

    signal.signal( signal.SIGTERM, handleSigTERM )
    signal.signal( signal.SIGINT, handleSigTERM )

    while g_con:
        if g_debug: print
        devids = getPendingMsgs( g_con, typ )
        if 0 < len(devids):
            devids = addClntVers( g_con, devids )
            targets = targetsAfterBackoff( devids )
            if 0 < len(targets):
                if 0 < emptyCount: print ""
                emptyCount = 0
                print strftime("%Y-%m-%d %H:%M:%S", time.localtime()),
                print "devices needing notification:", targets, '=>',
                toDelete = []
                for devid in targets.keys():
                    target = targets[devid]
                    if notifyGCM( asGCMIds(g_con, [devid], typ), typ, target )\
                            and 3 <= target['clntVers']:
                        toDelete.append( str(target['id']) )
                pruneSent( devids )
                deleteMsgs( g_con, toDelete )
            elif g_debug: print "no targets after backoff"
        else:
            emptyCount += 1
            if (0 == (emptyCount%5)) and not g_debug:
                sys.stdout.write('.')
                sys.stdout.flush()
            if 0 == (emptyCount % (LINE_LEN*5)): print ""
        if 0 == loopInterval: break
        time.sleep( loopInterval )

    cleanup()

##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/python

# Meant to be run on the server that's hosting the relay, loops,
# checking the relay for new messages whose target devices have GCM
# ids and sending GCM notifications to them.
#
# Depends on the gcm module

import getpass, sys, gcm, psycopg2, time, signal
from time import gmtime, strftime

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

g_con = None
g_debug = False
g_skipSend = False               # for debugging
DEVTYPE = 3                     # 3 == GCM
LINE_LEN = 76

def init():
    try:
        con = psycopg2.connect(database='xwgames', user=getpass.getuser())
    except psycopg2.DatabaseError, e:
        print 'Error %s' % e 
        sys.exit(1)
    return con

def getPendingMsgs( con ):
    cur = con.cursor()
    cur.execute("SELECT id, devid FROM msgs WHERE devid IN (SELECT id FROM devices WHERE devtype=%d)" % DEVTYPE)
    result = cur.fetchall()
    if g_debug: print "getPendingMsgs=>", result
    return result

def asGCMIds(con, devids):
    cur = con.cursor()
    query = "SELECT devid FROM devices WHERE devtype = %d AND id IN (%s)" \
        % (DEVTYPE, ",".join([str(y) for y in devids]))
    cur.execute( query )
    return [elem[0] for elem in cur.fetchall()]

def notifyGCM( devids ):
    instance = gcm.GCM( mykey.myKey )
    data = { 'getMoves': True, 
             # 'title' : 'Msg from Darth',
             # 'msg' : "I am your father, Luke.",
             }
    # JSON request

    response = instance.json_request( registration_ids = devids,
                                      data = data )

    if 'errors' in response:
        for error, reg_ids in response.items():
            print error
    else:
        print 'no errors'

# given a list of msgid, devid lists, figure out which messages should
# be sent/resent now and mark them as sent.  Backoff is based on
# msgids: if the only messages a device has pending have been seen
# before, backoff applies.
def targetsAfterBackoff( msgs, sent ):
    targets = []
    for row in msgs:
        msgid = row[0]
        if not msgid in sent:
            sent[ msgid ] = True
            targets.append( row[1] )
    return targets

# devids is an array of (msgid, devid) tuples
def pruneSent( devids, sent ):
    if g_debug: print "pruneSent: before:", sent
    lenBefore = len(sent)
    msgids = []
    for row in devids:
        msgids.append(row[0])
    for msgid in sent.keys():
        if not msgid in msgids:
            del sent[msgid]
    if g_debug: print "pruneSent: after:", sent
    return sent

def handleSigTERM( one, two ):
    print 'handleSigTERM called: ', one, two
    global g_con
    if g_con:
        g_con.close()
        g_con = None

def usage():
    print "usage:", sys.argv[0], "[--loop]"
    sys.exit();

def main():
    global g_con
    loopInterval = 0
    g_con = init()
    emptyCount = 0

    ii = 1
    while ii < len(sys.argv):
        arg = sys.argv[ii]
        if arg == '--loop':
            ii = ii + 1
            loopInterval = float(sys.argv[ii])
        else:
            usage()
        ii = ii + 1

    signal.signal( signal.SIGTERM, handleSigTERM )
    signal.signal( signal.SIGINT, handleSigTERM )

    sent = {}
    while g_con:
        devids = getPendingMsgs( g_con )
        if 0 < len(devids):
            targets = targetsAfterBackoff( devids, sent )
            if 0 < len(targets):
                if 0 < emptyCount: print ""
                emptyCount = 0
                print strftime("%Y-%m-%d %H:%M:%S", gmtime()),
                print "devices needing notification:", targets
                if not g_skipSend:
                    notifyGCM( asGCMIds( g_con, targets ) )
                pruneSent( devids, sent )
            else: 
                sys.stdout.write('.')
                sys.stdout.flush()
                emptyCount = emptyCount + 1
                if 0 == (emptyCount % LINE_LEN): print ""
        if 0 == loopInterval: break
        time.sleep( loopInterval )
        if g_debug: print

    if g_con:
        g_con.close()

##############################################################################
if __name__ == '__main__':
    main()

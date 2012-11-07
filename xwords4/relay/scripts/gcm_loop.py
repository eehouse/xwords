#!/usr/bin/python

# Meant to be run on the server that's hosting the relay, loops,
# checking the relay for new messages whose target devices have GCM
# ids and sending GCM notifications to them.
#
# Depends on the gcm module

import sys, gcm, psycopg2, time, signal
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

def init():
    try:
        con = psycopg2.connect(database='xwgames', user='relay')
    except psycopg2.DatabaseError, e:
        print 'Error %s' % e 
        sys.exit(1)
    return con

def getPendingMsgs( con ):
    cur = con.cursor()
    cur.execute("SELECT id, devid from msgs where devid != 0")
    result = cur.fetchall()
    if g_debug: print "getPendingMsgs=>", result
    return result

def asGCMIds(con, devids):
    cur = con.cursor()
    query = "SELECT devid FROM devices WHERE devtype = 3 AND id IN (%s)" % ",".join([str(y) for y in devids])
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
# be sent/resent now, pass their device ids to the sender, and if it's
# successful mark them as sent.  Backoff is based on msgids: if the
# only messages a device has pending have been seen before, backoff
# applies.
def sendWithBackoff( con, msgs, sent ):
    targets = []
    for row in msgs:
        devid = row[1]
        msgid = row[0]
        if not msgid in sent:
            sent[ msgid ] = True
            targets.append( devid )
            print "sendWithBackoff: sending for", msgid
    if 0 < len(targets):
        print strftime("%Y-%m-%d %H:%M:%S", gmtime()),
        print "sending for:", targets
        devids = asGCMIds( con, targets )
        notifyGCM( devids )
    return sent

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
            sent = sendWithBackoff( g_con, devids, sent )
            sent = pruneSent( devids, sent )
        else: print "no messages found"
        if 0 == loopInterval: break
        time.sleep( loopInterval )
        if g_debug: print

    if g_con:
        g_con.close()

##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/python

# Meant to be run on the server that's hosting the relay, loops,
# checking the relay for new messages whose target devices have GCM
# ids and sending GCM notifications to them.
#
# Depends on the gcm module

import sys, gcm, psycopg2, time, signal
# I'm not checking my key in...
import mykey

g_con = None
g_loop = True
g_sent = {}

def init():
    try:
        con = psycopg2.connect(database='xwgames', user='relay')
    except psycopg2.DatabaseError, e:
        print 'Error %s' % e 
        sys.exit(1)
    return con

def get( con ):
    cur = con.cursor()
    cur.execute("SELECT id, devid from msgs where devid != 0")
    return cur.fetchall()

def asGCMIds(con, devids):
    cur = con.cursor()
    query = "SELECT devid FROM devices WHERE devtype = 3 AND id IN (%s)" % ",".join([str(y) for y in devids])
    cur.execute( query )
    return [elem[0] for elem in cur.fetchall()]

def notifyGCM( devids ):
    print "sending for", len(devids), "devices"
    instance = gcm.GCM( mykey.myKey )
    data = {'param1': 'value1' }
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
def sendWithBackoff( con, msgs ):
    global g_sent
    targets = []
    for row in msgs:
        devid = row[1]
        if devid in targets: continue
        msgid = row[0]
        if not msgid in g_sent:
            g_sent[ msgid ] = True
            targets.append( devid )
    if 0 < len(targets):
        devids = asGCMIds( con, targets )
        notifyGCM( devids )
    else:
        print "no new targets found"

def pruneSent( devids ):
    global g_sent
    print "len of g_sent before prune:", len(g_sent)
    msgids = []
    for row in devids:
        msgids.append(row[0])
    for msgid in g_sent.keys():
        if not msgid in msgids:
            del g_sent[msgid]
    print "len of g_sent after prune:", len(g_sent)

def handleSigTERM( one, two ):
    print 'handleSigTERM called: ', one, two
    global g_con
    if g_con:
        g_con.close()
        g_con = None
    
signal.signal( signal.SIGTERM, handleSigTERM )
signal.signal( signal.SIGINT, handleSigTERM )

g_con = init()
while g_con:
    devids = get( g_con )
    if 0 < len(devids):
        sendWithBackoff( g_con, devids )
        pruneSent( devids )
    else: print "no messages found"
    if not g_loop: break
    time.sleep( 5 )

if g_con:
    g_con.close()

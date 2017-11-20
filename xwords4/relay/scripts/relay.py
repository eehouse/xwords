#!/usr/bin/python

import base64, json, mod_python, socket, struct, sys
import psycopg2, random

PROTOCOL_VERSION = 0
PRX_DEVICE_GONE = 3
PRX_GET_MSGS = 4

# try:
#     from mod_python import apache
#     apacheAvailable = True
# except ImportError:
#     apacheAvailable = False

# Joining a game. Basic idea is you have stuff to match on (room,
# number in game, language) and when somebody wants to join you add to
# an existing matching game if there's space otherwise create a new
# one. Problems are the unreliablity of transport: if you give a space
# and the device doesn't get the message you can't hold it forever. So
# device provides a seed that holds the space. If it asks again for a
# space with the same seed it gets the same space. If it never asks
# again (app deleted, say), the space needs eventually to be given to
# somebody else. I think that's done by adding a timestamp array and
# treating the space as available if TIME has expired. Need to think
# about this: what if app fails to ACK for TIME, then returns with
# seed to find it given away. Let's do a 30 minute reservation for
# now? [Note: much of this is PENDING]

def join(req, devID, room, seed, hid = 0, lang = 1, nInGame = 2, nHere = 1, inviteID = None):
    assert hid <= 4
    seed = int(seed)
    assert seed != 0
    nInGame = int(nInGame)
    nHere = int(nHere)
    assert nHere <= nInGame
    assert nInGame <= 4

    devID = int(devID, 16)

    connname = None
    logs = []                   # for debugging
    # logs.append('vers: ' + platform.python_version())

    con = psycopg2.connect(database='xwgames')
    cur = con.cursor()
    # cur.execute('LOCK TABLE games IN ACCESS EXCLUSIVE MODE')

    # First see if there's a game with a space for me. Must match on
    # room, lang and size. Must have room OR must have already given a
    # spot for a seed equal to mine, in which case I get it
    # back. Assumption is I didn't ack in time.

    query = "SELECT connname, seeds, nperdevice FROM games "
    query += "WHERE lang = %s AND nTotal = %s AND room = %s "
    query += "AND (njoined + %s <= ntotal OR %s = ANY(seeds)) "
    query += "LIMIT 1"
    cur.execute( query, (lang, nInGame, room, nHere, seed))
    for row in cur:
        (connname, seeds, nperdevice) = row
        print('found', connname, seeds, nperdevice)
        break                   # should be only one!

    # If we've found a match, we either need to UPDATE or, if the
    # seeds match, remind the caller of where he belongs. If a hid's
    # been specified, we honor it by updating if the slot's available;
    # otherwise a new game has to be created.
    if connname:
        if seed in seeds and nHere == nperdevice[seeds.index(seed)]:
            hid = seeds.index(seed) + 1
            print('resusing seed case; outta here!')
        else:
            if hid == 0:
                # Any gaps? Assign it
                if None in seeds:
                    hid = seeds.index(None) + 1
                else:
                    hid = len(seeds) + 1
                print('set hid to', hid, 'based on ', seeds)
            else:
                print('hid already', hid)
            query = "UPDATE games SET njoined = njoined + %s, "
            query += "devids[%d] = %%s, " % hid
            query += "seeds[%d] = %%s, " % hid
            query += "jtimes[%d] = 'now', " % hid
            query += "nperdevice[%d] = %%s " % hid
            query += "WHERE connname = %s "
            print(query)
            params = (nHere, devID, seed, nHere, connname)
            cur.execute(query, params)

    # If nothing was found, add a new game and add me. Honor my hid
    # preference if specified
    if not connname:
        # This requires python3, which likely requires mod_wsgi
        # ts = datetime.datetime.utcnow().timestamp()
        # connname = '%s:%d:1' % (xwconfig.k_HOSTNAME, int(ts * 1000))
        connname = '%s:%d:1' % (xwconfig.k_HOSTNAME, random.randint(0, 10000000000))
        useHid = hid == 0 and 1 or hid
        print('not found case; inserting using hid:', useHid)
        query = "INSERT INTO games (connname, room, lang, ntotal, njoined, " + \
                "devids[%d], seeds[%d], jtimes[%d], nperdevice[%d]) " % (4 * (useHid,))
        query += "VALUES (%s, %s, %s, %s, %s, %s, %s, 'now', %s) "
        query += "RETURNING connname, array_length(seeds,1); "
        cur.execute(query, (connname, room, lang, nInGame, nHere, devID, seed, nHere))
        for row in cur:
            connname, gothid = row
            break
        if hid == 0: hid = gothid

    con.commit()
    con.close()

    result = {'connname': connname, 'hid' : hid, 'log' : ':'.join(logs)}

    return json.dumps(result)

def kill(req, params):
    print(params)
    params = json.loads(params)
    count = len(params)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', 10998))

    header = struct.Struct('!BBh')
    strLens = 0
    for ii in range(count):
        strLens += len(params[ii]['relayID']) + 1
    size = header.size + (2*count) + strLens
    sock.send(struct.Struct('!h').pack(size))
    sock.send(header.pack(PROTOCOL_VERSION, PRX_DEVICE_GONE, count))

    for ii in range(count):
        elem = params[ii]
        asBytes = bytes(elem['relayID'])
        sock.send(struct.Struct('!H%dsc' % (len(asBytes))).pack(elem['seed'], asBytes, '\n'))
    sock.close()

    result = {'err': 0}
    return json.dumps(result)

# winds up in handle_udp_packet() in xwrelay.cpp
def post(req, params):
    err = 'none'
    params = json.loads(params)
    data = params['data']
    timeoutSecs = 'timeoutSecs' in params and params['timeoutSecs'] or 1.0
    binData = [base64.b64decode(datum) for datum in data]

    udpSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udpSock.settimeout(float(timeoutSecs))         # seconds
    addr = ("127.0.0.1", 10997)
    for binDatum in binData:
        udpSock.sendto(binDatum, addr)

    responses = []
    while True:
        try:
            data, server = udpSock.recvfrom(1024)
            responses.append(base64.b64encode(data))
        except socket.timeout:
            #If data is not received back from server, print it has timed out  
            err = 'timeout'
            break
    
    result = {'err' : err, 'data' : responses}
    return json.dumps(result)

def query(req, params):
    print('params', params)
    params = json.loads(params)
    ids = params['ids']
    timeoutSecs = 'timeoutSecs' in params and float(params['timeoutSecs']) or 2.0

    idsLen = 0
    for id in ids: idsLen += len(id)

    tcpSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcpSock.settimeout(timeoutSecs)
    tcpSock.connect(('127.0.0.1', 10998))

    lenShort = 2 + idsLen + len(ids) + 2
    print(lenShort, PROTOCOL_VERSION, PRX_GET_MSGS, len(ids))
    header = struct.Struct('!hBBh')
    assert header.size == 6
    tcpSock.send(header.pack(lenShort, PROTOCOL_VERSION, PRX_GET_MSGS, len(ids)))

    for id in ids: tcpSock.send(id + '\n')

    msgsLists = {}
    try:
        shortUnpacker = struct.Struct('!H')
        resLen, = shortUnpacker.unpack(tcpSock.recv(shortUnpacker.size)) # not getting all bytes
        nameCount, = shortUnpacker.unpack(tcpSock.recv(shortUnpacker.size))
        resLen -= shortUnpacker.size
        print('resLen:', resLen, 'nameCount:', nameCount)
        if nameCount == len(ids) and resLen > 0:
            print('nameCount', nameCount)
            for ii in range(nameCount):
                perGame = []
                countsThisGame, = shortUnpacker.unpack(tcpSock.recv(shortUnpacker.size)) # problem
                print('countsThisGame:', countsThisGame)
                for jj in range(countsThisGame):
                    msgLen, = shortUnpacker.unpack(tcpSock.recv(shortUnpacker.size))
                    print('msgLen:', msgLen)
                    msgs = []
                    if msgLen > 0:
                        msg = tcpSock.recv(msgLen)
                        print('msg len:', len(msg))
                        msg = base64.b64encode(msg)
                        msgs.append(msg)
                    perGame.append(msgs)
                msgsLists[ids[ii]] = perGame
    except:
        None

    return json.dumps(msgsLists)

def main():
    result = None
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        args = sys.argv[2:]
        if cmd == 'query' and len(args) > 0:
            result = query(None, json.dumps({'ids':args}))
        elif cmd == 'post':
            # Params = { 'data' : 'V2VkIE9jdCAxOCAwNjowNDo0OCBQRFQgMjAxNwo=' }
            # params = json.dumps(params)
            # print(post(None, params))
            pass
        elif cmd == 'join':
            if len(args) == 6:
                result = join(None, 1, args[0], int(args[1]), int(args[2]), int(args[3]), int(args[4]), int(args[5]))
        elif cmd == 'kill':
            result = kill( None, json.dumps([{'relayID': args[0], 'seed':int(args[1])}]) )

    if result:
        print '->', result
    else:
        print 'USAGE: query [connname/hid]*'
        print '       join <roomName> <seed> <hid> <lang> <nTotal> <nHere>'
        print '       query [connname/hid]*'
        # print '       post '
        print '       kill <relayID> <seed>'
        print '       join <roomName> <seed> <hid> <lang> <nTotal> <nHere>'

##############################################################################
if __name__ == '__main__':
    main()

#!/usr/bin/python

import base64, json, mod_python, socket, struct, sys

PROTOCOL_VERSION = 0
PRX_DEVICE_GONE = 3
PRX_GET_MSGS = 4

# try:
#     from mod_python import apache
#     apacheAvailable = True
# except ImportError:
#     apacheAvailable = False

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
    
def post(req, params, timeoutSecs = 1.0):
    err = 'none'
    dataLen = 0
    jobj = json.loads(params)
    data = base64.b64decode(jobj['data'])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(float(timeoutSecs))         # seconds
    addr = ("127.0.0.1", 10997)
    sock.sendto(data, addr)

    responses = []
    while True:
        try:
            data, server = sock.recvfrom(1024)
            responses.append(base64.b64encode(data))
        except socket.timeout:
            #If data is not received back from server, print it has timed out  
            err = 'timeout'
            break
    
    jobj = {'err' : err, 'data' : responses}
    return json.dumps(jobj)

def query(req, ids, timeoutSecs = 5.0):
    print('ids', ids)
    ids = json.loads(ids)

    idsLen = 0
    for id in ids: idsLen += len(id)

    tcpSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcpSock.settimeout(float(timeoutSecs))
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
        if cmd == 'query':
            result = query(None, json.dumps(args))
        elif cmd == 'post':
            # Params = { 'data' : 'V2VkIE9jdCAxOCAwNjowNDo0OCBQRFQgMjAxNwo=' }
            # params = json.dumps(params)
            # print(post(None, params))
            None
        elif cmd == 'kill':
            result = kill( None, json.dumps([{'relayID': args[0], 'seed':int(args[1])}]) )

    if result:
        print '->', result
    else:
        print 'USAGE: query [connname/hid]*'
        # print '       post '
        print '       kill <relayID> <seed>'

##############################################################################
if __name__ == '__main__':
    main()

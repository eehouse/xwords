#!/usr/bin/python

import base64, json, mod_python, socket, struct, sys

PROTOCOL_VERSION = 0
PRX_GET_MSGS = 4

try:
    from mod_python import apache
    apacheAvailable = True
except ImportError:
    apacheAvailable = False

def post(req, params, timeoutSecs = 1):
    err = 'none'
    dataLen = 0
    jobj = json.loads(params)
    data = base64.b64decode(jobj['data'])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeoutSecs)         # seconds
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

def query(req, ids):
    print(ids)
    ids = json.loads(ids)

    idsLen = 0
    for id in ids: idsLen += len(id)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)         # seconds
    sock.connect(('127.0.0.1', 10998))

    lenShort = 2 + idsLen + len(ids) + 1
    sock.send(struct.pack("hBBh", socket.htons(lenShort),
                          PROTOCOL_VERSION, PRX_GET_MSGS,
                          socket.htons(len(ids))))

    for id in ids: sock.send(id + '\n')

    unpacker = struct.Struct('2H') # 2s f')
    data = sock.recv(unpacker.size)
    resLen, nameCount = unpacker.unpack(data)
    resLen = socket.ntohs(resLen)
    nameCount = socket.ntohs(nameCount)
    print('resLen:', resLen, 'nameCount:', nameCount)
    msgsLists = {}
    if nameCount == len(ids):
        for ii in range(nameCount):
            perGame = []
            shortUnpacker = struct.Struct('H')
            countsThisGame, = shortUnpacker.unpack(sock.recv(shortUnpacker.size))
            countsThisGame = socket.ntohs(countsThisGame)
            print('countsThisGame:', countsThisGame)
            for jj in range(countsThisGame):
                msgLen, = shortUnpacker.unpack(sock.recv(shortUnpacker.size))
                msgLen = socket.ntohs(msgLen)
                print('msgLen:', msgLen)
                msgs = []
                if msgLen > 0:
                    msg = sock.recv(msgLen)
                    print('msg len:', len(msg))
                    msg = base64.b64encode(msg)
                    msgs.append(msg)
                perGame.append(msgs)
            msgsLists[ids[ii]] = perGame

    return json.dumps(msgsLists)

    # received = sock.recv(1024*4)
    # print('len:', len(received))
    # short resLen = dis.readShort();          // total message length
    # short nameCount = dis.readShort();


def dosend(sock, bytes):
    totalsent = 0
    while totalsent < len(bytes):
        sent = sock.send(bytes[totalsent:])
        if sent == 0:
            raise RuntimeError("socket connection broken")
        totalsent = totalsent + sent
                                                                                    

def main():
    print(query(None, json.dumps(sys.argv[1:])))
    # Params = { 'data' : 'V2VkIE9jdCAxOCAwNjowNDo0OCBQRFQgMjAxNwo=' }
    # params = json.dumps(params)
    # print(post(None, params))

##############################################################################
if __name__ == '__main__':
    main()

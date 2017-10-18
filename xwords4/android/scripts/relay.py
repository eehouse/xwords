#!/usr/bin/python

import mod_python, json, socket, base64

try:
    from mod_python import apache
    apacheAvailable = True
except ImportError:
    apacheAvailable = False
    print('failed')

def post(req, params):
    err = 'none'
    dataLen = 0
    jobj = json.loads(params)
    data = base64.b64decode(jobj['data'])

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(3)         # seconds
    addr = ("127.0.0.1", 10997)
    sock.sendto(data, addr)

    response = None
    try:
        data, server = sock.recvfrom(1024)
        response = base64.b64encode(data)
    except socket.timeout:
        #If data is not received back from server, print it has timed out  
        err = 'timeout'
    
    jobj = {'err' : err, 'data' : response}
    return json.dumps(jobj)

def main():
    params = { 'data' : 'V2VkIE9jdCAxOCAwNjowNDo0OCBQRFQgMjAxNwo=' }
    params = json.dumps(params)
    print(post(None, params))

##############################################################################
if __name__ == '__main__':
    main()

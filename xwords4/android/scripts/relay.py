#!/usr/bin/python

import mod_python, json

try:
    from mod_python import apache
    apacheAvailable = True
except ImportError:
    apacheAvailable = False
    print('failed')

def post(req, params):
    jobj = json.loads(params)
    jobj = {'data' : jobj['data']}
    return json.dumps(jobj)

def main():
    None

##############################################################################
if __name__ == '__main__':
    main()

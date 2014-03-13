#!/usr/bin/python
# Script meant to be installed on eehouse.org.

import getpass, logging, shelve, hashlib, sys, json, subprocess, psycopg2
try:
    from mod_python import apache
    apacheAvailable = True
except ImportError:
    apacheAvailable = False

# I'm not checking my key in...
import mykey

def init():
    global g_sent
    try:
        con = psycopg2.connect(port=mykey.psqlPort, database='xwgames', user=getpass.getuser())
    except psycopg2.DatabaseError, e:
        print 'Error %s' % e 
        sys.exit(1)
    return con


def getForConnnameAndHID( connname, hid ):
    con = init()
    cur = con.cursor()
    query = "SELECT msg64 FROM msgs WHERE connname='%s' AND hid=%d"
    cur.execute(query % (connname, hid))
    result = []
    for row in cur:
        result.append(row[0])

    return json.dumps( result )

def main():
    json = getForConnnameAndHID( 'eehouse.org:53042e07:129', 1 )
    print json

##############################################################################
if __name__ == '__main__':
    main()

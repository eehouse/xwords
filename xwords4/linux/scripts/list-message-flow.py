#!/usr/bin/python3

import getopt, re, sys
import json, psycopg2

"""

I want to understand why some messages linger on the database so
long. So given one or more logfiles that track a linux client's
interaction, look at what it sends and receives and compare that with
what's in the relay's msgs table.

"""

DEVID_PAT = re.compile('.*linux_getDevIDRelay => (\d+)$')
QUERY_GOT_PAT = re.compile('.*>(\d+:\d+:\d+):runWitCurl\(\): got for query: \"({.*})\"$')
# <26828:7f03b7fff700>07:47:20:runWitCurl(): got for post: "{"data": ["AR03ggcAH2gwBwESbnVja3k6NTlmYTFjZmM6MTEw", "AR43ggcAH2gwDQBvAgEAAAAAvdAAAAAAAAAAAJIGUGxheWVyGg==", "AYALgw=="], "err": "timeout"}"
POST_GOT_PAT = re.compile('.*>(\d+:\d+:\d+):runWitCurl\(\): got for post: \"({.*})\"$')
def usage(msg = None):
    if msg: sys.stderr.write('ERROR:' + msg + '\n')
    sys.stderr.write('usage: ' + sys.argv[0] + ': (-l logfile)+ \n')
    sys.exit(1)

def parseLog(log, data):
    devIDs = []
    msgMap = {}
    for line in open(log):
        line = line.strip()
        aMatch = DEVID_PAT.match(line)
        if aMatch:
            devID = int(aMatch.group(1))
            if devID and (len(devIDs) == 0 or devIDs[-1] != devID):
                devIDs.append(devID)

        aMatch = QUERY_GOT_PAT.match(line)
        if aMatch:
            rtime = aMatch.group(1)
            jobj = json.loads(aMatch.group(2))
            for relayID in jobj:
                msgs = jobj[relayID]
                for msgarr in msgs:
                    for msg in msgarr:
                        if not msg in msgMap: msgMap[msg] = []
                        msgMap[msg].append({'rtime' : rtime,})
                        if len(msgMap[msg]) > 1: print('big case')

        aMatch = POST_GOT_PAT.match(line)
        if aMatch:
            jobj = json.loads(aMatch.group(2))
            for datum in jobj['data']:
                data.add(datum)

    return devIDs, msgMap

def fetchMsgs(devIDs, msgMaps, data):
    foundCount = 0
    notFoundCount = 0

    con = psycopg2.connect(database='xwgames')
    cur = con.cursor()
    query = "SELECT ctime, stime, stime-ctime as age, msg64 FROM msgs WHERE devid in (%s) order by ctime" \
            % (','.join([str(id) for id in devIDs]))
    # print(query)
    cur.execute(query)
    for row in cur:
        msg64 = row[3]
        for msgMap in msgMaps:
            if msg64 in msgMap:
                print('added:', row[0], 'sent:', row[1], 'received:', msgMap[msg64][0]['rtime'], 'age:', row[2])
            if msg64 in data:
                foundCount += 1
            else:
                notFoundCount += 1
    print('found:', foundCount, 'not found:', notFoundCount);
                

def main():
    logs = []
    opts, args = getopt.getopt(sys.argv[1:], "l:")
    for option, value in opts:
        if option == '-l': logs.append(value)
        else: usage("unknown option" + option)

    if len(logs) == 0: usage('at least one -l requried')

    msgMaps = []
    devIDs = set()
    data = set()
    for log in logs:
        ids, msgMap = parseLog(log, data)
        msgMaps.append(msgMap)
        for id in ids: devIDs.add(id)

    print(msgMaps)
    print(devIDs)
    fetchMsgs(devIDs, msgMaps, data)

##############################################################################
if __name__ == '__main__':
    main()

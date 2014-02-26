#!/usr/bin/python

import sys, re, datetime
from datetime import date

def CONNNAME_ASSIGN(thread, timestamp, mtch):
    global g_games
    connname = mtch.group(2)
    if not connname in g_games:
        game = {'connname': connname, 
                'cids' : set(), 
                'ts': timestamp
        }
    g_games[connname] = game
    game = g_games[connname]
    game['cids'].add( mtch.group(1) )


def CID(thread, timestamp, mtch):
    print "CID called with", mtch.group(0)

# Global full-line patterns
LINE = re.compile('(<0x.*>)(\d\d:\d\d:\d\d): (.*)$')
DATE = re.compile('It\'s a new day: (\d\d/\d\d/\d\d\d\d)')

# Patterns for the meat of log lines, after thread and timestamp
g_patterns = {
    'CID' : [ re.compile('^cid:(\d*)\((.*):([0-9a-f]*):(\d)\) .*$'), CID ],
    'CONNNAME_ASSIGN': [re.compile('^(cid:\d*)\(.* assignConnName: assigning name: (.*)$'),
                        CONNNAME_ASSIGN],
    'UDP_READ':  [re.compile('read_udp_packet: recvfrom=>(\d*)$')],
    'REFRESH_RESETTING': [re.compile( '^Refresh: RESETTING.*$' )],
    'HANDLE_ENQUEUING': [re.compile( '^handle: enqueuing packet \d* \(socket (\d*), len (\d*)\)$' )],
    'PACKED_DISPATCH': [re.compile( '^thread_main: dispatching packet (\d*) \(socket (\d)\); (\d*) seconds old$' )],
    'HANDLE_UDP_PACKET': [re.compile( '^handle_udp_packet\(msg=(.*)\)$' )],
    'REMEMBERDEVICE': [re.compile( '^rememberDevice\(devid=(.*), saddr=\'.*\'\)')],
    'REMEMBERDEVICE_REPL': [re.compile( '^rememberDevice: replacing address for .*$' )],
    'MAP_CONTAINS': [re.compile( '^dev->addr map now contains \d* entries' )],
    'RETRIEVEMESSAGES': [re.compile( '^retrieveMessages\(\)$' )],
    'ACKPACKETIF': [re.compile( '^ackPacketIf: acking packet \d*$' )],
    'SEND_VIA_UDP_IMPL': [re.compile( '^send_via_udp_impl\(\)=>\d*$' )],
    'REFRESH_ADDING' : [re.compile( '^Refresh: adding \'(.*)\'$' )],
    'REFRESH_REFRESHING' : [re.compile( '^Refresh: refreshing \'(.*)\'; last seen \d* milliseconds ago$' )],
    'PROCESSMESSAGE' : [re.compile( '^processMessage got (.*)$' )],
    'PROCESSRECONNECT' : [re.compile( '^processReconnect\(\)$' )],
    'QUERY' : [re.compile( '^query: .*$' )],
    'ADDTOGAME_QUERY' : [re.compile( '^[a-zA-Z]*: query: .*$' )],
    'FINDGAMEFOR' : [re.compile( '^FindGameFor\((.*)\)=>1$' )],
    'SETOWNER' : [re.compile( '^SetOwner\(owner=(\d*)\); m_ownerCount now (\d*)$' )],
    'HOSTREC_CREATED' : [re.compile( '^HostRec created HostRec with id \d$' ) ],
    'SEND_MSG_VIA_UDP': [re.compile( '^send_msg_via_udp: sent \d* bytes \(plus header\) on UDP socket, token=.*\(\d*\)$')],
    'SEND_MSG_VIA_UDP_DONE': [re.compile( '^send_msg_via_udp\(\)=>\d*$' )],
    'HANDLE_UDP_PACKET_ACK' : [re.compile( '^handle_udp_packet: got ack for packet (\d*)$' )],
    'CALLPROC' : [re.compile( '^callProc\(.*\)$' )],
    'ONMSGACKED' : [re.compile( '^onMsgAcked\(packetID=\d*, acked=true\)$')],
    'RECYCLE_LOCKED_LOCKING' : [re.compile( '^Recycle_locked\(cref=0x.*,cookie=(.*)\)$')],

    'RECYCLE_LOCKED_ERASING' : [re.compile( 'Recycle_locked: erasing cref cid (\d*)')],
}


# indexed by connname.
g_games = {}

def handleLine(thread, timestamp, rest):
    global g_games, g_curDate, g_patterns
    mtch = None
    for RE in g_patterns.keys():
        mtch = g_patterns[RE][0].match( rest )
        if mtch: 
            if 1 < len(g_patterns[RE]):
                g_patterns[RE][1](thread, timestamp, mtch)
            break

    if not mtch:
        print "No match for:", rest
        sys.exit(0)

def main():
    global g_curDate, g_games
    g_curDate = None
    
    for line in sys.stdin:
        line.strip()
        mtch = LINE.match(line)
        if mtch:
            ts = datetime.datetime.strptime(mtch.group(2), '%H:%M:%S')
            if g_curDate:
                ts = datetime.datetime.combine( g_curDate, ts.time() )
            handleLine( mtch.group(1), ts, mtch.group(3) )
            continue
        mtch = DATE.match(line)
        if mtch:
            g_curDate = datetime.datetime.strptime(mtch.group(1), '%d/%m/%Y').date()
            continue
        print "unmatched: ", line
        break

    print 'here'
    for conname in g_games.keys():
        print conname, g_games[conname]['ts']

main()

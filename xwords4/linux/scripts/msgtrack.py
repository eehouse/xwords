#!/usr/bin/python3

import argparse, re, sys
from os import path
from enum import IntEnum
"""Given 2-4 logs from the same game (likely created by discon_ok2.py),
track all the messages they send (or don't.)

Let's track by msg (checksum). For each, track when 

"""


class Message():
    def __init__(self, msgID, sum, msgLen):
        self.msgID = msgID
        self.sum = sum
        self.len = msgLen
        self.sends = []
        self.receives = []
        self.sender = None
        self.receiver = None

    def __repr__(self):
        str = '{}: '.format(self.key())
        str += ' {} => {}'.format(self.sender, self.receiver)
        firstReceipt = self.receives and self.receives[0][0] or None
        str += ' sent: {}; arrived: {}'.format(self.firstSendTime(), firstReceipt)
        (nReceives, nDrops) = self.countReceives();
        str += '; nSents: {}; nReceives: {}; nDrops: {}'.format(len(self.sends), \
                nReceives, nDrops)
        str += '; nLost: {}'.format(len(self.sends) - (len(self.receives)))
        return str

    def addSend(self, sender, ts):
        if not self.sender:
            self.sender = sender
        else:
            assert(self.sender == sender)
        self.sends.append(ts)

    def addReceive(self, receiver, ts, dropped):
        if not self.receiver:
            self.receiver = receiver
        else:
            assert(self.receiver == receiver)
        self.receives.append((ts, dropped))

    def pruneEarlySends(self):
        firstReceipt = self.receives and self.receives[0][0] or None
        if firstReceipt:
            while 1 < len(self.sends) and self.sends[1] < firstReceipt:
                # print('{} < {}: removing from {}'.format(self.sends[1], firstReceipt, self.key()))
                self.sends.pop()

    def firstSendTime(self):
        return self.sends[0]

    def countReceives(self):
        nAccepts = nDrops = 0
        for tup in self.receives:
            if tup[1]: nDrops += 1
            else: nAccepts += 1
        return (nAccepts, nDrops)
        
    def key(self):
        return Message.mkKey(self.msgID, self.sum, self.len)
    
    @staticmethod
    def mkKey(id, sum, len):
        return '{:02d}:{}.{:03d}'.format(int(id), sum, len)

def errExit(msg):
    print(msg, file=sys.stderr)
    sys.exit(-1)

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--logs', nargs = '*', dest = 'LOGS',
                        help = 'two or more logfiles to scan')
    parser.add_argument('--skip-below', dest = 'SKIP_BELOW', default = 16,
                        help = 'ignore messages this length or below (acks?)')
    parser.add_argument('--drop-part', dest = 'DROP_PART', default = '_LOG.txt',
                        help = 'remove this from filename to shorten')
    
    return parser

def getMkMessage(msgs, msgID, len, sum):
    key = Message.mkKey(msgID, sum, len)
    msg = msgs.get(key)
    if not msg:
        msg = Message(msgID, sum, len)
        msgs[key] = msg
    return msg

# pull everything relevant out of logfile

# <16330:7fcf30445a80> 07:54:24:953 <> sendMsg(): sending message of len 135 on cno: 674C|1 with sum 8b8b58399159afdcd54a106d3546423f
# sSendRE = re.compile('<(.*)> (\d\d:\d\d:\d\d:\d\d\d) .*sending message of len (\d+) on cno: (.*) with sum (.*)')

# <18137:7f9b3c16ba80> 08:52:56:750 <> sendMsg(): sending message on cno: C824|1: id: 1; len: 135; sum: e2eb9c89ad6062e1a1d65e9da649957f
sSendRE = re.compile('<(.*)> (\d\d:\d\d:\d\d:\d\d\d) .*sending message on cno: (.*): id: (\d+); len: (\d+); sum: (.*)')

# <16327:7f54562f4a80> 07:54:25:359 <> comms_checkIncomingStream(): got message of len 135 with sum 8b8b58399159afdcd54a106d3546423f
# sReceiveRE = re.compile('<(.*)> (\d\d:\d\d:\d\d:\d\d\d) .*got message of len (\d+) with sum (.*)')

# <23256:7fafba33fa80> 11:53:48:976 ../common/comms.c:comms_msgProcessed(): id: 0; len: 14; sum: 56ba9311510efffe43729241fad1a9b6; rejected: true
sProcessedRE = re.compile('<(.*)> (\d\d:\d\d:\d\d:\d\d\d) .*comms_msgProcessed.*: id: (\d+); len: (\d+); sum: (.+); rejected: (.+)')

def parse(logfile, shortName, msgs, skipLen):
    print('parse({})'.format(shortName))
    with open(logfile, 'r') as log:
        for line in log:
            line = line.strip()

            match = sSendRE.match(line)
            if match:
                msgLen = int(match.group(5))
                if msgLen > skipLen:
                    msg = getMkMessage(msgs, match.group(4), msgLen, match.group(6))
                    msg.addSend(shortName, match.group(2))

            match = sProcessedRE.match(line)
            if match:
                msgLen = int(match.group(4))
                if msgLen > skipLen:
                    msgID = match.group(3)
                    msg = getMkMessage(msgs, msgID, msgLen, match.group(5))
                    rejected = match.group(6)
                    msg.addReceive(shortName, match.group(2), rejected=='true')

def main():
    args = mkParser().parse_args()

    msgs = {}
    for log in args.LOGS:
        if not path.exists(log): errExit('file {} not found'.format(log))
        shortName = path.basename(log).replace(args.DROP_PART, '')
        parse(log, shortName, msgs, args.SKIP_BELOW)

    asArr = [msgs[key] for key in msgs]
    asArr.sort(key = lambda msg: msg.firstSendTime())
    for msg in asArr:
        # msg.pruneEarlySends()
        print(msg)


##############################################################################
if __name__ == '__main__':
    main()

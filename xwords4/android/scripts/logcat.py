#!/usr/bin/python3

"""Beginnings of a script to filter logcat. Eventually I'd like a UI
like what's built into AS or apparently available on the Mac, where
individual tags, threads and and processes can be viewed clicking
buttons or dropdowns. For for now, just a commandline app that watches
for app startup and then prints everything with its PID.
"""

import argparse, re, subprocess, threading


# 10-05 20:49:56.220 11158 11158 I XWApp   : onCreate(); git_rev=android_beta_169-17-g94a709423-dirty

# sPIDPat = re.compile('^.*(\d+) (\d+) I XWApp.*git_rev=.*$')
sPIDPat = re.compile('.*\s(\d+)\s(\d+)\sI\sXWApp.*git_rev.*')
sLinePatFmt = '.* (\d+:\d+:\d+.\d+) {pid} (\d+) D (.*): .*'
sLinePats = {}

def addPid(pid):
    if not pid in sLinePats:
        pat = sLinePatFmt.format(pid=pid)
        reg = re.compile(pat)
        sLinePats[pid] = reg
        print('got pid {}'.format(pid))

def output_reader(proc, args):
    if 'PID' in args: addPid(args.PID)

    for line in iter(proc.stdout.readline, b''):
        line = str(line, 'utf8').strip()
        match = sPIDPat.match(line)
        if match:
            pid1 = int(match.group(1))
            pid2 = int(match.group(2))
            assert pid1 == pid2
            addPid(pid1)
        else:
            for pat in sLinePats.values():
                match = pat.match(line)
                if match:
                    print(line)
                    break

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--device', dest = 'DEVICE', type = str, default = None,
                        help = 'device to follow')
    parser.add_argument('--pid', dest = 'PID', type = int, default = 0,
                        help = 'pid of process')
    return parser

def main():
    args = mkParser().parse_args()

    cmds = ['adb']
    if args.DEVICE: cmds += ['-s', args.DEVICE ]
    cmds += ['logcat']
    proc = subprocess.Popen( cmds, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT)

    threading.Thread(target=output_reader, args=(proc,args,)).start()

##############################################################################
if __name__ == '__main__':
    main()

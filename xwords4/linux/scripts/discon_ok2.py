#!/usr/bin/env python3

import glob, json, os, re, requests, shutil, socket, sys, threading
import argparse, datetime, random, signal, subprocess, time
from shutil import rmtree

g_UDP_PCT_START = 100
gDeadLaunches = 0
g_DROP_N=0
g_NAMES = [None, 'Brynn', 'Ariela', 'Kati', 'Eric']

def pick_ndevs(args):
    RNUM = random.randint(0, 99)
    if RNUM > 95 and args.MAXDEVS >= 4:
        NDEVS = 4
    elif RNUM > 90 and args.MAXDEVS >= 3:
        NDEVS = 3
    else:
        NDEVS = 2
    if NDEVS < args.MINDEVS:
        NDEVS = args.MINDEVS
    return NDEVS

# # Given a device count, figure out how many local players per device.
# # "1 1" would be a two-device game with 1 each.  "1 2 1" a
# # three-device game with four players total
def figure_locals(args, NDEVS):
    NPLAYERS = pick_ndevs(args)
    if NPLAYERS < NDEVS: NPLAYERS = NDEVS
    
    EXTRAS = 0
    if not args.ONEPER:
        EXTRAS = NPLAYERS - NDEVS

    LOCALS = []
    for IGNORE in range(NDEVS):
         COUNT = 1
         if EXTRAS > 0:
             EXTRA = random.randint(0, EXTRAS)
             if EXTRA > 0:
                 COUNT += EXTRA
                 EXTRAS -= EXTRA
         LOCALS.append(COUNT)
    assert 0 < sum(LOCALS) <= 4
    return LOCALS

def player_params(args, NLOCALS, NPLAYERS, NAME_INDX):
    assert 0 < NPLAYERS <= 4
    NREMOTES = NPLAYERS - NLOCALS
    PARAMS = []
    while NLOCALS > 0 or NREMOTES > 0:
        if 0 == random.randint(0, 2) and 0 < NLOCALS:
            PARAMS += ['--robot',  g_NAMES[NAME_INDX]]
            if not args.IQS_SAME:
                PARAMS += ['--robot-iq', str(random.randint(1,100))]
            NLOCALS -= 1
            NAME_INDX += 1
        elif 0 < NREMOTES:
            PARAMS += ['--remote-player']
            NREMOTES -= 1
    return PARAMS

def logReaderStub(dev): dev.logReaderMain()

def statusReaderStub(dev): dev.statusReaderMain()

class Device():
    sHasLDevIDMap = {}
    sTilesLeftPoolPat = re.compile('.*pool_r.*Tiles: (\d+) tiles left in pool')
    sTilesLeftTrayPat = re.compile('.*player \d+ now has (\d+) tiles')
    sRelayIDPat = re.compile('.*UPDATE games.*seed=(\d+),.*relayid=\'([^\']+)\'.*')
    sDevIDPat = re.compile('.*storing new devid: ([\da-fA-F]+).*')
    sMQTTDevIDPat = re.compile('.*getMQTTDevID.*: generated id: ([\d[A-F]+).*')
    sConnPat = re.compile('.*linux_util_informMissing\(isServer.*nMissing=0\).*')

    sScoresDup = []
    sScoresReg = []
    
    def __init__(self, args, game, indx, params, peers, order,
                 db, log, script, nInGame, inDupMode):
        self.game = game
        self.indx = indx
        self.args = args
        self.pid = 0
        self.gamesOver = False
        self.params = params
        self.order = order
        self.db = db
        self.logPath = log
        self.script = script
        self.nInGame = nInGame
        self.inDupMode = inDupMode
        # runtime stuff; init now
        self.app = args.APP_OLD
        self.proc = None
        self.peers = peers
        self.devID = ''
        self.launchCount = 0
        self.allDone = False    # when true, can be killed
        self.nTilesLeftPool = None
        self.nTilesLeftTray = None
        self.relayID = None
        self.inviteeDevID = None
        self.inviteeDevIDs = [] # only servers use this
        self.inviteeMQTTDevID = None
        self.inviteeMQTTDevIDs = []
        self.connected = False
        self.relaySeed = 0
        self.locked = False
        self.msgCount = -1
        self.statusSocketPath = self.logPath.replace('.txt', '.sock')

        self.setApp(args.START_PCT)

        with open(self.logPath, "w") as log:
            log.write('New cmdline: ' + self.app + ' ' + (' '.join([str(p) for p in self.params])))
            log.write(os.linesep)

    def setApp(self, pct):
        if self.app == self.args.APP_OLD and not self.app == self.args.APP_NEW:
            if os.path.exists(self.script) and pct > random.randint(0, 99):
                print('launch(): upgrading {} from {} to {}' \
                      .format(self.devName(), self.app, self.args.APP_NEW))
                self.app = self.args.APP_NEW
                # nuke script to force regeneration
                os.unlink(self.script)

    def devName(self):
        return 'dev_' + str(self.indx)

    def statusReaderMain(self):
        self.statusData = self.statusSocket.recv(1024)
        self.statusSocket.close()

    def logReaderMain(self):
        assert self and self.proc
        # print('logReaderMain called; opening:', self.logPath)
        stdout, stderr = self.proc.communicate()
        nLines = 0
        with open(self.logPath, 'a') as log:
            for line in stderr.splitlines():
                nLines += 1
                log.write(line + os.linesep)

                self.locked = True

                # Check every line for tiles left in pool
                match = Device.sTilesLeftPoolPat.match(line)
                if match: self.nTilesLeftPool = int(match.group(1))

                # Check every line for tiles left in tray
                match = Device.sTilesLeftTrayPat.match(line)
                if match: self.nTilesLeftTray = int(match.group(1))

                if not self.relayID:
                    match = Device.sRelayIDPat.match(line)
                    if match:
                        self.relaySeed = int(match.group(1))
                        self.relayID = match.group(2)

                if self.args.WITH_MQTT and not self.inviteeMQTTDevID:
                    match = Device.sMQTTDevIDPat.match(line)
                    if match:
                        self.inviteeMQTTDevID = int(match.group(1), 16)
                        # print('read mqtt devid: {:16X}'.format(self.inviteeMQTTDevID))

                if not self.connected:
                    match = Device.sConnPat.match(line)
                    if match: self.connected = True

                self.locked = False

        # print('logReaderMain done, wrote lines:', nLines, 'to', self.logPath);

    def checkScript(self):
        if not os.path.exists(self.script):
            args = ['exec']     # without exec means terminate() won't work
            if self.args.VALGRIND:
                args += ['valgrind']
                # args += ['--leak-check=full']
                # args += ['--track-origins=yes']
            args += [self.app] + [str(p) for p in self.params]
            if self.devID: args.extend( ' '.split(self.devID))
            args += [ '$*' ]
            with open( self.script, 'w' ) as fil:
                fil.write( "#!/bin/sh\n" )
                fil.write( ' '.join(args) + '\n' )
            os.chmod(self.script, 0o755)

    def launch(self, canRematch):
        self.setApp(self.args.UPGRADE_PCT)
        self.checkScript()
        self.launchCount += 1
        args = [ self.script, '--close-stdin' ]
        if canRematch:
            args.append('--rematch-when-done')

        if self.statusSocketPath:
            args += ['--status-socket-name', self.statusSocketPath]
            self.statusSocket = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
            self.statusSocket.bind(self.statusSocketPath)
            self.statusReader = threading.Thread(target = statusReaderStub, args=(self,))
            self.statusReader.isDaemon = True
            self.statusReader.start()

        # If I'm an unconnected server and I know a client's relayid,
        # append it so invitation can happen. When more than one
        # device will be invited, the invitations must always go in
        # the same order so channels will be assigned consistently. So
        # keep them in an array as they're encountered, and use in
        # that order
        if self.args.WITH_MQTT:
            if self.order == 1 and not self.connected:
                for peer in self.peers:
                    if peer.inviteeMQTTDevID and not peer == self:
                        if not peer.inviteeMQTTDevID in self.inviteeMQTTDevIDs:
                            self.inviteeMQTTDevIDs.append(peer.inviteeMQTTDevID)
                if self.inviteeMQTTDevIDs:
                    args += [ '--force-invite' ]
                    for idid in self.inviteeMQTTDevIDs:
                        asHexStr = '{:16X}'.format(idid)
                        args += ['--invitee-mqtt-devid', asHexStr]

        self.proc = subprocess.Popen(args, stdout = subprocess.DEVNULL,
                                     stderr = subprocess.PIPE, universal_newlines = True)
        self.pid = self.proc.pid
        self.minEnd = datetime.datetime.now() + datetime.timedelta(seconds = self.args.MINRUN)

        # Now start a thread to read stdio
        self.reader = threading.Thread(target = logReaderStub, args=(self,))
        self.reader.isDaemon = True
        self.reader.start()

    def running(self):
        return self.proc and not self.proc.poll()

    def minTimeExpired(self):
        assert self.proc
        return self.minEnd < datetime.datetime.now()
        
    def kill(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            self.proc.wait()
            assert self.proc.poll() is not None

            self.reader.join()
            self.reader = None

            if self.statusSocketPath:
                self.statusReader.join()
                os.unlink(self.statusSocketPath)
        else:
            print('NOT killing')
        self.proc = None
        self.check_games_over()

    def handleAllDone(self):
        global gDeadLaunches
        if self.allDone:
            self.moveFiles()
            gDeadLaunches += self.launchCount
        return self.allDone

    def moveFiles(self):
        assert not self.running()
        for fil in [ self.logPath, self.db, self.script ]:
            shutil.move(fil, self.args.LOGDIR + '/done')

    def getTilesCount(self):
        assert not self.locked
        return {'index': self.indx,
                'nTilesLeftPool': self.nTilesLeftPool,
                'nTilesLeftTray': self.nTilesLeftTray,
                'launchCount': self.launchCount,
                'game': self.game,
        }

    def update_ldevid(self):
        if not self.app in Device.sHasLDevIDMap:
            hasLDevID = False
            proc = subprocess.Popen([self.app, '--help'], stderr=subprocess.PIPE)
            # output, err, = proc.communicate()
            for line in proc.stderr.readlines():
                if b'--ldevid' in line:
                    hasLDevID = True
                    break
            # print('found --ldevid:', hasLDevID);
            Device.sHasLDevIDMap[self.app] = hasLDevID

        if Device.sHasLDevIDMap[self.app]:
            RNUM = random.randint(0, 99)
            if not self.devID:
                if RNUM < 30:
                    self.devID = '--ldevid LINUX_TEST_%.5d_' % (self.indx)
            elif RNUM < 10:
                self.devID += 'x'

    def check_games_over(self):
        data = json.loads(self.statusData)
        self.gamesOver = data.get('allDone', False)
        if self.gamesOver and not self.allDone:
            allDone = True
            for dev in self.peers:
                if dev == self: continue
                if not dev.gamesOver:
                    allDone = False
                    break

            if allDone:
                for dev in self.peers:
                    assert self.game == dev.game
                    dev.allDone = True

def makeSMSPhoneNo( game, dev ):
    return '{:03d}{:03d}'.format( game, dev )

def build_cmds(args):
    devs = []
    COUNTER = 0

    for GAME in range(1, args.NGAMES + 1):
        peers = set()
        NDEVS = pick_ndevs(args)
        LOCALS = figure_locals(args, NDEVS) # as array
        NPLAYERS = sum(LOCALS)
        assert(len(LOCALS) == NDEVS)
        DICT = args.DICTS[GAME % len(args.DICTS)]
        # make one in three games public
        useDupeMode = random.randint(0, 100) < args.DUP_PCT
        if args.PHONIES == -1: phonies = GAME % 3
        else: phonies = args.PHONIES
        DEV = 0
        for NLOCALS in LOCALS:
            DEV += 1
            DB = '{}/{:02d}_{:02d}_DB.sql3'.format(args.LOGDIR, GAME, DEV)
            LOG = '{}/{:02d}_{:02d}_LOG.txt'.format(args.LOGDIR, GAME, DEV)
            SCRIPT = '{}/start_{:02d}_{:02d}.sh'.format(args.LOGDIR, GAME, DEV)

            PARAMS = player_params(args, NLOCALS, NPLAYERS, DEV)
            if not args.USE_GTK: PARAMS += ['--curses']
            PARAMS += ['--board-size', '15', '--sort-tiles']
            if not useDupeMode: PARAMS += ['--trade-pct', args.TRADE_PCT]

            # We SHOULD support having both SMS and relay working...
            if args.WITH_SMS:
                PARAMS += [ '--sms-number', makeSMSPhoneNo(GAME, DEV) ]
                if args.SMS_FAIL_PCT > 0:
                    PARAMS += [ '--sms-fail-pct', args.SMS_FAIL_PCT ]
                if DEV == 1:
                    PARAMS += [ '--force-invite' ]
                    for dev in range(2, NDEVS + 1):
                        PARAMS += [ '--invitee-sms-number', makeSMSPhoneNo(GAME, dev) ]

            PARAMS += [ '--mqtt-port', args.MQTT_PORT, '--mqtt-host', args.MQTT_HOST ]
            if args.WITH_MQTT:
                PARAMS += [ '--with-mqtt' ]
                if DEV == 1:
                    PARAMS += [ '--force-invite' ]
            else:
                PARAMS += [ '--without-mqtt' ]

            if args.UNDO_PCT > 0:
                PARAMS += ['--undo-pct', args.UNDO_PCT]
            PARAMS += [ '--game-dict', DICT]
            # Removing --slow-robot for now. With it on (and
            # successfully passed through to the curses client, which
            # hasn't always been happening), 20% of games
            # stall. PENDING...
            # PARAMS += ['--slow-robot', '1:3']
            PARAMS += ['--skip-confirm']
            PARAMS += ['--db', DB]

            PARAMS += ['--drop-nth-packet', g_DROP_N]
            PARAMS += ['--split-packets', '2']
            if args.SEND_CHAT:
                PARAMS += ['--send-chat', args.SEND_CHAT]

            if args.DUP_PACKETS:
                PARAMS += ['--dup-packets']
            # PARAMS += ['--my-port', '1024']
            # PARAMS += ['--savefail-pct', 10]

            # With the --seed param passed, games with more than 2
            # devices don't get going. No idea why. This param is NOT
            # passed in the old bash version of this script, so fixing
            # it isn't a priority.
            # PARAMS += ['--seed', args.SEED]

            if DEV == 1:
                PARAMS += ['--force-game']
                PARAMS += ['--server', '--phonies', phonies ]
                if 0 == args.TRAYSIZE: traySize = random.randint(7, 9)
                else: traySize = args.TRAYSIZE
                PARAMS += ['--tray-size', traySize] # randint() is *inclusive*
                # IFF there are any non-1 player counts, tell inviter which
                if sum(LOCALS) > NDEVS:
                    PARAMS += ['--invitee-counts', ":".join(str(n) for n in LOCALS[1:])]
            else:
                PARAMS += ['--force-channel', DEV - 1]
            if args.PHONY_PCT and phonies == 2: PARAMS += [ '--make-phony-pct', args.PHONY_PCT ]

            if useDupeMode: PARAMS += ['--duplicate-mode']

            PARAMS += ['--board-size', args.BOARD_SIZE]

            # print('PARAMS:', PARAMS)

            dev = Device( args, GAME, COUNTER, PARAMS, peers,
                          DEV, DB, LOG, SCRIPT, len(LOCALS), useDupeMode )
            peers.add(dev)
            dev.update_ldevid()
            devs.append(dev)

            COUNTER += 1
    return devs

def summarizeTileCounts(devs, endTime, state, changeSecs):
    global gDeadLaunches
    shouldGoOn = True
    data = [dev.getTilesCount() for dev in devs]
    dupModeFlags = [dev.inDupMode for dev in devs]
    nDevs = len(data)
    totalTilesStd = 0
    totalTilesDup = 0
    colWidth = max(2, len(str(nDevs)))
    headWidth = 0
    fmtData = [{'head' : 'dev', },
               {'head' : 'launches', },
               {'head' : 'tls left', },
    ]
    for datum in fmtData:
        headWidth = max(headWidth, len(datum['head']))
        datum['data'] = []

    # Group devices by game
    games = []
    joinChars = []
    prev = -1
    for datum, inDupMode in zip(data, dupModeFlags):
        gameNo = datum['game']
        if gameNo != prev:
            games.append([])
            if inDupMode: joinChars.append('.')
            else: joinChars.append('+')
            prev = gameNo
        games[-1].append('{:0{width}d}'.format(datum['index'], width=colWidth))

    fmtData[0]['data'] = []
    for game, joinChar in zip(games, joinChars):
        fmtData[0]['data'].append( joinChar.join(game) )

    nLaunches = gDeadLaunches
    for datum, inDupMode in zip(data, dupModeFlags):
        launchCount = datum['launchCount']
        nLaunches += launchCount
        fmtData[1]['data'].append('{:{width}d}'.format(launchCount, width=colWidth))

        # Format tiles left. It's the number in the bag/pool until
        # that drops to 0, then the number in the tray preceeded by
        # '+'. Only the pool number is included in the totalTiles sum.
        nTilesPool = datum['nTilesLeftPool']
        nTilesTray = datum['nTilesLeftTray']
        if nTilesPool is None and nTilesTray is None:
            txt = ('-' * colWidth)
        elif int(nTilesPool) == 0 and not nTilesTray is None:
            txt = '{:+{width}d}'.format(nTilesTray, width=colWidth-1)
        else:
            txt = '{:{width}d}'.format(nTilesPool, width=colWidth)
            if inDupMode: totalTilesDup += int(nTilesPool)
            else: totalTilesStd += int(nTilesPool)
        fmtData[2]['data'].append(txt)

    print('')
    if totalTilesDup: dupDetails = ' (std: {}, dup: {})'.format(totalTilesStd, totalTilesDup)
    else: dupDetails = ''
    # here
    print('devs left: {nDevs}; bag tiles left: {total}{details}; total launches: {nLaunches}; {now}/{endTime}' \
          .format(nDevs=nDevs, total=totalTilesStd + totalTilesDup, details=dupDetails, \
                  nLaunches=nLaunches, now=datetime.datetime.now(), endTime=endTime ))
    fmt = '{head:>%d} {data}' % headWidth
    for datum in fmtData: datum['data'] = ' '.join(datum['data'])
    for datum in fmtData:
        print(fmt.format(**datum))

    # Now let's see if things are stuck: if the tile string hasn't
    # changed in two minutes bail. Note that the count of tiles left
    # isn't enough because it's zero for a long time as devices are
    # using up what's left in their trays and getting killed.
    now = datetime.datetime.now()
    tilesStr = fmtData[2]['data']
    if not 'tilesStr' in state or state['tilesStr'] != tilesStr:
        state['lastChange'] = now
        state['tilesStr'] = tilesStr

    return now - state['lastChange'] < datetime.timedelta(seconds = changeSecs)

def countCores(args):
    count = 0
    if args.CORE_PAT:
        count = len( glob.glob(args.CORE_PAT) )
    return count

gDone = False

def run_cmds(args, devs, startTime):
    nCores = countCores(args)
    endTime = startTime + datetime.timedelta(minutes = args.TIMEOUT_MINS)
    endRematchTime = startTime + datetime.timedelta(seconds = args.REMATCH_SECS)
    printState = {}
    lastPrint = datetime.datetime.now()

    while len(devs) > 0 and not gDone:
        if countCores(args) > nCores:
            print('core file count increased; exiting')
            break
        now = datetime.datetime.now()
        if now > endTime:
            print('outta time; outta here')
            break

        # print stats every 5 seconds
        if now - lastPrint > datetime.timedelta(seconds = 5):
            lastPrint = now
            if not summarizeTileCounts(devs, endTime, printState, args.NO_CHANGE_SECS):
                print('no change in too long; exiting')
                break

        dev = random.choice(devs)
        if not dev.running():
            if dev.handleAllDone():
                devs.remove(dev)
            else:
                canRematch = now < endRematchTime
                dev.launch(canRematch)
        elif dev.minTimeExpired():
            dev.kill()
            if dev.handleAllDone():
                devs.remove(dev)
        else:
            time.sleep(1.0)
        print('.', end='', flush=True)

    # if we get here via a break, kill any remaining games
    if devs:
        print('stopping {} remaining games'.format(len(devs)))
        for dev in devs:
            if dev.running(): dev.kill()

def log_scores( devs ):
    if len(Device.sScoresReg) > 0:
        print( "average score for regular games:",
               sum(Device.sScoresReg) // len(Device.sScoresReg) )
    if len(Device.sScoresDup) > 0:
        print( "average score for dup games:",
               sum(Device.sScoresDup) // len(Device.sScoresDup) )

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--send-chat', dest = 'SEND_CHAT', type = str, default = None,
                        help = 'the message to send')

    parser.add_argument('--app-new', dest = 'APP_NEW', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll use')
    parser.add_argument('--app-old', dest = 'APP_OLD', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll upgrade from')
    parser.add_argument('--start-pct', dest = 'START_PCT', default = 50, type = int,
                        help = 'odds of starting with the new app, 0 <= n < 100')
    parser.add_argument('--upgrade-pct', dest = 'UPGRADE_PCT', default = 20, type = int,
                        help = 'odds of upgrading at any launch, 0 <= n < 100')

    parser.add_argument('--num-games', dest = 'NGAMES', type = int, default = 1, help = 'number of games')
    parser.add_argument('--timeout-mins', dest = 'TIMEOUT_MINS', default = 10000, type = int,
                        help = 'minutes after which to timeout')
    parser.add_argument('--nochange-secs', dest = 'NO_CHANGE_SECS', default = 30, type = int,
                        help = 'seconds without change after which to timeout')
    parser.add_argument('--log-root', dest='LOGROOT', default = '.', help = 'where logfiles go')
    parser.add_argument('--dup-packets', dest = 'DUP_PACKETS', default = False, action = 'store_true',
                        help = 'send all packet twice')
    parser.add_argument('--phonies', dest = 'PHONIES', default = -1, type = int,
                        help = '0 (ignore), 1 (warn)) or 2 (lose turn); default is pick at random')
    parser.add_argument('--make-phony-pct', dest = 'PHONY_PCT', default = 20, type = int,
                        help = 'how often a robot should play a phony (only applies when --phonies==2')
    parser.add_argument('--use-gtk', dest = 'USE_GTK', default = False, action = 'store_true',
                        help = 'run games using gtk instead of ncurses')

    parser.add_argument('--dup-pct', dest = 'DUP_PCT', default = 0, type = int,
                        help = 'this fraction played in duplicate mode')

    # # 
    # #     echo "    [--clean-start]                                         \\" >&2
    parser.add_argument('--game-dict', dest = 'DICTS', action = 'append', default = [])
    # #     echo "    [--help]                                                \\" >&2
    # #     echo "    [--max-devs <int>]                                      \\" >&2
    parser.add_argument('--min-devs', dest = 'MINDEVS', type = int, default = 2,
                        help = 'No game will have fewer devices than this')
    parser.add_argument('--max-devs', dest = 'MAXDEVS', type = int, default = 4,
                        help = 'No game will have more devices than this')

    parser.add_argument('--robots-all-same-iq', dest = 'IQS_SAME', default = False,
                        action = 'store_true', help = 'give all robots the same IQ')

    parser.add_argument('--min-run', dest = 'MINRUN', type = int, default = 2,
                        help = 'Keep each run alive at least this many seconds')
    # #     echo "    [--new-app <path/to/app]                                \\" >&2
    # #     echo "    [--new-app-args [arg*]]  # passed only to new app       \\" >&2
    # #     echo "    [--num-rooms <int>]                                     \\" >&2
    # #     echo "    [--old-app <path/to/app]*                               \\" >&2
    parser.add_argument('--one-per', dest = 'ONEPER', default = False,
                        action = 'store_true', help = 'force one player per device')
    parser.add_argument('--resign-pct', dest = 'RESIGN_PCT', default = 0, type = int, \
                        help = 'Odds of resigning [0..100]')
    parser.add_argument('--seed', type = int, dest = 'SEED',
                        default = random.randint(1, 1000000000))
    # #     echo "    [--send-chat <interval-in-seconds>                      \\" >&2
    # #     echo "    [--udp-incr <pct>]                                      \\" >&2
    # #     echo "    [--udp-start <pct>]      # default: $UDP_PCT_START                 \\" >&2
    # #     echo "    [--undo-pct <int>]                                      \\" >&2

    parser.add_argument('--undo-pct', dest = 'UNDO_PCT', default = 0, type = int)
    parser.add_argument('--trade-pct', dest = 'TRADE_PCT', default = 10, type = int)

    parser.add_argument('--with-sms', dest = 'WITH_SMS', action = 'store_true')
    parser.add_argument('--without-sms', dest = 'WITH_SMS', default = False, action = 'store_false')
    parser.add_argument('--sms-fail-pct', dest = 'SMS_FAIL_PCT', default = 0, type = int)

    parser.add_argument('--with-mqtt', dest = 'WITH_MQTT', default = True, action = 'store_true')
    parser.add_argument('--without-mqtt', dest = 'WITH_MQTT', action = 'store_false')
    parser.add_argument('--mqtt-port', dest = 'MQTT_PORT', default = 1883 )
    parser.add_argument('--mqtt-host', dest = 'MQTT_HOST', default = 'localhost' )

    parser.add_argument('--force-tray', dest = 'TRAYSIZE', default = 0, type = int,
                        help = 'Always this many tiles per tray')

    parser.add_argument('--board-size', dest = 'BOARD_SIZE', type = int, default = 15,
                        help = 'Use <n>x<n> size board')
    parser.add_argument('--rematch-limit-secs', dest = 'REMATCH_SECS', type = int, default = 0,
                        help = 'rematch games that end within this many seconds of script launch')

    parser.add_argument('--core-pat', dest = 'CORE_PAT', default = os.environ.get('DISCON_COREPAT'),
                        help = "pattern for core files that should stop the script " \
                        + "(default from env $DISCON_COREPAT)" )

    parser.add_argument('--with-valgrind', dest = 'VALGRIND', default = False,
                        action = 'store_true')

    return parser

def parseArgs():
    args = mkParser().parse_args()
    assignDefaults(args)
    print(args)
    return args
    # print(options)

def assignDefaults(args):
    if len(args.DICTS) == 0: args.DICTS.append('CollegeEng_2to8.xwd')
    args.LOGDIR = os.path.splitext(os.path.basename(sys.argv[0]))[0] + '_logs'
    # Move an existing logdir aside
    if os.path.exists(args.LOGDIR):
        shutil.move(args.LOGDIR, '/tmp/' + args.LOGDIR + '_' + str(random.randint(0, 100000)))
    for d in ['', 'done', 'dead',]:
        os.mkdir(args.LOGDIR + '/' + d)

def termHandler(signum, frame):
    global gDone
    print('termHandler() called')
    gDone = True

def main():
    startTime = datetime.datetime.now()
    signal.signal(signal.SIGINT, termHandler)

    args = parseArgs()
    # Hack: old files confuse things. Remove is simple fix good for now
    if args.WITH_SMS:
        try: rmtree('/tmp/xw_sms')
        except: None
    devs = build_cmds(args)
    nDevs = len(devs)
    run_cmds(args, devs, startTime)
    print('{} finished; took {} for {} devices'.format(sys.argv[0], datetime.datetime.now() - startTime, nDevs))
    log_scores( devs )

##############################################################################
if __name__ == '__main__':
    main()

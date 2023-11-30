#!/usr/bin/env python3

import argparse, datetime, json, os, random, shutil, signal, \
    socket, struct, subprocess, sys, threading, time

g_NAMES = ['Brynn', 'Ariela', 'Kati', 'Eric']
gDone = False

def log(args, msg):
    if args.VERBOSE:
        now = datetime.datetime.strftime(datetime.datetime.now(), '%X.%f')
        print('{} {}'.format(now, msg))

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

def chooseNames(nPlayers):
    players = g_NAMES[:]
    result = []
    for ii in range(nPlayers):
        indx = random.randint(0, len(players)-1)
        result.append(players.pop(indx))
    return result

class GuestGameInfo():
    def __init__(self, gid):
        self.gid = gid

# Should be subclass of GuestGameInfo
class HostedInfo():
    def __init__(self, guests, gid=None, invitesSent=False):
        self.guestNames = guests
        self.gid = gid and gid or '{:08X}'.format(random.randint(1, 0x7FFFFFFF))
        assert len(self.gid) == 8
        self.invitesSent = invitesSent

    def __str__(self):
        return 'gid: {}, guests: {}'.format(self.gid, self.guestNames)

class GameStatus():
    _N_LINES = 3                # could be lower if all games have 2 players
    _statuses = None

    def __init__(self, gid):
        self.gid = gid
        self.players = []
        self.allOver = True

    def harvest(self, dev):
        self.players.append(dev.host)
        self.allOver = self.allOver and dev.gameOver(self.gid)

    # Build a gid->status map for each game, querying each device in
    # the game for details
    @staticmethod
    def makeAll():
        statuses = {}
        for dev in Device.getAll():
            for game in dev._allGames():
                gid = game.gid
                assert 8 == len(gid)
                if not gid in statuses: statuses[gid] = GameStatus(gid)
                statuses[gid].harvest(dev)

        for gid in list(statuses.keys()):
            if statuses[gid].allOver: del statuses[gid]

        GameStatus._statuses = statuses

    @staticmethod
    def numLines():
        return GameStatus._N_LINES

    # For all games, print the proper line of status for that game in
    # exactly 8 chars
    @staticmethod
    def line(indx):
        results = []
        for gid in sorted(GameStatus._statuses.keys()):
            if indx == 0:
                results.append(gid)
                continue
            players = indx == 1 and g_NAMES[:2] or g_NAMES[2:]
            lineTxt = ''
            for player in players:
                if not player in GameStatus._statuses[gid].players:
                    lineTxt += '    '
                else:
                    dev = Device._devs.get(player)
                    initial = player[0]
                    arg2 = -1
                    status = dev.gameStates.get(gid)
                    if status:
                        if status.get('gameOver', False):
                            initial = initial.lower()
                            arg2 = status.get('nPending', 0)
                        else:
                            arg2 = status.get('nTiles')
                    lineTxt += '{}{: 3}'.format(initial, arg2)
            results.append(lineTxt)
        return ' '.join(results)

class Device():
    _devs = {}
    _logdir = None
    _nSteps = 0

    @staticmethod
    def setup():
        logdir = os.path.splitext(os.path.basename(sys.argv[0]))[0] + '_logs'
        Device._logdir = logdir
        # Move an existing logdir aside
        if os.path.exists(logdir):
            shutil.rmtree(logdir)
            # shutil.move(logdir, '/tmp/' + logdir + '_' + str(random.randint(0, 100000)))
        os.mkdir(logdir)
        for d in ['done', 'dead',]:
            os.mkdir(logdir + '/' + d)

    def __init__(self, args, host):
        self.args = args
        self.endTime = None
        self.mqttDevID = None
        self.smsNumber = args.WITH_SMS and '{}_phone'.format(host) or None
        self.host = host
        self.hostedGames = []       # array of HostedInfo for each game I host
        self.guestGames = []
        self.script = '{}/{}.sh'.format(Device._logdir, host)
        self.dbName = '{}/{}.db'.format(Device._logdir, host)
        self.logfile = '{}/{}_log.txt'.format(Device._logdir, host)
        self.cmdSocketName = '{}/{}.sock'.format(Device._logdir, host)
        self.gameStates = {}
        self._keyCur = 10000 * (1 + g_NAMES.index(host))

    def init(self):
        self._checkScript()

    # called by thread proc
    def _launchProc(self):
        assert not self.endTime
        self.endTime = datetime.datetime.now() + datetime.timedelta(seconds = 5)
        args = [ self.script, '--close-stdin' ]
        if not self.args.USE_GTK: args.append('--curses')
        with open( self.logfile, 'a' ) as logfile:
            subprocess.run(args, stdout = subprocess.DEVNULL,
                           stderr = logfile, universal_newlines = True)
        self._log('_launchProc() (in thread): subprocess FINISHED')
        os.unlink(self.cmdSocketName)
        self.endTime = None

    def launchIfNot(self):
        if not self.endTime:
            self.watcher = threading.Thread(target = Device.runnerStub, args=(self,))
            self.watcher.isDaemon = True
            self.watcher.start()

            while not self.endTime or not os.path.exists(self.cmdSocketName):
                time.sleep(0.2)

    def moveOne(self):
        moved = False
        gids = [game.gid for game in self._allGames() if not self.gameOver(game.gid)]
        random.shuffle(gids)
        for gid in gids:
            response = self._sendWaitReply('moveIf', gid=gid)
            moved = response.get('success', False)
            if moved: break
        return moved

    def _sendWaitReply(self, cmd, **kwargs):
        self.launchIfNot()

        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        # print('connecting to: {}'.format(self.cmdSocketName))

        client.connect(self.cmdSocketName);

        key = self._nextKey()
        params = [{'cmd': cmd, 'key': key, 'args': {**kwargs}}]
        payload = json.dumps(params).encode()
        client.send(struct.pack('!h', len(payload)))
        client.sendall(payload)

        # # Receive a response from the server
        # self._log('_sendWaitReply({}): calling recv()'.format(cmd))
        reslen = struct.unpack('!h', client.recv(2))[0]
        response = client.recv(reslen).decode()
        # self._log('_sendWaitReply({}): recv => str: {}'.format(cmd, response))
        response = json.loads(response)
        self._log('_sendWaitReply({}, {}): recv => {}'.format(cmd, kwargs, response))
        assert 1 == len(response)
        response = response[0]
        assert response.get('key', 0) == key
        assert response.get('cmd') == cmd
        response = response.get('response')

        client.close()
        return response

    def _nextKey(self):
        self._keyCur += 1
        return self._keyCur

    def setDevID(self):
        response = self._sendWaitReply('getMQTTDevID')
        if response:
            self.mqttDevID = response

    def makeGames(self):
        args = self.args
        for remote in self.hostedGames:
            nPlayers = 1 + len(remote.guestNames)
            hostPosn = random.randint(0, nPlayers-1)
            traySize = 0 == args.TRAY_SIZE and random.randint(7, 9) or args.TRAY_SIZE

            self._sendWaitReply('makeGame', nPlayers=nPlayers, hostPosn=hostPosn,
                                gid=remote.gid, dict=args.DICTS[0],
                                boardSize=args.BOARD_SIZE, traySize=traySize)

    # This is the heart of things. Do something as long as we have a
    # game that needs to run.
    def step(self):
        # self._log('step() called for {}'.format(self))
        stepped = False
        for game in self.hostedGames:
            if not game.invitesSent:
                self.invite(game)
                stepped = True
                break

        if not stepped:
            if not self.endTime:
                self.launchIfNot()
            elif datetime.datetime.now() > self.endTime:
                self.quit()
            elif self.moveOne():
                pass
            else:
                # self._log('sleeping with {} to go'.format(self.endTime-now))
                time.sleep(0.5)
            stepped = True;

    # I may be a guest or a host in this game. Rematch works either
    # way. But how I figure out the other players differs.
    def rematch(self, gid):
        newGid = self._sendWaitReply('rematch', gid=gid).get('newGid')
        if newGid:
            guests = Device.playersIn(gid)
            guests.remove(self.host)
            self._log('rematch: new host: {}; new guest[s]: {}, gid: {}'.format(self.host, guests, newGid))

            self.hostedGames.append(HostedInfo(guests, newGid, True))
            for guest in guests:
                Device.getFor(guest).expectInvite(newGid)

    def invite(self, game):
        failed = False
        for ii in range(len(game.guestNames)):
            guestName = game.guestNames[ii]
            # self._log('inviting {}'.format(guestName))
            guestDev = self._devs[guestName]

            addr = {}
            if self.args.WITH_MQTT: addr['mqtt'] = guestDev.mqttDevID
            if self.args.WITH_SMS: addr['sms'] = guestDev.smsNumber
            response = self._sendWaitReply('invite', gid=game.gid,
                                           channel=ii+1, addr=addr,
                                           name=guestName) # just for logging

            if response['success']:
                guestDev.expectInvite(game.gid)
            else:
                failed = True
        if not failed: game.invitesSent = True

    def expectInvite(self, gid):
        self.guestGames.append(GuestGameInfo(gid))
        self.launchIfNot()

    # Return true only if all games I host are finished on all games.
    # But: what about games I don't host? For now,  let's make it all
    # games!
    def finished(self):
        allGames = self._allGames()
        result = 0 < len(allGames)
        for game in allGames:
            if not result: break
            peers = Device.devsWith(game.gid)
            for dev in peers:
                result = dev.gameOver(game.gid)
                if not result: break
        # if result: self._log('finished() => {}'.format(result))
        return result

    def gameOver(self, gid):
        result = False
        """Is the game is over for *this* device"""
        gameState = self.gameStates.get(gid, None)
        if gameState:
            result = gameState.get('gameOver', False) and 0 == gameState.get('nPending', 1)
        # if result: self._log('gameOver({}) => {}'.format(gid, result))
        return result

    # this device is stalled if none of its unfinshed games has
    # changed state in some interval
    def stalled(self):
        return False

    def _allGames(self):
        return self.hostedGames + self.guestGames

    def haveGame(self, gid):
        withGid = [game for game in self._allGames() if gid == game.gid]
        return 0 < len(withGid)

    def quit(self):
        if self.endTime:
            allGames = self._allGames()
            gids = [game.gid for game in allGames if not self.gameOver(game.gid)]
            response = self._sendWaitReply('quit', gids=gids)

            for obj in response:
                gid = obj.get('gid')
                self.gameStates[gid] = obj

            # wait for the thing to actually die
            self.watcher.join()
            self.watcher = None
            assert not self.endTime

    def _checkScript(self):
        if not os.path.exists(self.script):
            scriptArgs = ['exec']     # without exec means terminate() won't work
            if self.args.VALGRIND:
                scriptArgs += ['valgrind']
                # args += ['--leak-check=full']
                # args += ['--track-origins=yes']
            scriptArgs.append(self.args.APP_NEW) #  + [str(p) for p in self.params]

            scriptArgs += '--db', self.dbName, '--skip-confirm'
            if self.args.SEND_CHAT:
                scriptArgs += '--send-chat', self.args.SEND_CHAT
            scriptArgs += '--localName', self.host
            scriptArgs += '--cmd-socket-name', self.cmdSocketName

            if self.args.WITH_MQTT:
                scriptArgs += [ '--mqtt-port', self.args.MQTT_PORT, '--mqtt-host', self.args.MQTT_HOST ]

            if self.args.WITH_SMS:
                scriptArgs += [ '--sms-number', self.smsNumber ]

            scriptArgs += ['--board-size', '15', '--sort-tiles']

            # useDupeMode = random.randint(0, 100) < self.args.DUP_PCT
            # if not useDupeMode: scriptArgs += ['--trade-pct', self.args.TRADE_PCT]
            
            # if self.devID: args.extend( ' '.split(self.devID))
            scriptArgs += [ '$*' ]

            with open( self.script, 'w' ) as fil:
                fil.write( "#!/bin/sh\n" )
                fil.write( ' '.join([str(arg) for arg in scriptArgs]) + '\n' )
            os.chmod(self.script, 0o755)

    @staticmethod
    def printStatus(statusSteps):
        Device._nSteps += 1
        print('.', end='', flush=True)
        if 0 == Device._nSteps % statusSteps:
            print()
            GameStatus.makeAll()
            for line in range(GameStatus.numLines()):
                print(GameStatus.line(line))

    @staticmethod
    def deviceFor(args, host):
        dev = Device._devs.get(host)
        if not dev:
            dev = Device(args, host)
            Device._devs[host] = dev
        return dev

    @staticmethod
    def playersIn(gid):
        return [dev.host for dev in Device.devsWith(gid)]

    @staticmethod
    # return all devices (up to 4 of them) that are host or guest in a
    # game with <gid>"""
    def devsWith(gid):
        result = [dev for dev in Device._devs.values() if dev.haveGame(gid)]
        return result

    @staticmethod
    def getAll():
        return [dev for dev in Device._devs.values()]

    @staticmethod
    def getFor(player):
        result = None
        for dev in Device.getAll():
            if dev.host == player:
                result = dev
                break;
        assert result
        return result

    def addGameWith(self, guests):
        # self._log('addGameWith({})'.format(guests))
        hosted = HostedInfo(guests)
        self.hostedGames.append(hosted)
        for guest in guests:
            Device.deviceFor(self.args, guest)    # in case this device never hosts

    def _log(self, msg):
        log(self.args, '{}: {}'.format(self.host, msg))

    def __str__(self):
        result = 'host: {}, devID: {}, with {} games: ' \
            .format(self.host, self.mqttDevID,
                    len(self.hostedGames)+len(self.guestGames))
        result += '{' + ', '.join(['{}'.format(game) for game in self._allGames()]) + '}'
        result += ' running={}'.format(self.endTime is not None)
        return result

    @staticmethod
    def runnerStub(self):
        self._launchProc()

def openOnExit(args):
    devs = Device.getAll()
    for dev in devs:
        appargs = [args.APP_NEW, '--db', dev.dbName]
        if args.WITH_MQTT:
            appargs += [ '--mqtt-port', args.MQTT_PORT, '--mqtt-host', args.MQTT_HOST ]
        subprocess.Popen([str(arg) for arg in appargs], stdout = subprocess.DEVNULL,
                         stderr = subprocess.DEVNULL, universal_newlines = True)

# Pick a game that's joined -- all invites accepted -- and call
# rematch on it. Return True if successful
def testRematch():
    for dev in Device.getAll():
        for gid, status in dev.gameStates.items():
            if 2 < status.get('nMoves', 0):
                dev.rematch(gid)
                return True
    return False

def mainLoop(args, devs):
    startCount = len(devs)

    startTime = datetime.datetime.now()
    nextStallCheck = startTime + datetime.timedelta(seconds = 20)
    rematchTested = False

    while 0 < len(devs):
        if gDone:
            print('gDone set; exiting loop')
            break
        dev = random.choice(devs)
        dev.step()
        if dev.finished():
            dev.quit()
            devs.remove(dev)
            log(args, 'removed dev for {}; {} devs left'.format(dev.host, len(devs)))

        if not rematchTested: rematchTested = testRematch()

        now = datetime.datetime.now()
        if devs and now > nextStallCheck:
            nextStallCheck = now + datetime.timedelta(seconds = 10)
            allStalled = True
            for dev in devs:
                if not dev.stalled():
                    allStalled = False
                    break
            if allStalled:
                log(args, 'exiting mainLoop with {} left (of {}) because all stalled' \
                    .format(len(devs), startCount))
                break

        if False and endTime < datetime.datetime.now():
            log(args, 'exiting mainLoop with {} left (of {}) because out of time' \
                .format(len(devs), startCount))
            break

        if not args.VERBOSE:
            Device.printStatus(args.STATUS_STEPS)

    for dev in devs:
        print('killing {}'.format(dev.host))
        dev.quit()

# We will build one Device for each player in the set of games, and
# prime each with enough information that when we start running them
# they can invite each other.
def build_devs(args):
    for ii in range(args.NGAMES):
        nPlayers = pick_ndevs(args)
        players = chooseNames(nPlayers)
        host = players[0]
        guests = players[1:]

        Device.deviceFor(args, host).addGameWith(guests)

    return Device.getAll()

def mkParser():
    parser = argparse.ArgumentParser()

    parser.add_argument('--status-steps', dest = 'STATUS_STEPS', type = int, default = 20,
                        help = 'how many steps between status dumps (matters only if not --debug)')

    parser.add_argument('--send-chat', dest = 'SEND_CHAT', type = str, default = None,
                        help = 'the message to send')

    parser.add_argument('--app-new', dest = 'APP_NEW', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll use')
    # parser.add_argument('--app-old', dest = 'APP_OLD', default = './obj_linux_memdbg/xwords',
    #                     help = 'the app we\'ll upgrade from')
    # parser.add_argument('--start-pct', dest = 'START_PCT', default = 50, type = int,
    #                     help = 'odds of starting with the new app, 0 <= n < 100')
    # parser.add_argument('--upgrade-pct', dest = 'UPGRADE_PCT', default = 20, type = int,
    #                     help = 'odds of upgrading at any launch, 0 <= n < 100')

    parser.add_argument('--num-games', dest = 'NGAMES', type = int, default = 1, help = 'number of games')
    parser.add_argument('--timeout-mins', dest = 'TIMEOUT_MINS', default = 10000, type = int,
                        help = 'minutes after which to timeout')
    # parser.add_argument('--nochange-secs', dest = 'NO_CHANGE_SECS', default = 30, type = int,
    #                     help = 'seconds without change after which to timeout')
    # parser.add_argument('--log-root', dest='LOGROOT', default = '.', help = 'where logfiles go')
    # parser.add_argument('--dup-packets', dest = 'DUP_PACKETS', default = False, action = 'store_true',
    #                     help = 'send all packet twice')
    # parser.add_argument('--phonies', dest = 'PHONIES', default = -1, type = int,
    #                     help = '0 (ignore), 1 (warn)) or 2 (lose turn); default is pick at random')
    # parser.add_argument('--make-phony-pct', dest = 'PHONY_PCT', default = 20, type = int,
    #                     help = 'how often a robot should play a phony (only applies when --phonies==2')
    parser.add_argument('--use-gtk', dest = 'USE_GTK', default = False, action = 'store_true',
                        help = 'run games using gtk instead of ncurses')

    # parser.add_argument('--dup-pct', dest = 'DUP_PCT', default = 0, type = int,
    #                     help = 'this fraction played in duplicate mode')

    # # 
    # #     echo "    [--clean-start]                                         \\" >&2
    parser.add_argument('--game-dict', dest = 'DICTS', action = 'append', default = [])
    # #     echo "    [--help]                                                \\" >&2
    # #     echo "    [--max-devs <int>]                                      \\" >&2
    parser.add_argument('--min-devs', dest = 'MINDEVS', type = int, default = 2,
                        help = 'No game will have fewer devices than this')
    parser.add_argument('--max-devs', dest = 'MAXDEVS', type = int, default = 4,
                        help = 'No game will have more devices than this')

    # parser.add_argument('--robots-all-same-iq', dest = 'IQS_SAME', default = False,
    #                     action = 'store_true', help = 'give all robots the same IQ')

    # parser.add_argument('--min-run', dest = 'MINRUN', type = int, default = 2,
    #                     help = 'Keep each run alive at least this many seconds')
    # #     echo "    [--new-app <path/to/app]                                \\" >&2
    # #     echo "    [--new-app-args [arg*]]  # passed only to new app       \\" >&2
    # #     echo "    [--num-rooms <int>]                                     \\" >&2
    # #     echo "    [--old-app <path/to/app]*                               \\" >&2
    # parser.add_argument('--one-per', dest = 'ONEPER', default = False,
    #                     action = 'store_true', help = 'force one player per device')
    # parser.add_argument('--resign-pct', dest = 'RESIGN_PCT', default = 0, type = int, \
    #                     help = 'Odds of resigning [0..100]')
    parser.add_argument('--seed', type = int, dest = 'SEED', default = 0)
    # #     echo "    [--send-chat <interval-in-seconds>                      \\" >&2
    # #     echo "    [--udp-incr <pct>]                                      \\" >&2
    # #     echo "    [--udp-start <pct>]      # default: $UDP_PCT_START                 \\" >&2
    # #     echo "    [--undo-pct <int>]                                      \\" >&2

    # parser.add_argument('--undo-pct', dest = 'UNDO_PCT', default = 0, type = int)
    # parser.add_argument('--trade-pct', dest = 'TRADE_PCT', default = 10, type = int)

    parser.add_argument('--with-sms', dest = 'WITH_SMS', action = 'store_true')
    parser.add_argument('--without-sms', dest = 'WITH_SMS', default = False, action = 'store_false')
    # parser.add_argument('--sms-fail-pct', dest = 'SMS_FAIL_PCT', default = 0, type = int)

    parser.add_argument('--with-mqtt', dest = 'WITH_MQTT', default = True, action = 'store_true')
    parser.add_argument('--without-mqtt', dest = 'WITH_MQTT', action = 'store_false')
    parser.add_argument('--mqtt-port', dest = 'MQTT_PORT', default = 1883 )
    parser.add_argument('--mqtt-host', dest = 'MQTT_HOST', default = 'localhost' )

    parser.add_argument('--force-tray', dest = 'TRAY_SIZE', default = 0, type = int,
                        help = 'Always this many tiles per tray')
    parser.add_argument('--board-size', dest = 'BOARD_SIZE', type = int, default = 15,
                        help = 'Use <n>x<n> size board')

    # parser.add_argument('--rematch-limit-secs', dest = 'REMATCH_SECS', type = int, default = 0,
    #                     help = 'rematch games that end within this many seconds of script launch')

    # envpat = 'DISCON_COREPAT'
    # parser.add_argument('--core-pat', dest = 'CORE_PAT', default = os.environ.get(envpat),
    #                     help = "pattern for core files that should stop the script " \
    #                     + "(default from env {})".format(envpat) )

    parser.add_argument('--with-valgrind', dest = 'VALGRIND', default = False,
                        action = 'store_true')

    parser.add_argument('--debug', dest='VERBOSE', default=False, action='store_true',
                        help='log stuff')
    parser.add_argument('--open-on-exit', dest = 'OPEN_ON_EXIT', default = False,
                        action = 'store_true', help='Open devs in gtk app when finished')

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
    if args.SEED: random.seed(args.SEED)
    # Hack: old files confuse things. Remove is simple fix good for now
    if args.WITH_SMS:
        try: rmtree('/tmp/xw_sms')
        except: None

    Device.setup()
    devs = build_devs(args)
    for dev in devs:
        dev.init()
        dev.launchIfNot()
    time.sleep(1)
    for dev in devs:
        if args.WITH_MQTT: dev.setDevID()
    for dev in devs:
        dev.makeGames()
        dev.quit()
    mainLoop(args, devs)

    if args.OPEN_ON_EXIT: openOnExit(args)

##############################################################################
if __name__ == '__main__':
    main()

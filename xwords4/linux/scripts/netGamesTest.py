#!/usr/bin/env python3

import argparse, datetime, glob, json, os, random, shutil, signal, \
    socket, struct, subprocess, sys, threading, time

g_ROOT_NAMES = ['Brynn', 'Ariela', 'Kati', 'Eric']
# These must correspond to what the linux app is looking for in roFromStr()
g_ROS = ['same', 'low_score_first', 'high_score_first', 'juggle',]
gDone = False
gGamesMade = 0
g_LOGFILE = None

def log(args, msg):
    now = datetime.datetime.strftime(datetime.datetime.now(), '%X.%f')
    msg = '{} {}'.format(now, msg)
    if args.VERBOSE:
        print(msg)
    global g_LOGFILE
    if g_LOGFILE:
        print(msg, file=g_LOGFILE)
        g_LOGFILE.flush()

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

def makeNames(nDevs):
    names = g_ROOT_NAMES[:nDevs]
    for ii in range(len(names), nDevs):
        newName = '{}{:02}'.format(g_ROOT_NAMES[ii%len(g_ROOT_NAMES)], ii//len(g_ROOT_NAMES))
        names += [newName]
    return names

def chooseNames(nPlayers):
    global g_NAMES
    players = g_NAMES[:]
    result = []
    for ii in range(nPlayers):
        indx = random.randint(0, len(players)-1)
        result.append(players.pop(indx))
    return result

class GameInfo():
    def __init__(self, device, gid, rematchLevel, parent, rematchOrder):
        self.device = device
        self.gid = gid
        self.state = {}
        assert isinstance(rematchLevel, int)
        self.rematchLevel = rematchLevel
        self.parent = parent
        self.rematchOrder = rematchOrder

    def setGid(self, gid):
        # set only once!
        assert 8 == len(gid) and not self.gid
        self.gid = gid

    def gameOver(self):
        return self.state.get('gameOver', False)

    def getDevice(self): return self.device

    def __str__(self):
        return 'gid: {}, parent: {}, ro: {}' \
            .format(self.gid, self.parent, self.rematchOrder)


class GuestGameInfo(GameInfo):
    def __init__(self, device, gid, rematchLevel, parent, rematchOrder):
        super().__init__(device, gid, rematchLevel, parent, rematchOrder)

class SoloGameInfo(GameInfo):
    def __init__(self, device, nRobots=0, **kwargs):
        super().__init__(device, kwargs.get('gid'), kwargs.get('rematchLevel'),
                         kwargs.get('parent'), kwargs.get('rematchOrder'))
        self.nRobots = nRobots  # only matters for creating games, not rematching
        global gGamesMade
        gGamesMade += 1

class HostGameInfo(GameInfo):
    def __init__(self, device, guestNames, **kwargs):
        super().__init__(device, kwargs.get('gid'), kwargs.get('rematchLevel'),
                         kwargs.get('parent'), kwargs.get('rematchOrder'))
        self.guestNames = guestNames
        self.needsInvite = kwargs.get('needsInvite', True)
        self.orderedPlayers = None
        global gGamesMade
        gGamesMade += 1

    def haveOrder(self): return self.orderedPlayers is not None

    def setOrder(self, orderedPlayers): self.orderedPlayers = orderedPlayers

    def __str__(self):
        return 'gid: {}, parent: {}, ro: {}, guests: {}' \
            .format(self.gid, self.parent, self.rematchOrder, self.guestNames)

class GameStatus():
    _statuses = None
    _prevLines = []
    _lastChange = datetime.datetime.now()
    _tileCount = 0

    def __init__(self, gid):
        self.gid = gid
        self.players = []
        self.allOver = True
        self.hostPlayerName = None
        self.hostName = None
        self.isSolo = False

    def harvest(self, dev, isSolo):
        # Sending lots of bogus invitations gets duplicates here
        if not dev.hostName in self.players:
            self.players.append(dev.hostName)
        self.allOver = self.allOver and dev.gameOver(self.gid)
        self.isSolo = isSolo

    def sortPlayers(self):
        game = Device.getHostGame(self.gid)
        orderedPlayers = isinstance(game, HostGameInfo) and game.orderedPlayers
        if orderedPlayers:
            assert len(orderedPlayers) == len(self.players)
            self.players = orderedPlayers
            self.hostName = game.getDevice().hostName

    # Build a gid->status map for each game, querying each device in
    # the game for details
    @staticmethod
    def makeAll():
        GameStatus._tileCount = 0
        statuses = {}
        for dev in Device.getAll():
            for game in dev._allGames():
                gid = game.gid
                assert 8 == len(gid)
                isSolo = isinstance(game, SoloGameInfo)
                if not gid in statuses: statuses[gid] = GameStatus(gid)
                statuses[gid].harvest(dev, isSolo)

        for status in statuses.values():
            status.sortPlayers()

        GameStatus._statuses = statuses

    @staticmethod
    def summary():
        now = datetime.datetime.strftime(datetime.datetime.now(), '%H:%M:%S')
        count = sum([1 for one in GameStatus._statuses.values() if one.allOver])
        return '{}; {} tiles; {} games; {} finished' \
            .format(now, GameStatus._tileCount, len(GameStatus._statuses), count)

    @staticmethod
    def numLines():
        maxPlayers = 0
        for status in GameStatus._statuses.values():
            nPlayers = len(status.players)
            if nPlayers > maxPlayers: maxPlayers = nPlayers
        return maxPlayers + 1

    # For all games, print the proper line of status for that game in
    # exactly 8 chars
    @staticmethod
    def line(indx):
        results = []
        for gid in sorted(GameStatus._statuses.keys()):
            status = GameStatus._statuses[gid]
            line = ''
            if indx == 0:
                line = gid
            elif indx <= len(status.players) and not status.allOver:
                player = status.players[indx-1]
                if status.isSolo: hostMarker = 's'
                elif status.hostName == player: hostMarker = '*'
                else: hostMarker = ' '
                initial = GameStatus._abbrev(player)
                dev = Device._devs.get(player)
                gameState = dev.gameFor(gid).state
                if gameState:
                    if gameState.get('gameOver', False):
                        initial = initial.lower()
                        arg3 = gameState.get('nPending', 0)
                    else:
                        arg3 = gameState.get('nTiles')
                        if arg3 > 0: GameStatus._tileCount += arg3
                    arg3 = 0 <= arg3 and '{: 3}'.format(arg3) or '-'.rjust(3)
                    line = '{}{:3}{}'.format(hostMarker, initial, arg3)
            results.append(line.center(len(gid)))

        return ' '.join(results)

    @staticmethod
    def _abbrev(name):
        for base in g_ROOT_NAMES:
            if name.startswith(base):
                return name[0] + name.strip(base)

class Device():
    _devs = {}
    _logdir = None
    _nSteps = 0
    _nextChatID = 0
    _kps = {}
    _stats = None

    @staticmethod
    def setup(logdir):
        Device._logdir = logdir

    def __init__(self, args, hostName):
        self.args = args
        self.app = None
        self.endTime = None
        self.mqttDevID = None
        self.smsNumber = args.WITH_SMS and '{}_phone'.format(hostName) or None
        self.hostName = hostName
        self.hostedGames = []       # array of HostGameInfo for each game I host
        self.guestGames = []
        self.script = '{}/{}.sh'.format(Device._logdir, hostName)
        self.dbName = '{}/{}.db'.format(Device._logdir, hostName)
        self.logfile = '{}/{}_log.txt'.format(Device._logdir, hostName)
        self.cmdSocketName = '{}/{}.sock'.format(Device._logdir, hostName)
        self._keyCur = 10000 * (1 + g_NAMES.index(hostName))

    def init(self):
        self._checkScript()

    def _getApp(self):
        # first time?
        if not self.app:
            pct = random.randint(0,99)
            if pct < self.args.OLD_START_PCT:
                self.app = self.args.APP_OLD
            else:
                self.app = self.args.APP_NEW
        # upgrade time?
        elif not self.app == self.args.APP_NEW:
            pct = random.randint(0,99)
            if pct < self.args.UPGRADE_PCT:
                self._log('upgrading app')
                self.app = self.args.APP_NEW

        return self.app

    def stats(self): return self._stats

    # called by thread proc
    def _launchProc(self):
        assert not self.endTime
        self.endTime = datetime.datetime.now() + \
            datetime.timedelta(seconds = self.args.MIN_APP_LIFE)
        args = [ self.script, '--close-stdin', '--skip-user-errors' ]
        if not self.args.USE_GTK: args.append('--curses')

        env = os.environ.copy()
        env['APP'] = self._getApp()

        with open( self.logfile, 'a' ) as logfile:
            subprocess.run(args, env=env, stdout = subprocess.DEVNULL,
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
            tryTrade = random.randint(0, 99) < self.args.TRADE_PCT
            response = self._sendWaitReply('moveIf', gid=gid, tryTrade=tryTrade)
            moved = response.get('success', False)
            if moved:
                response = self._sendWaitReply('getKPs', gid=gid)
                self.checkKPs(response.get('kps'))
                break
        return moved

    def sendChat(self):
        success = False
        if random.randint(0, 99) < self.args.CHAT_PCT:
            gid = self._pickGid()
            if gid:
                response = self._sendWaitReply('sendChat', gid=gid,
                                               msg=Device.nextChatMsg(self.hostName))
                success = response.get('success', False)
        return success

    def postUndo(self):
        success = False
        if random.randint(0, 999) < self.args.UNDO_PCT:
            gid = self._pickGid()
            if gid:
                response = self._sendWaitReply('undoMove', gid=gid)
                success = response.get('success', False)
        return success

    def postResign(self):
        success = False
        if random.randint(0, 999) < self.args.RESIGN_PCT:
            gid = self._pickGid()
            if gid:
                response = self._sendWaitReply('resign', gid=gid)
                success = response.get('success', False)
        return success

    def checkKPs(self, kps):
        if kps:
            for kp in kps:
                devID = kp.get('devID')
                if not devID in Device._kps: Device._kps[devID] = set()
                names = Device._kps[devID]

                name = kp.get('name')
                if name not in names and len(names):
                    print('adding {} to {} for {}' \
                          .format(name, names, Device._kps.get(devID)))
                names.add(name)

    def _pickGid(self):
        result = None
        gids = [game.gid for game in self._allGames()]
        if gids:
            random.shuffle(gids)
            result = gids[0]
        return result

    def _sendWaitReply(self, cmd, **kwargs):
        self.launchIfNot()

        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        # print('connecting to: {}'.format(self.cmdSocketName))

        client.connect(self.cmdSocketName)

        key = self._nextKey()
        params = [{'cmd': cmd, 'key': key, 'args': {**kwargs}}]
        payload = json.dumps(params).encode()
        client.sendall(payload)

        # # Receive a response from the server
        # self._log('_sendWaitReply({}): calling recv()'.format(cmd))
        response = client.recv(8*1024).decode()
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
        mqttDevID = response and response.get('mqtt')
        if mqttDevID:
            self.mqttDevID = mqttDevID
        else:
            printError('no mqtt or no response')

    def makeHostGames(self):
        args = self.args
        for game in self.hostedGames:
            isSolo = isinstance(game, SoloGameInfo)
            nPlayers = 1 + (isSolo and game.nRobots or len(game.guestNames))
            hostPosn = random.randint(0, nPlayers-1)
            traySize = 0 == args.TRAY_SIZE and random.randint(7, 9) or args.TRAY_SIZE
            boardSize = random.choice(range(args.BOARD_SIZE_MIN, args.BOARD_SIZE_MAX+1, 2))
            allowSub7 = random.randint(0, 99) < self.args.SUB7_TRADES_PCT
            timerSeconds = self.args.TIMER_SECS
            response = self._sendWaitReply('makeGame', nPlayers=nPlayers,
                                           hostPosn=hostPosn, dict=args.DICTS[0],
                                           boardSize=boardSize, traySize=traySize,
                                           isSolo=isSolo, allowSub7=allowSub7,
                                           timerSeconds=timerSeconds)
            newGid = response.get('newGid')
            if newGid:
                game.setGid(newGid)

    # This is the heart of things. Do something as long as we have a
    # game that needs to run.
    def step(self):
        # self._log('step() called for {}'.format(self))
        stepped = False
        for game in self.hostedGames:
            if isinstance(game, HostGameInfo) and game.needsInvite:
                self.invite(game)
                stepped = True
                break

        if not stepped:
            if not self.endTime:
                self.launchIfNot()
            elif datetime.datetime.now() > self.endTime:
                self.rematchOrQuit()
            elif self.sendChat():
                pass
            elif self.postUndo():
                pass
            elif self.postResign():
                pass
            elif self.moveOne():
                pass
            else:
                # self._log('sleeping with {} to go'.format(self.endTime-now))
                time.sleep(0.5)
            stepped = True

    # I may be a guest or a host in this game. Rematch works either
    # way. But how I figure out the other players differs.
    def rematch(self, game):
        gid = game.gid
        rematchOrder = self.figureRematchOrder()
        newGid = self._sendWaitReply('rematch', gid=gid, rematchOrder=rematchOrder) \
                     .get('newGid')
        if newGid:
            guests = Device.playersIn(gid)
            guests.remove(self.hostName)
            self._log('rematch: new host: {}; new guest[s]: {}, gid: {}' \
                      .format(self.hostName, guests, newGid))

            rematchLevel = game.rematchLevel - 1
            assert rematchLevel >= 0 # fired

            if isinstance(game, SoloGameInfo):
                newGame = SoloGameInfo(self, rematchLevel=rematchLevel, gid=newGid,
                                       parent=gid, rematchOrder=rematchOrder)
            else:
                newGame = HostGameInfo(self, guests, needsInvite=False, gid=newGid,
                                       rematchLevel=rematchLevel, parent=gid,
                                       rematchOrder = rematchOrder)

            self.hostedGames.append(newGame)
            for guest in guests:
                Device.getForPlayer(guest) \
                      .expectInvite(newGid, rematchLevel, gid, rematchOrder)

    # inviting means either causing host to send an in-game invitation
    # (the way rematch works) or causing guest to register (as happens
    # when email or SMS is used for invitations.)
    def invite(self, game):
        doOOB = random.randint(0, 99) < self.args.OOB_PCT
        if doOOB: self.inviteOutOfBand(game)
        else: self.inviteInBand(game)

    def inviteOutOfBand(self, game):
        # For each invitee, we need to make sure it exists, launch it,
        # and then send it the equivalent of an emailed invitation.

        nPlayersT = 1 + len(game.guestNames)
        for name in game.guestNames:
            invitee = Device.getForPlayer(name)

            invitee.launchIfNot()
            response = invitee._sendWaitReply('inviteRcvd', gid=game.gid,
                                              nPlayersT=nPlayersT,
                                              dict = self.args.DICTS[0],
                                              addr = self._mkAddr(self),
                                              )
            if response['success']:
                invitee.expectInvite(game.gid, game.rematchLevel)

        game.needsInvite = False


    def inviteInBand(self, game):
        remotes = []
        guestDevs = []
        useRandomDevID = random.randint(0, 100) < self.args.BAD_INVITE_PCT
        for ii in reversed(range(len(game.guestNames))):
            guestDev = Device.getForPlayer(game.guestNames[ii])
            guestDevs.append(guestDev)

            addr = self._mkAddr(guestDev, useRandomDevID)
            remotes.append({'addr': addr})

        response = self._sendWaitReply('invite', gid=game.gid,
                                       remotes=remotes)

        if response['success']:
            for guestDev in guestDevs:
                guestDev.expectInvite(game.gid, game.rematchLevel)
            game.needsInvite = useRandomDevID

    def _mkAddr(self, dev, useRandomDevID=False):
        addr = {}
        if self.args.WITH_MQTT:
            if useRandomDevID:
                mqttAddr = '{:016X}'.format(random.randint(1, 0x7FFFFFF))
            else:
                mqttAddr = dev.mqttDevID
            addr['mqtt'] = mqttAddr
        if self.args.WITH_SMS: addr['sms'] = dev.smsNumber
        return addr

    def expectInvite(self, gid, rematchLevel, parent=None, rematchOrder = None):
        self.guestGames.append(GuestGameInfo(self, gid, rematchLevel, parent, rematchOrder))
        self.launchIfNot()

    def figureRematchOrder(self):
        ro = self.args.REMATCH_ORDER
        if not ro: ro = random.choice(g_ROS)
        return ro

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
        gameState = self.gameFor(gid).state
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

    def gameFor(self, gid):
        for game in self._allGames():
            if gid == game.gid:
                return game

    def rematchOrQuit(self):
        if self.endTime:
            gids = [game.gid for game in self._allGames() if not self.gameOver(game.gid)]

            orders = []
            for gid in gids:
                game = Device.getHostGame(gid)
                if game and not game.haveOrder():
                    orders.append(gid)

            # PENDING. Don't print this, but include in summary on exit
            response = self._sendWaitReply('stats')
            self._addStats(response)

            response = self \
                ._sendWaitReply('getStates', gids=gids, orders=orders)

            for order in response.get('orders', []):
                gid = order.get('gid')
                game = Device.getHostGame(gid)
                assert isinstance(game, HostGameInfo) # firing
                game.setOrder(order.get('players'))

            anyRematched = False
            for obj in response.get('states', []):
                game = self.gameFor(obj.get('gid'))
                game.state = obj

                if game.gameOver() and 0 < game.rematchLevel:
                    if random.randint(0, 100) < self.args.REMATCH_PCT:
                        self.rematch(game)
                        anyRematched = True
                    game.rematchLevel = 0 # so won't be used again

            if not anyRematched:
                response = self._sendWaitReply('quit')
                self.watcher.join()
                self.watcher = None
                assert not self.endTime

    def quit(self):
        if self.endTime:
            allGames = self._allGames()
            gids = [game.gid for game in allGames if not self.gameOver(game.gid)]
            response = self._sendWaitReply('quit', gids=gids)

            for obj in response.get('states', []):
                self.gameFor(obj.get('gid')).state = obj

            # wait for the thing to actually die
            self.watcher.join()
            self.watcher = None
            assert not self.endTime

    def _addStats(self, stats):
        stats = stats.get('stats').get('stats')
        if stats and self._stats:
            for key in stats.keys():
                if key in self._stats:
                    assert self._stats[key] <= stats[key]
        self._stats = stats

    def _checkScript(self):
        assert os.path.exists(self.args.APP_NEW)
        if not os.path.exists(self.script):
            scriptArgs = ['exec']     # without exec means terminate() won't work
            if self.args.VALGRIND:
                scriptArgs += ['valgrind']
                # args += ['--leak-check=full']
                # args += ['--track-origins=yes']

            scriptArgs.append('"${APP}"')

            scriptArgs += '--db', self.dbName, '--skip-confirm'
            scriptArgs += '--localName', self.hostName
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
                fil.write('#!/bin/bash\n' )
                fil.write('APP="${{APP:-{}}}"\n'.format(self.args.APP_NEW))
                fil.write(' '.join([str(arg) for arg in scriptArgs]) + '\n')
            os.chmod(self.script, 0o755)

    @staticmethod
    def printStatus(statusSteps, noPrint):
        Device._nSteps += 1
        noPrint or print('.', end='', flush=True)
        if 0 == Device._nSteps % statusSteps:
            GameStatus.makeAll()

            lines = [GameStatus.line(ii) for ii in range(GameStatus.numLines())]
            now = datetime.datetime.now()
            if lines == GameStatus._prevLines:
                since = str(now - GameStatus._lastChange).split('.')[0]
                noPrint or print(' no change in {}'.format(since))
            else:
                if not noPrint:
                    print(' ' + GameStatus.summary())
                    for line in lines: print(line)
                GameStatus._prevLines = lines
                GameStatus._lastChange = now
        return GameStatus._lastChange

    @staticmethod
    def deviceFor(args, hostName):
        dev = Device._devs.get(hostName)
        if not dev:
            dev = Device(args, hostName)
            Device._devs[hostName] = dev
        return dev

    @staticmethod
    def playersIn(gid):
        return [dev.hostName for dev in Device.devsWith(gid)]

    @staticmethod
    def printLiveGames():
        for dev in Device.getAll():
            for game in dev._allGames():
                if not game.gameOver() and not isinstance(game, GuestGameInfo):
                    print('game: {}'.format(game))

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
    def getForPlayer(player):
        result = None
        for dev in Device.getAll():
            # print('getForPlayer(); found {}'.format(dev.hostName))
            if dev.hostName == player:
                result = dev
                break
        assert result
        return result

    @staticmethod
    def getHostGame(gid):
        devs = Device.devsWith(gid)
        for dev in devs:
            game = dev.gameFor(gid)
            if isinstance(game, HostGameInfo):
                return game
        return None

    def addSoloGame(self, nRobots):
        self.hostedGames \
            .append(SoloGameInfo(self, nRobots,
                                 rematchLevel=self.args.REMATCH_LEVEL))

    def addGameWith(self, guests):
        self.hostedGames.append(HostGameInfo(self, guests, needsInvite=True,
                                             rematchLevel=self.args.REMATCH_LEVEL))
        for guest in guests:
            Device.deviceFor(self.args, guest)    # in case this device never hosts

    def _log(self, msg):
        log(self.args, '{}: {}'.format(self.hostName, msg))

    def __str__(self):
        result = 'host: {}, devID: {}, with {} games: ' \
            .format(self.hostName, self.mqttDevID,
                    len(self.hostedGames)+len(self.guestGames))
        result += '{' + ', '.join(['{}'.format(game) for game in self._allGames()]) + '}'
        result += ' running={}'.format(self.endTime is not None)
        return result

    @staticmethod
    def runnerStub(self):
        self._launchProc()

    @staticmethod
    def nextChatMsg(sender):
        Device._nextChatID += 1
        return 'chat msg {}: Hi, this is {}'.format(Device._nextChatID, sender)

def openOnExit(args):
    devs = Device.getAll()
    for dev in devs:
        appargs = [args.APP_NEW, '--db', dev.dbName]
        if args.WITH_MQTT:
            appargs += [ '--mqtt-port', args.MQTT_PORT, '--mqtt-host', args.MQTT_HOST ]
        subprocess.Popen([str(arg) for arg in appargs], stdout = subprocess.DEVNULL,
                         stderr = subprocess.DEVNULL, universal_newlines = True)


def countCores(args):
    count = 0
    if args.CORE_PAT:
        count = len( glob.glob(args.CORE_PAT) )
    return count

def printKPs():
    kps = Device._kps
    for names in kps.values():
        assert 1 == len(names)
    print('Known players: {}'.format(kps))

def printStats():
    for dev in Device.getAll():
        print('stats for {}: {}'.format(dev.hostName, dev.stats()))

def mainLoop(args, devs):
    startCount = len(devs)
    nCores = countCores(args)

    startTime = datetime.datetime.now()
    nextStallCheck = startTime + datetime.timedelta(seconds = 20)

    while 0 < len(devs):
        if gDone:
            print('gDone set; exiting loop')
            break
        elif nCores < countCores(args):
            print('core file count increased; exiting')
            break

        dev = random.choice(devs)
        dev.step()
        if dev.finished():
            dev.quit()
            devs.remove(dev)
            log(args, 'removed dev for {}; {} devs left'.format(dev.hostName, len(devs)))

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

        lastChange = Device.printStatus(args.STATUS_STEPS, args.VERBOSE)
        if now - lastChange > datetime.timedelta(seconds=args.NO_CHANGE_SECS):
            print('exiting mainLoop because no change in {} seconds' \
                .format(args.NO_CHANGE_SECS))
            break

    # list info about any remaining games
    print('games left: ...')
    Device.printLiveGames()

    # kill anybody left alive
    for dev in devs:
        print('killing {}'.format(dev.hostName))
        dev.quit()

# We will build one Device for each player in the set of games, and
# prime each with enough information that when we start running them
# they can invite each other.
def build_devs(args):
    global g_NAMES
    g_NAMES = makeNames(args.NDEVS)
    for ii in range(args.NGAMES):
        nPlayers = pick_ndevs(args)
        players = chooseNames(nPlayers)
        host = players[0]
        guests = players[1:]

        dev = Device.deviceFor(args, host)
        if random.randint(0, 100) < args.SOLO_PCT:
            dev.addSoloGame(len(guests))
        else:
            dev.addGameWith(guests)

    return Device.getAll()

def mkParser():
    # formatter_class gets us (default=<something>) added to help, but
    # only if help= is present
    parser = argparse \
        .ArgumentParser(formatter_class = argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('--status-steps', dest = 'STATUS_STEPS', type = int, default = 20,
                        help = 'how many steps between status dumps (matters only if not --debug)')

    parser.add_argument('--chat-pct', dest = 'CHAT_PCT', type = int, default = 0,
                        help = 'odds of sending a chat message')

    parser.add_argument('--app-new', dest = 'APP_NEW', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll use')
    parser.add_argument('--app-old', dest = 'APP_OLD', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll upgrade from')
    parser.add_argument('--old-start-pct', dest = 'OLD_START_PCT', default = 90, type = int,
                        help = 'odds of starting with the old app, 0 <= n < 100')
    parser.add_argument('--upgrade-pct', dest = 'UPGRADE_PCT', default = 0, type = int,
                        help = 'odds of upgrading at any launch, 0 <= n < 100')

    parser.add_argument('--solo-pct', dest = 'SOLO_PCT', default = 20, type = int,
                        help = 'odds a game will be standalone')

    parser.add_argument('--num-games', dest = 'NGAMES', type = int, default = 1, help = 'number of games')
    parser.add_argument('--num-devs', dest = 'NDEVS', type = int, default = len(g_ROOT_NAMES), help = 'number of devices')
    parser.add_argument('--timeout-mins', dest = 'TIMEOUT_MINS', default = 10000, type = int,
                        help = 'minutes after which to timeout')
    parser.add_argument('--nochange-secs', dest = 'NO_CHANGE_SECS', default = 45, type = int,
                        help = 'seconds without change after which to timeout')
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
    parser.add_argument('--resign-pct', dest = 'RESIGN_PCT', default = 0, type = int, \
                        help = 'Odds of resigning [0..999]')
    parser.add_argument('--seed', type = int, dest = 'SEED', default = 0)
    # #     echo "    [--send-chat <interval-in-seconds>                      \\" >&2
    # #     echo "    [--udp-incr <pct>]                                      \\" >&2
    # #     echo "    [--udp-start <pct>]      # default: $UDP_PCT_START                 \\" >&2
    # #     echo "    [--undo-pct <int>]                                      \\" >&2

    parser.add_argument('--undo-pct', dest = 'UNDO_PCT', default = 0, type = int,
                        help='odds (0 <= n <= 1000) of undoing, per second')
    parser.add_argument('--trade-pct', dest = 'TRADE_PCT', default = 10, type = int,
                        help='what percent of moves will trade tiles')
    parser.add_argument('--sub7-trades-pct', dest = 'SUB7_TRADES_PCT', default = 10, type=int)

    parser.add_argument('--bad-invite-pct', dest = 'BAD_INVITE_PCT', default = 0, type=int,
                        help='What pct (0..99) of MQTT invitations will be to non-existant devices')
    parser.add_argument('--oob-invite-pct', dest = 'OOB_PCT', default = 50, type=int,
                        help='What pct (0..99) of guests will "emailed" rather than comms-invited')

    parser.add_argument('--timer-seconds', dest='TIMER_SECS', default=10, type=int,
                        help='Enable game timer with game this many seconds long')
    parser.add_argument('--min-app-life', dest='MIN_APP_LIFE', default=15, type=int,
                        help='Minimum number of seconds app will run after each launch')

    parser.add_argument('--with-sms', dest = 'WITH_SMS', action = 'store_true')
    parser.add_argument('--without-sms', dest = 'WITH_SMS', default = False, action = 'store_false')
    # parser.add_argument('--sms-fail-pct', dest = 'SMS_FAIL_PCT', default = 0, type = int)

    parser.add_argument('--with-mqtt', dest = 'WITH_MQTT', default = True, action = 'store_true')
    parser.add_argument('--without-mqtt', dest = 'WITH_MQTT', action = 'store_false')
    parser.add_argument('--mqtt-port', dest = 'MQTT_PORT', default = 1883 )
    parser.add_argument('--mqtt-host', dest = 'MQTT_HOST', default = 'localhost' )

    parser.add_argument('--force-tray', dest = 'TRAY_SIZE', default = 0, type = int,
                        help = 'Always this many tiles per tray')
    parser.add_argument('--board-size-min', dest = 'BOARD_SIZE_MIN', type = int, default = 15,
                        help = 'give boards at least this many rows and columns')
    parser.add_argument('--board-size-max', dest = 'BOARD_SIZE_MAX', type = int, default = 15,
                        help = 'give boards no more than this many rows and columns')

    parser.add_argument('--rematch-level', dest = 'REMATCH_LEVEL', type = int, default = 0,
                        help = 'rematch games down to this ancestry/depth')
    parser.add_argument('--rematch-order', dest = 'REMATCH_ORDER', type = str, default = None,
                        help = 'order rematched games one of these ways: {}'.format(g_ROS))
    parser.add_argument('--rematch-pct', dest = 'REMATCH_PCT', type = int, default = 20,
                        help = 'what percent of players will rematch at game end')

    envpat = 'DISCON_COREPAT'
    parser.add_argument('--core-pat', dest = 'CORE_PAT', default = os.environ.get(envpat),
                        help = "pattern for core files that should stop the script " \
                        + "(default from env {})".format(envpat) )

    parser.add_argument('--with-valgrind', dest = 'VALGRIND', default = False,
                        action = 'store_true')

    parser.add_argument('--debug', dest='VERBOSE', default=False, action='store_true',
                        help='log stuff')
    parser.add_argument('--open-on-exit', dest = 'OPEN_ON_EXIT', default = False,
                        action = 'store_true', help='Open devs in gtk app when finished')
    parser.add_argument('--no-reuse-dbs', dest = 'REUSE_DBS', default = True,
                        action = 'store_false', help='Don\'t reuse device dbs on startup')

    return parser

def parseArgs():
    args = mkParser().parse_args()
    assignDefaults(args)
    print(args)
    return args
    # print(options)

def assignDefaults(args):
    if len(args.DICTS) == 0: args.DICTS.append('CollegeEng_2to8.xwd')
    assert 1 == (args.BOARD_SIZE_MAX % 2)
    assert 1 == (args.BOARD_SIZE_MIN % 2)
    assert args.BOARD_SIZE_MAX >= args.BOARD_SIZE_MIN
    assert args.BOARD_SIZE_MIN >= 11
    assert args.BOARD_SIZE_MAX <= 23

def termHandler(signum, frame):
    global gDone
    print('termHandler() called')
    gDone = True

def printError(msg): print( 'ERROR: {}'.format(msg))

def initLogs(args):
    scriptName = os.path.splitext(os.path.basename(sys.argv[0]))[0]
    statedir = scriptName + '_state'
    if not args.REUSE_DBS and os.path.exists(statedir):
        shutil.move(statedir, '/tmp/{}_{}'.format(statedir, os.getpid()))
    if not os.path.exists(statedir): os.mkdir(statedir)

    logfilepath = '{}/{}_logs.txt'.format(statedir, scriptName)
    global g_LOGFILE
    g_LOGFILE = open(logfilepath, 'w')

    return statedir

def main():
    startTime = datetime.datetime.now()
    signal.signal(signal.SIGINT, termHandler)

    args = parseArgs()
    statedir = initLogs(args)

    if args.SEED: random.seed(args.SEED)
    # Hack: old files confuse things. Remove is simple fix good for now
    if args.WITH_SMS:
        try: rmtree('/tmp/xw_sms')
        except: None

    Device.setup(statedir)       # deletes old logdif
    devs = build_devs(args)
    for dev in devs:
        dev.init()
        dev.launchIfNot()
    time.sleep(1)
    for dev in devs:
        if args.WITH_MQTT: dev.setDevID()
    for dev in devs:
        dev.makeHostGames()
        dev.quit()
    mainLoop(args, devs)

    printKPs()
    printStats()


    elapsed = str(datetime.datetime.now() - startTime).split('.')[0]
    print('played {} games in {}'.format(gGamesMade, elapsed))

    if g_LOGFILE: g_LOGFILE.close()

    if args.OPEN_ON_EXIT: openOnExit(args)

##############################################################################
if __name__ == '__main__':
    main()

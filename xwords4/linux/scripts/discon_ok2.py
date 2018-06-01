#!/usr/bin/env python3

import re, os, sys, getopt, shutil, threading, requests, json, glob
import argparse, datetime, random, signal, subprocess, time

# LOGDIR=./$(basename $0)_logs
# APP_NEW=""
# DO_CLEAN=""
# APP_NEW_PARAMS=""
# NGAMES = 1
g_UDP_PCT_START = 100
gDeadLaunches = 0
# UDP_PCT_INCR=10
# UPGRADE_ODDS=""
# NROOMS=""
# HOST=""
# PORT=""
# TIMEOUT=""
# SAVE_GOOD=""
# MINDEVS=""
# MAXDEVS=""
# ONEPER=""
# RESIGN_PCT=0
g_DROP_N=0
# MINRUN=2		                # seconds
# ONE_PER_ROOM=""                 # don't run more than one device at a time per room
# USE_GTK=""
# UNDO_PCT=0
# ALL_VIA_RQ=${ALL_VIA_RQ:-FALSE}
# SEED=""
# BOARD_SIZES_OLD=(15)
# BOARD_SIZES_NEW=(15)
g_NAMES = [None, 'Brynn', 'Ariela', 'Kati', 'Eric']
# SEND_CHAT=''
# CORE_COUNT=$(ls core.* 2>/dev/null | wc -l)
# DUP_PACKETS=''
# HTTP_PCT=0

# declare -A PIDS
# declare -A APPS
# declare -A NEW_ARGS
# declare -a ARGS
# declare -A ARGS_DEVID
# declare -A ROOMS
# declare -A FILES
# declare -A LOGS
# declare -A MINEND
# ROOM_PIDS = {}
# declare -a APPS_OLD=()
# declare -a DICTS=				# wants to be =() too?
# declare -A CHECKED_ROOMS

# function cleanup() {
#     APP="$(basename $APP_NEW)"
#     while pidof $APP; do
#         echo "killing existing $APP instances..."
#         killall -9 $APP
#         sleep 1
#     done
#     echo "cleaning everything up...."
#     if [ -d $LOGDIR ]; then
#         mv $LOGDIR /tmp/${LOGDIR}_$$
#     fi
#     if [ -e $(dirname $0)/../../relay/xwrelay.log ]; then
#         mkdir -p /tmp/${LOGDIR}_$$
#         mv $(dirname $0)/../../relay/xwrelay.log /tmp/${LOGDIR}_$$
#     fi

#     echo "DELETE FROM games WHERE room LIKE 'ROOM_%';" | psql -q -t xwgames
#     echo "DELETE FROM msgs WHERE NOT devid in (SELECT unnest(devids) from games);" | psql -q -t xwgames
# }

# function connName() {
#     LOG=$1
#     grep -a 'got_connect_cmd: connName' $LOG | \
#         tail -n 1 | \
#         sed 's,^.*connName: \"\(.*\)\" (reconnect=.)$,\1,'
# }

# function check_room() {
#     ROOM=$1
#     if [ -z ${CHECKED_ROOMS[$ROOM]:-""} ]; then
#         NUM=$(echo "SELECT COUNT(*) FROM games "\
#             "WHERE NOT dead "\
#             "AND ntotal!=sum_array(nperdevice) "\
#             "AND ntotal != -sum_array(nperdevice) "\
#             "AND room='$ROOM'" |
#             psql -q -t xwgames)
#         NUM=$((NUM+0))
#         if [ "$NUM" -gt 0 ]; then
#             echo "$ROOM in the DB has unconsummated games.  Remove them."
#             exit 1
#         else
#             CHECKED_ROOMS[$ROOM]=1
#         fi
#     fi
# }

# print_cmdline() {
#     local COUNTER=$1
#     local LOG=${LOGS[$COUNTER]}
#     echo -n "New cmdline: " >> $LOG
#     echo "${APPS[$COUNTER]} ${NEW_ARGS[$COUNTER]} ${ARGS[$COUNTER]}" >> $LOG
# }

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
            PARAMS += ['--robot',  g_NAMES[NAME_INDX], '--robot-iq', str(random.randint(1,100))]
            NLOCALS -= 1
            NAME_INDX += 1
        elif 0 < NREMOTES:
            PARAMS += ['--remote-player']
            NREMOTES -= 1
    return PARAMS

def logReaderStub(dev): dev.logReaderMain()

class Device():
    sConnnameMap = {}
    sHasLDevIDMap = {}
    sConnNamePat = re.compile('.*got_connect_cmd: connName: "([^"]+)".*$')
    sGameOverPat = re.compile('.*\[unused tiles\].*')
    sTilesLeftPoolPat = re.compile('.*pool_removeTiles: (\d+) tiles left in pool')
    sTilesLeftTrayPat = re.compile('.*player \d+ now has (\d+) tiles')
    sRelayIDPat = re.compile('.*UPDATE games.*seed=(\d+),.*relayid=\'([^\']+)\'.*')
    
    def __init__(self, args, game, indx, app, params, room, db, log, nInGame):
        self.game = game
        self.indx = indx
        self.args = args
        self.pid = 0
        self.gameOver = False
        self.app = app
        self.params = params
        self.room = room
        self.db = db
        self.logPath = log
        self.nInGame = nInGame
        # runtime stuff; init now
        self.proc = None
        self.connname = None
        self.devID = ''
        self.launchCount = 0
        self.allDone = False    # when true, can be killed
        self.nTilesLeftPool = None
        self.nTilesLeftTray = None
        self.relayID = None
        self.relaySeed = 0
        self.locked = False

        with open(self.logPath, "w") as log:
            log.write('New cmdline: ' + self.app + ' ' + (' '.join([str(p) for p in self.params])))
            log.write(os.linesep)

    def logReaderMain(self):
        assert self and self.proc
        stdout, stderr = self.proc.communicate()
        # print('logReaderMain called; opening:', self.logPath, 'flag:', flag)
        nLines = 0
        with open(self.logPath, 'a') as log:
            for line in stderr.splitlines():
                nLines += 1
                log.write(line + os.linesep)

                self.locked = True

                # check for connname
                if not self.connname:
                    match = Device.sConnNamePat.match(line)
                    if match:
                        self.connname = match.group(1)
                        if not self.connname in Device.sConnnameMap:
                            Device.sConnnameMap[self.connname] = set()
                        Device.sConnnameMap[self.connname].add(self)

                # check for game over
                if not self.gameOver:
                    match = Device.sGameOverPat.match(line)
                    if match: self.gameOver = True

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

                self.locked = False

        # print('logReaderMain done, wrote lines:', nLines, 'to', self.logPath);

    def launch(self):
        args = []
        if self.args.VALGRIND:
            args += ['valgrind']
            # args += ['--leak-check=full']
            # args += ['--track-origins=yes']
        args += [self.app] + [str(p) for p in self.params]
        if self.devID: args.extend( ' '.split(self.devID))
        self.launchCount += 1
        # self.logStream = open(self.logPath, flag)
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
        else:
            print('NOT killing')
        self.proc = None
        self.check_game()

    def handleAllDone(self):
        global gDeadLaunches
        if self.allDone:
            self.moveFiles()
            self.send_dead()
            gDeadLaunches += self.launchCount
        return self.allDone

    def moveFiles(self):
        assert not self.running()
        shutil.move(self.logPath, self.args.LOGDIR + '/done')
        shutil.move(self.db, self.args.LOGDIR + '/done')

    def send_dead(self):
        JSON = json.dumps([{'relayID': self.relayID, 'seed': self.relaySeed}])
        url = 'http://%s/xw4/relay.py/kill' % (self.args.HOST)
        params = {'params' : JSON}
        try:
            req = requests.get(url, params = params) # failing
        except requests.exceptions.ConnectionError:
            print('got exception sending to', url, params, '; is relay.py running as apache module?')

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
            print('found --ldevid:', hasLDevID);
            Device.sHasLDevIDMap[self.app] = hasLDevID

        if Device.sHasLDevIDMap[self.app]:
            RNUM = random.randint(0, 99)
            if not self.devID:
                if RNUM < 30:
                    self.devID = '--ldevid LINUX_TEST_%.5d_' % (self.indx)
            elif RNUM < 10:
                self.devID += 'x'

    def check_game(self):
        if self.gameOver and not self.allDone:
            allDone = False
            if len(Device.sConnnameMap[self.connname]) == self.nInGame:
                allDone = True
                for dev in Device.sConnnameMap[self.connname]:
                    if dev == self: continue
                    if not dev.gameOver:
                        allDone = False
                        break

                if allDone:
                    for dev in Device.sConnnameMap[self.connname]:
                        assert self.game == dev.game
                        dev.allDone = True

            # print('Closing', self.connname, datetime.datetime.now())
            # for dev in Device.sConnnameMap[self.connname]:
            #     dev.kill()
#         # kill_from_logs $OTHERS $KEY
#         for ID in $OTHERS $KEY; do
#             echo -n "${ID}:${LOGS[$ID]}, "
#             kill_from_log ${LOGS[$ID]} || /bin/true
# 			send_dead $ID
#             close_device $ID $DONEDIR "game over"
#         done
#         echo ""
#         # XWRELAY_ERROR_DELETED may be old
#     elif grep -aq 'relay_error_curses(XWRELAY_ERROR_DELETED)' $LOG; then
#         echo "deleting $LOG $(connName $LOG) b/c another resigned"
#         kill_from_log $LOG || /bin/true
#         close_device $KEY $DEADDIR "other resigned"
#     elif grep -aq 'relay_error_curses(XWRELAY_ERROR_DEADGAME)' $LOG; then
#         echo "deleting $LOG $(connName $LOG) b/c another resigned"
#         kill_from_log $LOG || /bin/true
#         close_device $KEY $DEADDIR "other resigned"
#     else
#         maybe_resign $KEY
#     fi
# }


def build_cmds(args):
    devs = []
    COUNTER = 0
    PLAT_PARMS = []
    if not args.USE_GTK:
        PLAT_PARMS += ['--curses', '--close-stdin']

    for GAME in range(1,  args.NGAMES + 1):
        ROOM = 'ROOM_%.3d' % (GAME % args.NROOMS)
        NDEVS = pick_ndevs(args)
        LOCALS = figure_locals(args, NDEVS) # as array
        NPLAYERS = sum(LOCALS)
        assert(len(LOCALS) == NDEVS)
        DICT = args.DICTS[GAME % len(args.DICTS)]
        # make one in three games public
        PUBLIC = []
        if random.randint(0, 3) == 0: PUBLIC = ['--make-public', '--join-public']
        DEV = 0
        for NLOCALS in LOCALS:
            DEV += 1
            DB = '{}/{:02d}_{:02d}_DB.sql3'.format(args.LOGDIR, GAME, DEV)
            LOG = '{}/{:02d}_{:02d}_LOG.txt'.format(args.LOGDIR, GAME, DEV)

            BOARD_SIZE = ['--board-size', '15']
            #             if [ 0 -lt ${#APPS_OLD[@]} ]; then
            #                 # 50% chance of starting out with old app
            #                 NAPPS=$((1+${#APPS_OLD[*]}))
            #                 if [ 0 -lt $((RANDOM%$NAPPS)) ]; then
            #                     APPS[$COUNTER]=${APPS_OLD[$((RANDOM%${#APPS_OLD[*]}))]}
            #                     BOARD_SIZE="--board-size ${BOARD_SIZES_OLD[$((RANDOM%${#BOARD_SIZES_OLD[*]}))]}"
            #                     NEW_ARGS[$COUNTER]=""
            #                 fi
            #             fi

            PARAMS = player_params(args, NLOCALS, NPLAYERS, DEV)
            PARAMS += PLAT_PARMS
            PARAMS += BOARD_SIZE + ['--room', ROOM, '--trade-pct', args.TRADE_PCT, '--sort-tiles']
            if args.UNDO_PCT > 0:
                PARAMS += ['--undo-pct', args.UNDO_PCT]
            PARAMS += [ '--game-dict', DICT, '--relay-port', args.PORT, '--host', args.HOST]
            PARAMS += ['--slow-robot', '1:3', '--skip-confirm']
            PARAMS += ['--db', DB]
            if random.randint(0,100) % 100 < g_UDP_PCT_START:
                PARAMS += ['--use-udp']

            PARAMS += ['--drop-nth-packet', g_DROP_N]
            if random.randint(0, 100) < args.HTTP_PCT:
                PARAMS += ['--use-http']

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
            PARAMS += PUBLIC
            if  DEV > 1: 
                PARAMS += ['--force-channel', DEV - 1]
            else:
                PARAMS += ['--server']

            # print('PARAMS:', PARAMS)

            dev = Device(args, GAME, COUNTER, args.APP_NEW, PARAMS, ROOM, DB, LOG, len(LOCALS))
            dev.update_ldevid()
            devs.append(dev)

            COUNTER += 1
    return devs

# read_resume_cmds() {
#     COUNTER=0
#     for LOG in $(ls $LOGDIR/*.txt); do
#         echo "need to parse cmd and deal with changes"
#         exit 1
#         CMD=$(head -n 1 $LOG)

#         ARGS[$COUNTER]=$CMD
#         LOGS[$COUNTER]=$LOG
#         PIDS[$COUNTER]=0

#         set $CMD
#         while [ $# -gt 0 ]; do
#             case $1 in
#                 --file)
#                     FILES[$COUNTER]=$2
#                     shift
#                     ;;
#                 --room)
#                     ROOMS[$COUNTER]=$2
#                     shift
#                     ;;
#             esac
#             shift
#         done
#         COUNTER=$((COUNTER+1))
#     done
#     ROOM_PIDS[$ROOM]=0
# }

# launch() {
#     KEY=$1
#     LOG=${LOGS[$KEY]}
#     APP="${APPS[$KEY]}"
# 	if [ -z "$APP" ]; then
# 		echo "error: no app set"
# 		exit 1
# 	fi
#     PARAMS="${NEW_ARGS[$KEY]} ${ARGS[$KEY]} ${ARGS_DEVID[$KEY]}"
#     exec $APP $PARAMS >/dev/null 2>>$LOG
# }

# # launch_via_rq() {
# #      KEY=$1
# #      RELAYID=$2
# #      PIPE=${PIPES[$KEY]}
# #      ../relay/rq -f $RELAYID -o $PIPE &
# #      CMD="${CMDS[$KEY]}"
# #      exec $CMD >/dev/null 2>>$LOG
# # }

# send_dead() {
# 	ID=$1
# 	DB=${FILES[$ID]}
# 	while :; do
# 		[ -f $DB ] || break		# it's gone
# 		RES=$(echo 'select relayid, seed from games limit 1;' | sqlite3 -separator ' ' $DB || /bin/true)
# 		[ -n "$RES" ] && break
# 		sleep 0.2
# 	done
# 	RELAYID=$(echo $RES | awk '{print $1}')
# 	SEED=$(echo $RES | awk '{print $2}')
# 	JSON="[{\"relayID\":\"$RELAYID\", \"seed\":$SEED}]"
# 	curl -G --data-urlencode params="$JSON" http://$HOST/xw4/relay.py/kill >/dev/null 2>&1
# }

# close_device() {
#     ID=$1
#     MVTO=$2
#     REASON="$3"
#     PID=${PIDS[$ID]}
#     if [ $PID -ne 0 ]; then
#         kill ${PIDS[$ID]} 2>/dev/null
#         wait ${PIDS[$ID]}
#         ROOM=${ROOMS[$ID]}
#         [ ${ROOM_PIDS[$ROOM]} -eq $PID ] && ROOM_PIDS[$ROOM]=0
#     fi
#     unset PIDS[$ID]
#     unset ARGS[$ID]
#     echo "closing game: $REASON" >> ${LOGS[$ID]}
#     if [ -n "$MVTO" ]; then
#         [ -f "${FILES[$ID]}" ] && mv ${FILES[$ID]} $MVTO
#         mv ${LOGS[$ID]} $MVTO
#     else
#         rm -f ${FILES[$ID]}
#         rm -f ${LOGS[$ID]}
#     fi
#     unset FILES[$ID]
#     unset LOGS[$ID]
#     unset ROOMS[$ID]
#     unset APPS[$ID]
#     unset ARGS_DEVID[$ID]

#     COUNT=${#ARGS[*]}
#     echo "$COUNT devices left playing..."
# }

# OBITS=""

# kill_from_log() {
#     LOG=$1
#     RELAYID=$(./scripts/relayID.sh --long $LOG)
#     if [ -n "$RELAYID" ]; then
#         OBITS="$OBITS -d $RELAYID"
#         if [ 0 -eq $(($RANDOM%2)) ]; then
#             ../relay/rq -a $HOST $OBITS 2>/dev/null || /bin/true
#             OBITS=""
#         fi
#         return 0                # success
#     fi
#     echo "unable to send kill command for $LOG"
#     return 1
# }

# maybe_resign() {
#     if [ "$RESIGN_PCT" -gt 0 ]; then
#         KEY=$1
#         LOG=${LOGS[$KEY]}
#         if grep -aq XWRELAY_ALLHERE $LOG; then
# 			if [ $((${RANDOM}%100)) -lt $RESIGN_PCT ]; then
#                 echo "making $LOG $(connName $LOG) resign..."
#                 kill_from_log $LOG && close_device $KEY $DEADDIR "resignation forced" || /bin/true
#             fi
#         fi
#     fi
# }

# try_upgrade() {
#     KEY=$1
#     if [ 0 -lt ${#APPS_OLD[@]} ]; then
#         if [ $APP_NEW != "${APPS[$KEY]}" ]; then
#             # one in five chance of upgrading
#             if [ 0 -eq $((RANDOM % UPGRADE_ODDS)) ]; then
#                 APPS[$KEY]=$APP_NEW
#                 NEW_ARGS[$KEY]="$APP_NEW_PARAMS"
#                 print_cmdline $KEY
#             fi
#         fi
#     fi
# }

# try_upgrade_upd() {
#     KEY=$1
#     CMD=${ARGS[$KEY]}
#     if [ "${CMD/--use-udp/}" = "${CMD}" ]; then
#         if [ $((RANDOM % 100)) -lt $UDP_PCT_INCR ]; then
#             ARGS[$KEY]="$CMD --use-udp"
#             echo -n "$(date +%r): "
#             echo "upgrading key $KEY to use UDP"
#         fi
#     fi
# }

# check_game() {
#     KEY=$1
#     LOG=${LOGS[$KEY]}
#     CONNNAME="$(connName $LOG)"
#     OTHERS=""
#     if [ -n "$CONNNAME" ]; then
#         if grep -aq '\[unused tiles\]' $LOG ; then
#             for INDX in ${!LOGS[*]}; do
#                 [ $INDX -eq $KEY ] && continue
#                 ALOG=${LOGS[$INDX]}
#                 CONNNAME2="$(connName $ALOG)"
#                 if [ "$CONNNAME2" = "$CONNNAME" ]; then
#                     if ! grep -aq '\[unused tiles\]' $ALOG; then
#                         OTHERS=""
#                         break
#                     fi
#                     OTHERS="$OTHERS $INDX"
#                 fi
#             done
#         fi
#     fi

#     if [ -n "$OTHERS" ]; then
#         echo -n "Closing $CONNNAME [$(date)]: "
#         # kill_from_logs $OTHERS $KEY
#         for ID in $OTHERS $KEY; do
#             echo -n "${ID}:${LOGS[$ID]}, "
#             kill_from_log ${LOGS[$ID]} || /bin/true
# 			send_dead $ID
#             close_device $ID $DONEDIR "game over"
#         done
#         echo ""
#         # XWRELAY_ERROR_DELETED may be old
#     elif grep -aq 'relay_error_curses(XWRELAY_ERROR_DELETED)' $LOG; then
#         echo "deleting $LOG $(connName $LOG) b/c another resigned"
#         kill_from_log $LOG || /bin/true
#         close_device $KEY $DEADDIR "other resigned"
#     elif grep -aq 'relay_error_curses(XWRELAY_ERROR_DEADGAME)' $LOG; then
#         echo "deleting $LOG $(connName $LOG) b/c another resigned"
#         kill_from_log $LOG || /bin/true
#         close_device $KEY $DEADDIR "other resigned"
#     else
#         maybe_resign $KEY
#     fi
# }

# increment_drop() {
#     KEY=$1
#     CMD=${ARGS[$KEY]}
#     if [ "$CMD" != "${CMD/drop-nth-packet//}" ]; then
#         DROP_N=$(echo $CMD | sed 's,^.*drop-nth-packet \(-*[0-9]*\) .*$,\1,')
#         if [ $DROP_N -gt 0 ]; then
#             NEXT_N=$((DROP_N+1))
#             ARGS[$KEY]=$(echo $CMD | sed "s,^\(.*drop-nth-packet \)$DROP_N\(.*\)$,\1$NEXT_N\2,")
#         fi
#     fi
# }

def summarizeTileCounts(devs, endTime, state):
    global gDeadLaunches
    shouldGoOn = True
    data = [dev.getTilesCount() for dev in devs]
    nDevs = len(data)
    totalTiles = 0
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
    prev = -1
    for datum in data:
        gameNo = datum['game']
        if gameNo != prev:
            games.append([])
            prev = gameNo
        games[-1].append('{:0{width}d}'.format(datum['index'], width=colWidth))
    fmtData[0]['data'] = ['+'.join(game) for game in games]

    nLaunches = gDeadLaunches
    for datum in data:
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
            totalTiles += int(nTilesPool)
        fmtData[2]['data'].append(txt)

    print('')
    print('devs left: {}; bag tiles left: {}; total launches: {}; {}/{}'
          .format(nDevs, totalTiles, nLaunches, datetime.datetime.now(), endTime ))
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

    return now - state['lastChange'] < datetime.timedelta(minutes = 1)

def countCores():
    return len(glob.glob1('/tmp',"core*"))

gDone = False

def run_cmds(args, devs):
    nCores = countCores()
    endTime = datetime.datetime.now() + datetime.timedelta(minutes = args.TIMEOUT_MINS)
    printState = {}
    lastPrint = datetime.datetime.now()

    while len(devs) > 0 and not gDone:
        if countCores() > nCores:
            print('core file count increased; exiting')
            break
        now = datetime.datetime.now()
        if now > endTime:
            print('outta time; outta here')
            break

        # print stats every 5 seconds
        if now - lastPrint > datetime.timedelta(seconds = 5):
            lastPrint = now
            if not summarizeTileCounts(devs, endTime, printState):
                print('no change in too long; exiting')
                break

        dev = random.choice(devs)
        if not dev.running():
            if dev.handleAllDone():
                devs.remove(dev)
            else:
#             if [ -n "$ONE_PER_ROOM" -a 0 -ne ${ROOM_PIDS[$ROOM]} ]; then
#                 continue
#             fi
#             try_upgrade $KEY
#             try_upgrade_upd $KEY
                dev.launch()
#             PID=$!
#             # renice doesn't work on one of my machines...
#             renice -n 1 -p $PID >/dev/null 2>&1 || /bin/true
#             PIDS[$KEY]=$PID
#             ROOM_PIDS[$ROOM]=$PID
#             MINEND[$KEY]=$(($NOW + $MINRUN))
        elif not dev.minTimeExpired():
            # print('sleeping...')
            time.sleep(1.0)
        else:
            dev.kill()
            if dev.handleAllDone():
                devs.remove(dev)
            # if g_DROP_N >= 0: dev.increment_drop()
            #             update_ldevid $KEY


    # if we get here via a break, kill any remaining games
    if devs:
        print('stopping %d remaining games' % (len(devs)))
        for dev in devs:
            if dev.running(): dev.kill()

# run_via_rq() {
#     # launch then kill all games to give chance to hook up
#     for KEY in ${!ARGS[*]}; do
#         echo "launching $KEY"
#         launch $KEY &
#         PID=$!
#         sleep 1
#         kill $PID
#         wait $PID
#         # add_pipe $KEY
#     done

#     echo "now running via rq"
#     # then run them
#     while :; do
#         COUNT=${#ARGS[*]}
#         [ 0 -ge $COUNT ] && break

#         INDX=$(($RANDOM%COUNT))
#         KEYS=( ${!ARGS[*]} )
#         KEY=${KEYS[$INDX]}
#         CMD=${ARGS[$KEY]}
            
#         RELAYID=$(./scripts/relayID.sh --short ${LOGS[$KEY]})
#         MSG_COUNT=$(../relay/rq -a $HOST -m $RELAYID 2>/dev/null | sed 's,^.*-- ,,')
#         if [ $MSG_COUNT -gt 0 ]; then
#             launch $KEY &
#             PID=$!
#             sleep 2
#             kill $PID || /bin/true
#             wait $PID
#         fi
#         [ "$DROP_N" -ge 0 ] && increment_drop $KEY
#         check_game $KEY
#     done
# } # run_via_rq

# function getArg() {
#     [ 1 -lt "$#" ] || usage "$1 requires an argument"
#     echo $2
# }

def mkParser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--send-chat', dest = 'SEND_CHAT', type = str, default = None,
                        help = 'the message to send')

    parser.add_argument('--app-new', dest = 'APP_NEW', default = './obj_linux_memdbg/xwords',
                        help = 'the app we\'ll use')
    parser.add_argument('--num-games', dest = 'NGAMES', type = int, default = 1, help = 'number of games')
    parser.add_argument('--num-rooms', dest = 'NROOMS', type = int, default = 0,
                        help = 'number of roooms (default to --num-games)')
    parser.add_argument('--timeout-mins', dest = 'TIMEOUT_MINS', default = 10000, type = int,
                        help = 'minutes after which to timeout')
    parser.add_argument('--log-root', dest='LOGROOT', default = '.', help = 'where logfiles go')
    parser.add_argument('--dup-packets', dest = 'DUP_PACKETS', default = False, help = 'send all packet twice')
    parser.add_argument('--use-gtk', dest = 'USE_GTK', default = False, action = 'store_true',
                        help = 'run games using gtk instead of ncurses')
    # # 
    # #     echo "    [--clean-start]                                         \\" >&2
    parser.add_argument('--game-dict', dest = 'DICTS', action = 'append', default = [])
    # #     echo "    [--help]                                                \\" >&2
    parser.add_argument('--host',  dest = 'HOST', default = 'localhost',
                        help = 'relay hostname')
    # #     echo "    [--max-devs <int>]                                      \\" >&2
    parser.add_argument('--min-devs', dest = 'MINDEVS', type = int, default = 2,
                        help = 'No game will have fewer devices than this')
    parser.add_argument('--max-devs', dest = 'MAXDEVS', type = int, default = 4,
                        help = 'No game will have more devices than this')
    parser.add_argument('--min-run', dest = 'MINRUN', type = int, default = 2,
                        help = 'Keep each run alive at least this many seconds')
    # #     echo "    [--new-app <path/to/app]                                \\" >&2
    # #     echo "    [--new-app-args [arg*]]  # passed only to new app       \\" >&2
    # #     echo "    [--num-rooms <int>]                                     \\" >&2
    # #     echo "    [--old-app <path/to/app]*                               \\" >&2
    parser.add_argument('--one-per', dest = 'ONEPER', default = False,
                        action = 'store_true', help = 'force one player per device')
    parser.add_argument('--port', dest = 'PORT', default = 10997, type = int, \
                        help = 'Port relay\'s on')
    parser.add_argument('--resign-pct', dest = 'RESIGN_PCT', default = 0, type = int, \
                        help = 'Odds of resigning [0..100]')
    parser.add_argument('--seed', type = int, dest = 'SEED',
                        default = random.randint(1, 1000000000))
    # #     echo "    [--send-chat <interval-in-seconds>                      \\" >&2
    # #     echo "    [--udp-incr <pct>]                                      \\" >&2
    # #     echo "    [--udp-start <pct>]      # default: $UDP_PCT_START                 \\" >&2
    # #     echo "    [--undo-pct <int>]                                      \\" >&2
    parser.add_argument('--http-pct', dest = 'HTTP_PCT', default = 0, type = int,
                        help = 'pct of games to be using web api')

    parser.add_argument('--undo-pct', dest = 'UNDO_PCT', default = 0, type = int)
    parser.add_argument('--trade-pct', dest = 'TRADE_PCT', default = 0, type = int)

    parser.add_argument('--with-valgrind', dest = 'VALGRIND', default = False,
                        action = 'store_true')

    return parser

# #######################################################
# ##################### MAIN begins #####################
# #######################################################

def parseArgs():
    args = mkParser().parse_args()
    assignDefaults(args)
    print(args)
    return args
    # print(options)

# while [ "$#" -gt 0 ]; do
#     case $1 in
#         --udp-start)
#             UDP_PCT_START=$(getArg $*)
#             shift
#             ;;
#         --udp-incr)
#             UDP_PCT_INCR=$(getArg $*)
#             shift
#             ;;
#         --clean-start)
#             DO_CLEAN=1
#             ;;
#         --num-games)
#             NGAMES=$(getArg $*)
#             shift
#             ;;
#         --num-rooms)
#             NROOMS=$(getArg $*)
#             shift
#             ;;
#         --old-app)
#             APPS_OLD[${#APPS_OLD[@]}]=$(getArg $*)
#             shift
#             ;;
# 		--log-root)
# 			[ -d $2 ] || usage "$1: no such directory $2"
# 			LOGDIR=$2/$(basename $0)_logs
# 			shift
# 			;;
#         --dup-packets)
                  #             DUP_PACKETS=1
#             ;;
#         --new-app)
#             APP_NEW=$(getArg $*)
#             shift
#             ;;
#         --new-app-args)
#             APP_NEW_PARAMS="${2}"
#             echo "got $APP_NEW_PARAMS"
#             shift
#             ;;
#         --game-dict)
#             DICTS[${#DICTS[@]}]=$(getArg $*)
#             shift
#             ;;
#         --min-devs)
#             MINDEVS=$(getArg $*)
#             shift
#             ;;
#         --max-devs)
#             MAXDEVS=$(getArg $*)
#             shift
#             ;;
# 		--min-run)
# 			MINRUN=$(getArg $*)
# 			[ $MINRUN -ge 2 -a $MINRUN -le 60 ] || usage "$1: n must be 2 <= n <= 60"
# 			shift
# 			;;
#         --one-per)
#             ONEPER=TRUE
#             ;;
#         --host)
#             HOST=$(getArg $*)
#             shift
#             ;;
#         --port)
#             PORT=$(getArg $*)
#             shift
#             ;;
#         --seed)
#             SEED=$(getArg $*)
#             shift
#             ;;
#         --undo-pct)
#             UNDO_PCT=$(getArg $*)
#             shift
#             ;;
#         --http-pct)
#             HTTP_PCT=$(getArg $*)
#             [ $HTTP_PCT -ge 0 -a $HTTP_PCT -le 100 ] || usage "$1: n must be 0 <= n <= 100"
#             shift
#             ;;
#         --send-chat)
#             SEND_CHAT=$(getArg $*)
#             shift
#             ;;
#         --resign-pct)
#             RESIGN_PCT=$(getArg $*)
# 			[ $RESIGN_PCT -ge 0 -a $RESIGN_PCT -le 100 ] || usage "$1: n must be 0 <= n <= 100"
#             shift
#             ;;
# 		--no-timeout)
# 			TIMEOUT=0x7FFFFFFF
# 			;;
#         --help)
#             usage
#             ;;
#         *) usage "unrecognized option $1"
#             ;;
#     esac
#     shift
# done

def assignDefaults(args):
    if not args.NROOMS: args.NROOMS = args.NGAMES
    if len(args.DICTS) == 0: args.DICTS.append('CollegeEng_2to8.xwd')
    args.LOGDIR = os.path.basename(sys.argv[0]) + '_logs'
    # Move an existing logdir aside
    if os.path.exists(args.LOGDIR):
        shutil.move(args.LOGDIR, '/tmp/' + args.LOGDIR + '_' + str(random.randint(0, 100000)))
    for d in ['', 'done', 'dead',]:
        os.mkdir(args.LOGDIR + '/' + d)
# [ -z "$SAVE_GOOD" ] && SAVE_GOOD=YES
# # [ -z "$RESIGN_PCT" -a "$NGAMES" -gt 1 ] && RESIGN_RATIO=1000 || RESIGN_RATIO=0
# [ -z "$DROP_N" ] && DROP_N=0
# [ -z "$USE_GTK" ] && USE_GTK=FALSE
# [ -z "$UPGRADE_ODDS" ] && UPGRADE_ODDS=10
# #$((NGAMES/50))
# [ 0 -eq $UPGRADE_ODDS ] && UPGRADE_ODDS=1
# [ -n "$SEED" ] && RANDOM=$SEED
# [ -z "$ONEPER" -a $NROOMS -lt $NGAMES ] && usage "use --one-per if --num-rooms < --num-games"

# [ -n "$DO_CLEAN" ] && cleanup

# RESUME=""
# for FILE in $(ls $LOGDIR/*.{xwg,txt} 2>/dev/null); do
#     if [ -e $FILE ]; then
#         echo "Unfinished games found in $LOGDIR; continue with them (or discard)?"
#         read -p "<yes/no> " ANSWER
#         case "$ANSWER" in
#             y|yes|Y|YES)
#                 RESUME=1
#                 ;;
#             *)
#                 ;;
#         esac
#     fi
#     break
# done

# if [ -z "$RESUME" -a -d $LOGDIR ]; then
# 	NEWNAME="$(basename $LOGDIR)_$$"
#     (cd $(dirname $LOGDIR) && mv $(basename $LOGDIR) /tmp/${NEWNAME})
# fi
# mkdir -p $LOGDIR

# if [ "$SAVE_GOOD" = YES ]; then
#     DONEDIR=$LOGDIR/done
#     mkdir -p $DONEDIR
# fi
# DEADDIR=$LOGDIR/dead
# mkdir -p $DEADDIR

# for VAR in NGAMES NROOMS USE_GTK TIMEOUT HOST PORT SAVE_GOOD \
#     MINDEVS MAXDEVS ONEPER RESIGN_PCT DROP_N ALL_VIA_RQ SEED \
#     APP_NEW; do
#     echo "$VAR:" $(eval "echo \$${VAR}") 1>&2
# done
# echo "DICTS: ${DICTS[*]}"
# echo -n "APPS_OLD: "; [ xx = "${APPS_OLD[*]+xx}" ] && echo "${APPS_OLD[*]}" || echo ""

# echo "*********$0 starting: $(date)**************"
# STARTTIME=$(date +%s)
# [ -z "$RESUME" ] && build_cmds || read_resume_cmds
# if [ TRUE = "$ALL_VIA_RQ" ]; then
#     run_via_rq
# else
#     run_cmds
# fi

# wait

# SECONDS=$(($(date +%s)-$STARTTIME))
# HOURS=$((SECONDS/3600))
# SECONDS=$((SECONDS%3600))
# MINUTES=$((SECONDS/60))
# SECONDS=$((SECONDS%60))
# echo "*********$0 finished: $(date) (took $HOURS:$MINUTES:$SECONDS)**************"

def termHandler(signum, frame):
    global gDone
    print('termHandler() called')
    gDone = True

def main():
    startTime = datetime.datetime.now()
    signal.signal(signal.SIGINT, termHandler)

    args = parseArgs()
    devs = build_cmds(args)
    nDevs = len(devs)
    run_cmds(args, devs)
    print('{} finished; took {} for {} devices'.format(sys.argv[0], datetime.datetime.now() - startTime, nDevs))

##############################################################################
if __name__ == '__main__':
    main()

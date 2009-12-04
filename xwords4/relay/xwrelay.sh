#!/bin/sh

DIR=${DIR:-$(dirname $0)}
XWRELAY=${DIR}/xwrelay
PIDFILE=${DIR}/xwrelay.pid
CONFFILE=${DIR}/xwrelay.conf
IDFILE=${DIR}/nextid.txt
CSSFILE=${DIR}/xwrelay.css

LOGFILE=/tmp/xwrelay_log.txt
#LOGFILE=/dev/null

date > $LOGFILE

do_start() {
    if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
        echo "already running: pid=$(cat $PIDFILE)" | tee -a $LOGFILE
    elif pidof $XWRELAY >/dev/null; then
        echo "already running: pid=$(pidof $XWRELAY)" | tee -a $LOGFILE
    else
        echo "starting..." | tee -a $LOGFILE
        echo "running $XWRELAY $@ -f $CONFFILE -s $CSSFILE" | tee -a $LOGFILE
        $XWRELAY $@ -f $CONFFILE -i $IDFILE -s $CSSFILE &
        NEWPID=$!                
        echo -n $NEWPID > $PIDFILE
        sleep 1
        echo "running with pid=$(cat $PIDFILE)" | tee -a $LOGFILE
    fi
}

case $1 in
    
    stop)
        shift
        if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
            sync
            echo "killing pid=$(cat $PIDFILE)" | tee -a $LOGFILE
            kill $(cat $PIDFILE)
        else
            echo "not running or $PIDFILE not found" | tee -a $LOGFILE
            PID=$(pidof $XWRELAY || true)
            if [ -n "$PID" ]; then
                echo "maybe it's $PID; killing them" | tee -a $LOGFILE
                kill $PID
            fi
        fi
        rm -f $PIDFILE
        ;;

    restart)
        shift
        $0 stop
        sleep 1
        $0 start $@
        ;;

    start)
        shift
        do_start $@
        ;;
    *)
        do_start $@
        ;;

esac

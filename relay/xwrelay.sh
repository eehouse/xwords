#!/bin/sh

DIR=${DIR:-$(dirname $0)}
XWRELAY=${DIR}/xwrelay
PIDFILE=${DIR}/xwrelay.pid
CONFFILE=${DIR}/xwrelay.conf
IDFILE=${DIR}/nextid.txt

do_start() {
    if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
        echo "already running: pid=$(cat $PIDFILE)"
    elif pidof xwrelay >/dev/null; then
        echo "already running: pid=$(pidof xwrelay)"
    else
        echo "starting..."
        echo "running $XWRELAY $@ -f $CONFFILE"
        $XWRELAY $@ -f $CONFFILE -i $IDFILE &
        NEWPID=$!                
        echo $NEWPID > $PIDFILE
        sleep 1
        echo "running with pid=$(cat $PIDFILE)"
    fi
}

case $1 in
    
    stop)
        shift
        if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
            sync
            echo "killing pid=$(cat $PIDFILE)"
            kill $(cat $PIDFILE)
        else
            echo "not running or $PIDFILE not found"
            PID=$(pidof xwrelay || true)
            if [ "x${PID}" != "x" ]; then
                echo "maybe it's $PID; killing them"
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

#!/bin/sh

XWRELAY="./xwrelay"
PIDFILE=./xwrelay.pid

CMD=$1
shift

case $CMD in
    
    stop)
        if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
            sync
            echo "killing pid=$(cat $PIDFILE)"
            kill $(cat $PIDFILE)
        else
            echo "not running"
        fi
        rm $PIDFILE
        ;;

    restart)
        $0 stop
        sleep 1
        $0 start $@
        ;;

    start|*)
        if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
            echo "already running: pid=$(cat $PIDFILE)"
        else
            echo "starting..."
            $XWRELAY $@ &
            NEWPID=$!
            echo $NEWPID > $PIDFILE
            sleep 1
            echo "running with pid=$(cat $PIDFILE)"
        fi
        ;;

esac

#!/bin/sh

XWRELAY="./xwrelay"
PIDFILE=./xwrelay.pid


case $1 in
    
    stop)
        if [ -f $PIDFILE ]; then
            sync
            echo "killing pid=$(cat $PIDFILE)"
            kill $(cat $PIDFILE)
            rm $PIDFILE
        else
            echo "not running"
        fi
        ;;

    restart)
        $0 stop
        sleep 1
        $0 start
        ;;

    start|*)
        if [ -f $PIDFILE ]; then
            echo "already running: pid=$(cat $PIDFILE)"
        else
            echo "starting..."
            $XWRELAY
            sleep 1
            echo "running with pid=$(cat $PIDFILE)"
        fi
        ;;

esac

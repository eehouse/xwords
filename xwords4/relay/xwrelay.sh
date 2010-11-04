#!/bin/sh

DIR=$(pwd)
XWRELAY=${DIR}/xwrelay
PIDFILE=${DIR}/xwrelay.pid
CONFFILE=${DIR}/xwrelay.conf
IDFILE=${DIR}/nextid.txt
CSSFILE=${DIR}/xwrelay.css

LOGFILE=/tmp/xwrelay_log_$$.txt
#LOGFILE=/dev/null

date > $LOGFILE

usage() {
    echo "usage: $0 start | stop | restart | mkdb"
}

make_db() {
    createdb xwgames
    cat | psql xwgames --file - <<EOF
create or replace function sum_array( DECIMAL [] )
returns decimal
as \$\$
select sum(\$1[i])
from generate_series(
    array_lower(\$1,1),
    array_upper(\$1,1)
) g(i);
\$\$ language sql immutable;
EOF

    cat | psql xwgames --file - <<EOF
CREATE TABLE games ( 
cid integer
,room VARCHAR(32)
,lang INTEGER
,pub BOOLEAN
,connName VARCHAR(64) UNIQUE PRIMARY KEY
,nTotal INTEGER
,nPerDevice INTEGER[]
,seeds INTEGER[]
,nSent INTEGER DEFAULT 0
,ctime TIMESTAMP (0) DEFAULT CURRENT_TIMESTAMP
,mtime TIMESTAMP (0)
);
EOF

    cat | psql xwgames --file - <<EOF
CREATE TABLE msgs ( 
id SERIAL
,connName VARCHAR(64)
,hid INTEGER
,ctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP
,msg BYTEA
,UNIQUE ( connName, hid, msg )
);
EOF
}

do_start() {
    if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
        echo "already running: pid=$(cat $PIDFILE)" | tee -a $LOGFILE
    elif pidof $XWRELAY >/dev/null; then
        echo "already running: pid($XWRELAY)=>$(pidof $XWRELAY)" | tee -a $LOGFILE
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

    mkdb)
        make_db
        ;;

    *)
        usage
        exit 0
        ;;

esac

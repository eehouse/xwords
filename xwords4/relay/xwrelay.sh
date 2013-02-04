#!/bin/sh

set -e -u

DIR=$(pwd)
XWRELAY=${DIR}/xwrelay
PIDFILE=${DIR}/xwrelay.pid
CONFFILE=${DIR}/xwrelay.conf
IDFILE=${DIR}/nextid.txt

LOGFILE=/tmp/xwrelay_log_$$.txt
#LOGFILE=/dev/null

date > $LOGFILE

usage() {
    echo "usage: $0 start | stop | restart | mkdb"
}

make_db() {
    if [ ! -e $CONFFILE ]; then
        echo "unable to find $CONFFILE"
        exit 1
    fi
    DBNAME=$(grep '^DB_NAME' $CONFFILE | sed 's,^.*=,,')
    if [ -z "$DBNAME" ]; then
        echo "DB_NAME keyword not found"
        exit 1
    fi
    createdb $DBNAME
    cat | psql $DBNAME --file - <<EOF
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

    cat | psql $DBNAME --file - <<EOF
CREATE TABLE games ( 
cid integer
,room VARCHAR(32)
,dead BOOLEAN DEFAULT FALSE
,lang INTEGER
,pub BOOLEAN
,connName VARCHAR(64) UNIQUE PRIMARY KEY
,nTotal INTEGER
,clntVers INTEGER[]
,nPerDevice INTEGER[]
,seeds INTEGER[]
,ack VARCHAR(1)[]
,nSent INTEGER DEFAULT 0
,ctime TIMESTAMP (0) DEFAULT CURRENT_TIMESTAMP
,mtimes TIMESTAMP(0)[]
,addrs INET[]
,devids INTEGER[]
,tokens INTEGER[]
);
EOF

    cat | psql $DBNAME --file - <<EOF
CREATE TABLE msgs ( 
id SERIAL
,connName VARCHAR(64)
,hid INTEGER
,token INTEGER
,ctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP
,stime TIMESTAMP DEFAULT NULL
,devid INTEGER
,msg BYTEA
,msg64 TEXT
,msglen INTEGER
,UNIQUE ( connName, hid, msg )
);
EOF

    cat | psql $DBNAME --file - <<EOF
CREATE TABLE devices ( 
id INTEGER UNIQUE PRIMARY KEY
,devType INTEGER
,devid TEXT
,ctime TIMESTAMP DEFAULT CURRENT_TIMESTAMP
,mtime TIMESTAMP
,unreg BOOLEAN DEFAULT FALSE
);
EOF
}

do_start() {
    if [ -f $PIDFILE ] && [ -f /proc/$(cat $PIDFILE)/exe ]; then
        echo "already running: pid=$(cat $PIDFILE)" | tee -a $LOGFILE
    elif pidof $XWRELAY >/dev/null; then
        echo "already running: pid($XWRELAY)=>$(pidof $XWRELAY)" | tee -a $LOGFILE
    else
        if [ ! -e $CONFFILE ]; then
            echo "unable to find $CONFFILE"
            exit 1
        fi
        echo "starting..." | tee -a $LOGFILE
        echo "running $XWRELAY $@ -f $CONFFILE" | tee -a $LOGFILE
        $XWRELAY $@ -f $CONFFILE &
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

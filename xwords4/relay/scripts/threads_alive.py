#!/usr/bin/python

import re;

LOGFILE = "./xwrelay.log"
LINEPAT = r"^<0x.*>"

# first get a list of the threads

last_seen = {}
last_ts = ""

def do_init():
    return [ {}, "00:00:00" ]

def print_last_seen(last_seen):
    print "one run's worth:"
    for key,value in last_seen.iteritems():
        print key, "---", value

def main():
    cur_date = "00/00/00"
    last_seen, last_ts = do_init()

    fil = open( LOGFILE, "r" )
    for line in fil:
        mo = re.match( r".* \*\*\*\*\* forked \d*th new process \*\*\*\*\*", line )
        if mo:
            print_last_seen( last_seen )
            last_seen, last_ts = do_init()
            continue
        mo = re.match( r"<(0x.*)>(\d\d:\d\d:\d\d): (.*)$", line)
        if mo:
            thread = mo.group(1);
            last_ts = mo.group(2)
            last_seen[thread] = cur_date + "--" + last_ts
            continue
        mo = re.match( r"It's a new day: (.*)$", line )
        if mo:
            cur_date = mo.group(1)
    fil.close()

    print_last_seen( last_seen )

#######################################################################
# main
#######################################################################
if __name__ == "__main__":
    main()
else:
    print "not main"

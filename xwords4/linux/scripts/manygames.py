#!/usr/bin/python3

import os, subprocess, threading, time

gFIFO_NAME = '/tmp/fifo'

def launch_thread():
    print('launch_thread() called')
    args = ['./obj_linux_memdbg/xwords', '--curses', '--cmd-socket-name', gFIFO_NAME ]
    prcss = subprocess.Popen(args, stdout = subprocess.DEVNULL)
    print('launch_thread() calling communicate()')
    prcss.communicate()
    print('launch_thread() DONE')

def main():
    os.unlink(gFIFO_NAME)
    os.mkfifo(gFIFO_NAME)
    # mkfifo

    # launch app in background
    thrd = threading.Thread( target=launch_thread)
    thrd.start()

    # Loop writing to fifo
    for cmd in ['null', 'null', 'null', 'quit']:
        time.sleep(2)
        fifo_out = open( gFIFO_NAME, 'w' )
        print('open DONE')
        fifo_out.write( '{}'.format(cmd) )
        fifo_out.close()

    # Kill app

##############################################################################
if __name__ == '__main__':
    main()

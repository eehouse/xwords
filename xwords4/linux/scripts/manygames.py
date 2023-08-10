#!/usr/bin/python3

import os, subprocess, time

gFIFO_NAME = '/tmp/fifo'

def main():
    os.mkfifo(gFIFO_NAME)
    # mkfifo

    # launch app in background
    args = ['./obj_linux_memdbg/xwords', '--curses', '--cmd-socket-name', gFIFO_NAME ]
    subprocess.Popen(args, stdout = subprocess.DEVNULL)

    # Loop writing to fifo
    for ii in range(5):
        time.sleep(2)
        print('calling open')
        fifo_out = open( gFIFO_NAME, 'w' )
        print('open DONE')
        fifo_out.write( 'message {}'.format(ii) )
        fifo_out.close()

    # Kill app

##############################################################################
if __name__ == '__main__':
    main()

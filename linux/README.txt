This directory contains the desktop Linux port of Crosswords.

To build, run a shell in this directory and type

# make
or
# make debug
or
# make memdebug

Any will work as long as you have both libncurses and libgtk-1.2 and
the associated headers installed on your system.  If you don't you can
play with the Makefile to build with only GTK or ncurses.

Once you've built, go to the linux directory that will be created
within this one and type, at a minimum

# ./xwords -s -n SomeName

to get a GTK-based game with the built-in (English) tiles.  (Add the
-u flag to run with ncurses instead of GTK.)  There will be no robot
player, and the hint feature ('?' button) won't work.  For that you
need a real dictionary, which you can build in the dawg directory.  If
you build the BasEnglish2to8.xwd one in dawg/English, this command
will run a two person game between you and the machine:

# ./xwords -s -r robot -n SomeName -d ../../dawg/English/BasEnglish2to8.xwd

Here are the commands to launch two copies playing against each other
over the network.  Do these in separate shells both in the same
directory as the above commands ran in.  Launch the one with the -s
flag (the "server") first.

 s1# ./xwords -s -r Eric -N -p 4000 -l 4001
 s2# ./xwords -d ../../dawg/English/BasEnglish2to8.xwd -r Kati -p 4001 -l 6002

Both of these have "robot" players.  Turn one or both -r flags to -n
for human players who make their own moves.

If you want to run them on different machines, just add the -a flag to
the client telling it on what machine to find the server (since it
sends the first message, and the server will use the return address
from that message.)



*****

Please keep in mind that these Linux desktop clients are meant for
development only, as testbeds for code in ../common/ that will also be
used for the "real" products on PalmOS, PocketPC, eBookman, etc.
They're not supposed to be polished.

This file describes how to build Crosswords for Franklin's eBookman.

It should be possible to do eBookman development on either Windows
(with cygwin) or Linux, but I've only done it on Linux, so that's what
I'll describe.  I expect it's not much different on Windows once
cygwin's installed.

You can get the SDK, and instructions for installing it, here:
http://download.franklin.com/franklin/ebookman/developer/

There are two environment variables you'll need to build for eBookman.
Add these to your shell startup script.  Here's mine (for bash):

export ARCH=i686
export EBOOKMAN_SDK=/home/ehouse/franklin/SDK

You can build either for the simulator/debugger, or to run on a
device.  For the simulator, type at a commandline in this directory:

# make memdebug

or

# make

Provided you have a copy of BasEnglish2to8.xwd in this directory, you
can then run Crosswords in the simulator by typing:

# ./sGDB

(To run with additional dictionaries, edit initial.mom.)

The command ./GDB is also available.  The difference is that the
former is much faster and launches you directly into Crosswords, while
the latter launches the device's App Picker screen, allowing you to
launch Crosswords the way an end-user would.  sGDB is what I use 95%
of the time.

(The simulator is a bit rough.  Not all of the buttons work, etc.  To
at least get started, wait for the two windows to come up.  Go to the
one titled "Source Window", and choose "Continue" on the "Control"
menu.  That'll get Crosswords running.)

To build a .seb file you can install on a device, type

# make xwords4.seb

Install this on an eBookman together with at least one dictionary
(e.g. BasEnglish2to8.seb) and you're good to go.

******************

On debugging: I've never had much luck with the source-level debugger
in the eBookman SDK.  I use XP_DEBUGF statements a lot, and do common
code development on the Linux port where debugging's better.  If you're
using cygwin on Windows you may have better luck.

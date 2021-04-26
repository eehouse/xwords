This directory contains the desktop Linux port of CrossWords. It
should build and run on any Debian derivative (e.g. Ubuntu or
Raspbian).

You'll need the build tool "make" installed. Once you have that, cd
into this directory and run

# make debs_install

which will get you all the packages the build depends on. Your user
will need sudo capability.

Once that's done, build:

# make MEMDEBUG=TRUE
or
# make

the latter for a release build

If the build succeeds (if it doesn't, and it's not because you changed
something, email me at xwords@eehouse.org), run the debug build thus:

# ./obj_linux_memdbg/xwords

or the release thus:

# ./obj_linux_rel/xwords

Add --help to get a list of all the options.

The first time you launch its main window will be tiny and in the
upper-left corner of your screen. If you resize or move the window the
app will remember its new size and location. Use the buttons and menus
to do stuff. Not all will be obvious, and some stuff may not
work. Always remember that this app exists to develop and test code
whose main target is Android or other mobile platforms, not desktop
Linux.

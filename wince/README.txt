This document describes how to build Crosswords for PocketPC (and for
desktop Windows).  You can build using Debian Linux, or using
Microsoft's embedded visual tools (which are free provided you have a
copy of Visual Studio.)

(Caveat: I've had to make modifications to the MinGW and pocketpc-sdk
packages to support Crosswords.  I have submitted the changes back to
the tool maintainers, but it may take some time before they appear in
Debian.  Please contact me if you need them in the meantime, at
ehouse@users.sf.net.)

To build for PocketPC with Debian, you need to install the MinGW and
pocketpc packages:

sudo apt-get install pocketpc-binutils pocketpc-gas pocketpc-gcc \
pocketpc-sdk mingw32-runtime

If you're building for Windows, you also need the mingw tools: 

sudo apt-get install mingw32-binutils mingw32

Once those are installed, it's just a matter of typing

make TARGET_OS=wince

or

make TARGET_OS=win32

at the commandline in this directory.

******************************************************************************
The rest of this file is older, and talks about building with
Microsoft's tools.

First, you need to install Microsoft's SDK for Wince/PocketPC.  It's
available for about $8 shipping and handling from Microsoft, and is
included with many books on PocketPC/Wince programming.  Here's a link
where I was able to get it:

https://microsoft.order-5.com/trialstore/product.asp?catalog%5Fname=MSTrialandEval&category%5Fname=Developer+Tools&product%5Fid=X09%2D17298

1.  Launch eMbedded Visual C++ (EVC++).

2.  Choose "Open" from the "File" menu, and open xwords.vcp in this
    directory.

*** To build for the emulator (best for development work): ***

3.  On the "Build" menu, choose "Set active conguration".  Select
    "xwords - Win32 (WCE x86) Debug" and click on the "OK" button.

4.  Choose "Build xwords.exe" from the "Build" menu.  You will see the
    names of files appear in the Build window, and a few warnings that
    you can safely ignore (though fixes are welcomed!)  After the link
    finishes, you'll see a dialog announcing that the emulator is
    being started.  The emulator will appear, and once the dialog
    disappears, if you go to the Start menu in the emulator you'll see
    a listing for "xwords".

5.  Before you can run Crosswords/xwords, you need to install a
    dictionary on the emulator.  To do this, go back to EVC++ and
    choose "Remote file viewer" from the "Tools" menu.

6.  From the "Connection" menu of the file browser that appears, choose
    "Add connection", select "Pocket PC 2002 Emulation" and click "Ok".

7.  Now navigate to a directory.  (I usually use "\My
    Documents\Personal", but it shouldn't matter.)  From the "File"
    menu, choose "Export file".  Then select the file
    "BasEnglish2to8.xwd" from the directory dawg/English.  (The
    directory dawg lives in the same directory as the wince directory
    this document is in.)

8.  Now when you launch xwords on the emulator it will not complain
    that there's no dictionary, and you'll be able to navigate to
    BasEnglish2to8.xwd when starting your first game.


*** To build for a device ***

9.  On the "Build" menu, choose "Set active conguration".  Select
    "xwords - Win32 (WCE ARM) Release" or "xwords - Win32 (WCE x86)
    Debug" and click on the "OK" button.  The "Debug" version has a
    number of asserts and other debugging aids compiled in, and logs
    to the file "/My Documents/Personal/xwDbgLog.txt" on the device.
    The "Release" version does no logging, and is smaller and faster
    and in general better suited for non-developers.

10. Choose "Build xwords.exe" from the Build menu.  Once the link is
    finished, EVC++ will try to upload the executable to your device.
    If that works, fine.  Otherwise, you'll need to get the file
    wince/ARMRel/xwords.exe (or wince/ARMDbg/xwords.exe) to your
    device on your own.  I use either an SD card or IR beaming for
    this.  You'll also need a dictionary such as BasEnglish2to8.xwd
    from dawg/English.

11. Once xwords.exe and a dictionary are on your device, just use the
    File Explorer to lauch xwords.exe.

12. Enjoy!  And please report bugs and/or submit fixes.


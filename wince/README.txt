This document describes how to build Crosswords for PocketPC.

First, you need to install Microsoft's SDK for Wince/PocketPC.  It's
available for about $7 shipping and handling from Microsoft, and is
included with many books on PocketPC/Wince programming.

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


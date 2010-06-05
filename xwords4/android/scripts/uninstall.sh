#!/bin/sh

for DEVICE in $(adb devices | grep '^emulator' | sed 's/device//'); do
    adb -s $DEVICE uninstall org.eehouse.android.xw4
done

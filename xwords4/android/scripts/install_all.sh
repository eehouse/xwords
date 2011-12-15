#!/bin/sh

for DEVICE in $(adb devices | grep '^emulator' | sed 's/device//'); do
    adb -s $DEVICE install -r ./bin/XWords4-debug.apk
    COUNT=$((COUNT+1))
done

echo "installed into $COUNT emulator instances"

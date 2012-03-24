#!/bin/sh

set -u -e

TARGET="android-7"

usage() {
    echo "usage: $0 [--target TARGET]"
    exit 1
}

while [ $# -ge 1 ]; do
    echo $1
    case $1 in
        --target)
            shift
            TARGET=$1
            ;;
        *)
            echo "default: got $1"
            usage
            ;;
    esac
    shift
done

# create local.properties for 1.6 sdk (target id 4).  Use 'android
# list targets' to get the full set.
android update project --path . --target $TARGET

echo "local.properties looks like this:"
echo ""
cat local.properties
echo ""
exit 0

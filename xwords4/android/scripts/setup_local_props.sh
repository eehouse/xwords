#!/bin/sh

set -u -e

TARGET="Google Inc.:Google APIs:11"

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

# create local.properties 
android update project --path . --target "$TARGET"

echo "local.properties looks like this:"
echo ""
cat local.properties
echo ""
exit 0

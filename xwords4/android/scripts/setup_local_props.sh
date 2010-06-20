#!/bin/sh

echo "i am here"

cd $(dirname $0)
cd ../XWords4

if [ ! -e local.properties ]; then
    ANDROID="$(which android)"
    SDK_DIR=$(dirname $ANDROID)
    SDK_DIR=$(dirname $SDK_DIR)
    echo "# generated by $0" > local.properties
    echo "sdk.dir=$SDK_DIR" >> local.properties
fi

exit 0

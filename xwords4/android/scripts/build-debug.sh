#!/bin/bash

DIR=/tmp/build_$$_dir
mkdir -p $DIR
pushd $DIR

git clone https://github.com/eehouse/xwords.git
cd xwords/xwords4/android
./gradlew asXw4dDeb

if [ -n "${XW4D_UPLOAD}" ]; then
	scp $(find . -name '*.apk') ${XW4D_UPLOAD}
else
	echo "not uploading: XW4D_UPLOAD not set" >&2
fi

popd
rm -rf $DIR

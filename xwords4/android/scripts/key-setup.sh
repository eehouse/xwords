#!/bin/sh

set -e -u

if [ $1 = 'debug' ]; then

	HOMELOC=~/.android/debug.keystore
	HERELOC=$(pwd)/$(dirname $0)/debug.keystore

	if [ -L $HOMELOC ]; then 
		if cmp $(readlink $HOMELOC) $HERELOC; then
			exit 0
		fi
	elif [ -e $HOMELOC ]; then
		cat << EOF
* You are using a different keystore from what's part of this project.
* You won't be able to install this over something built from a
* different machine.  If that's not ok, remove it (i.e. run this on a
* commandline: 
rm $HOMELOC
* and rerun ant to use the built-in keystore.
EOF
	else
		echo "$0: creating link at $HOMELOC"
		ln -sf  $HERELOC $HOMELOC
	fi

fi

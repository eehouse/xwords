#!/bin/bash

set -e -u

PKG=''
NUM=''
DEVICE=''
FORMAT="--format=color"
PKGS=("org.eehouse.android.xw4GPTest"
      "org.eehouse.android.xw4"
	  "org.eehouse.android.xw4dbg"
	  "org.eehouse.android.xw4grd"
	  "org.eehouse.android.xw4dbg"
	  "org.eehouse.android.xw4"
	 )

usage() {
	echo "usage: $0 [-s device] [--no-color] [<index>|<package>]"
	exit 1
}

# Process parameters
while [ $# -ge 1 ]; do
	case $1 in
		[0-9]) NUM=$1
			   ;;
		--no-color) FORMAT=''
					 ;;
		--help) usage
				;;
		-s) DEVICE="-s $2"
			shift
			;;
		*) echo "unexpected param $1"
		   usage
		   ;;
	esac
	shift
done

# Ask for NUM if wasn't provided on cmdline
while [ -z $NUM ]; do
	for ii in "${!PKGS[@]}"; do
		printf "[%d] %s\n" "$ii" "${PKGS[$ii]}"
	done

	read -p "Choose a package id (by number), or 'q' to exit: " CHOICE
	case $CHOICE in
		[0-9])
			NUM=$CHOICE
			break
			;;
		q)
			break
			;;
		*)
			echo "not q!!"
			;;
	esac
done

if [ -z $NUM ]; then
	echo "no package specified; exiting"
else
	PKG=${PKGS[$NUM]}
	echo "using $PKG"
	
	# if [ -z "$PKG" ]; then
	# 	echo "Usage: $0 <package_name>"
	# 	return 1
	# fi

	echo "Waiting for $PKG to start..."

	# Wait for PID to exist
	PID=""
	while [ -z "$PID" ]; do
		PID=$(adb ${DEVICE} shell pidof -s "$PKG")
		sleep 0.5
	done

	echo "Found PID $PID. Starting logcat..."
	# --format=color makes it readable in Emacs/Terminal
	# --pid filters to our specific process
	adb ${DEVICE} logcat --pid=$PID ${FORMAT}
fi

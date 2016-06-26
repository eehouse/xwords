#!/bin/bash

set -e -u

SERIAL=''
declare -a SERIALS=[]
ADB=$(which adb)

usage() {
	[ $# -ge 1 ] && echo "ERROR: $1"
	echo "usage $0 # start logcat, allowing choice if more than one device available"
	exit 1
}

listSerials() {
	COUNT=0
	while read LINE; do
		SERIALS[$COUNT]=$(echo $LINE | awk '{print $1}')
		COUNT=$((COUNT+1))
	done <<< "$(adb devices | grep 'device$')"
}

setSerial() {
	NSERIALS=${#SERIALS[@]}
	while :; do
		echo "There are multiple devices. Please type the index (1-$NSERIALS) of the one you want"
		COUNT=0
		while read LINE; do
			echo -n "$((COUNT+1)): "
			echo ${SERIALS[$COUNT]}
			COUNT=$((COUNT+1))
		done <<< "$(adb devices | grep 'device$')"
		read TYPED
		if [ $TYPED -gt 0 -a $TYPED -le $COUNT ]; then
			SERIAL="-s ${SERIALS[$((TYPED-1))]}"
			break
		fi
		echo "$TYPED is not between 1 and $NSERIALS"
	done
}

[ $# -ge 1 ] && usage "unexpected parameter: $1"

listSerials

COUNT=${#SERIALS[@]}
case $COUNT in
	0)
		usage "no devices found"
		;;
	1)
		SERIAL="-s ${SERIALS[0]}"
		;;
	*)
		setSerial
		;;
esac

$ADB $SERIAL logcat | grep -i xw4

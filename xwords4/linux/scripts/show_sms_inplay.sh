#!/bin/sh

for MSG in $(find /tmp/xw_sms/ -type f); do
	NOLOCK=$(echo $MSG | sed 's,.lock,,')
	if [ $NOLOCK = $MSG ]; then
		basename $(dirname "$MSG")
		echo -n "${MSG}: "
		head -n 1 $MSG
	fi
done
# ls -l /tmp/xw_sms/*

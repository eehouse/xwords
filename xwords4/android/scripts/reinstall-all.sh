#!/bin/sh

WD=$(dirname $0)

$WD/uninstall.sh $*
$WD/adb-install.sh $*

#!/bin/sh

WD=$(dirname $0)

$WD/uninstall.sh $*
$WD/install-all.sh $*
